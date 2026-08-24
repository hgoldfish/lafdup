#include <QtCore/qdatastream.h>
#include <QtCore/qloggingcategory.h>
#include "discovery_p.h"
#include "peer.h"

static Q_LOGGING_CATEGORY(logger, "lafdup.discovery");
using namespace qtng;
const quint16 DefaultPort = 7951;
const quint16 MagicNumber = DefaultPort;
const quint8 CurrentVersion = 1;

LafdupDiscoveryPrivate::LafdupDiscoveryPrivate(LafdupDiscovery *q, const QByteArray &uuid, quint16 port,
                                             LafdupPeer *peer)
    : operations(new CoroutineGroup())
    , uuid(uuid)
    , local(peer)
    , port(port)
    , q_ptr(q)
{
}

LafdupDiscoveryPrivate::~LafdupDiscoveryPrivate()
{
    delete operations;
}

void LafdupDiscoveryPrivate::serveUdp()
{
    while (true) {
        QSharedPointer<KcpSocket> request(kcpSocket->accept());
        if (request.isNull()) {
            return;
        }
        local->tryToConnectPeer(request);
    }
}

void LafdupDiscoveryPrivate::serveTcp()
{
    while (true) {
        QSharedPointer<Socket> request(tcpServer->accept());
        if (request.isNull()) {
            return;
        }
        local->tryToConnectPeer(request);
    }
}

void LafdupDiscoveryPrivate::handleDiscoveryRequest(const QByteArray &packet, HostAddress addr, quint16 port)
{
    quint16 magicNumber;
    quint8 version;
    quint32 len;
    QByteArray peerUuid;

    QDataStream ds(packet);
    ds >> magicNumber >> version >> len;
    if (ds.status() != QDataStream::Ok) {
        qCInfo(logger) << "got invalid discovery packet.";
        return;
    }
    if (magicNumber != MagicNumber) {
        qCInfo(logger) << "got datagram with bad magic number: " << magicNumber;
        return;
    }
    if (version != CurrentVersion) {
        qCInfo(logger) << "version" << version << "is unknown.";
        return;
    }
    if (len > 64) {
        qCInfo(logger) << "got datagram with bad uuid length: " << len;
        return;
    }
    peerUuid.resize(static_cast<int>(len));

    ds.readRawData(peerUuid.data(), static_cast<int>(len));
    if (ds.status() != QDataStream::Ok) {
        qCInfo(logger) << "got invalid discovery packet.";
        return;
    }

    if (peerUuid.isEmpty()) {
        qCInfo(logger) << "got datagram with empty uuid.";
        return;
    }

    if (peerUuid == uuid) {
        return;
    }
    const QString &peerName = QString::fromUtf8(peerUuid);
    knownPeers.insert(peerName, qMakePair(addr, port));
    if (local->hasPeer(peerName)) {
        return;
    }
    if (local->hasPeer(addr, port)) {
        return;
    }

    local->tryToConnectPeer(peerName, addr, port);
}

static QSet<HostAddress> allBroadcastAddresses()
{
    QSet<HostAddress> addresses;
    addresses.insert(HostAddress::Broadcast);
    const auto &listall = NetworkInterface::allInterfaces();
    for (const NetworkInterface &interface : listall) {
        const auto &listadd = interface.addressEntries();
        for (const NetworkAddressEntry &entry : listadd) {
            const HostAddress &addr = entry.broadcast();
            if (!addr.isNull()) {
                addresses.insert(addr);
            }
        }
    }
    return addresses;
}

void LafdupDiscoveryPrivate::discovery()
{
    QByteArray packet;
    QDataStream ds(&packet, QIODevice::WriteOnly);
    ds << DefaultPort << CurrentVersion << uuid;
    if (ds.status() != QDataStream::Ok) {
        qCCritical(logger) << "can not make discovery packet.";
        return;
    }
    while (true) {
        const QSet<HostAddress> &broadcastList = allBroadcastAddresses();
        for (const HostAddress &addr : broadcastList) {
            qint32 bs = kcpSocket->udpSend(packet, addr, DefaultPort);
            if (bs != packet.size()) {
                qCDebug(logger) << "can not send packet to" << addr << kcpSocket->errorString();
            } else {
                // qCDebug(logger) << "send to broadcast address: " << addr.toString();
            }
        }
        // prevent undefined behavior if addresses changed while broadcasting.
        QHash<QString, QPair<HostAddress, quint16>> addresses = knownPeers;
        const auto &list = addresses.keys();
        for (const QString &peerName : list) {
            const QPair<HostAddress, quint16> &addr = addresses.value(peerName);
            if (local->hasPeer(peerName)) {
                continue;
            }
            if (local->hasPeer(addr.first, addr.second)) {
                continue;
            }
            qint32 bs = kcpSocket->udpSend(packet, addr.first, addr.second);
            if (bs != packet.size()) {
                qCDebug(logger) << "can not send packet to" << addr << kcpSocket->errorString();
            } else {
                //qCDebug(logger) << "send to known peer: " << addr.first.toString() << ":" << addr.second;
            }
        }

        // prevent undefined behavior if addresses changed while broadcasting.
        for (const QPair<HostAddress, quint16> &extraKnownPeer : qAsConst(extraKnownPeers)) {
            bool found = false;
            for (const auto &value : qAsConst(knownPeers)) {
                if (value == extraKnownPeer) {
                    found = true;
                    break;
                }
            }
            if (found) {
                continue;
            }
            if (local->hasPeer(extraKnownPeer.first, extraKnownPeer.second)) {
                continue;
            }
            qint32 bs = kcpSocket->udpSend(packet, extraKnownPeer.first, extraKnownPeer.second);
            if (bs != packet.size()) {
                qCDebug(logger) << "can not send packet to" << extraKnownPeer.first.toString() << ":"
                                << extraKnownPeer.second;
            } else {
                //qCDebug(logger) << "send to extra known peer: " << extraKnownPeer.first.toString() << ":"
                //                << extraKnownPeer.second;
            }
        }
        Coroutine::sleep(5.0);
    }
}

LafdupKcpSocket::LafdupKcpSocket(LafdupDiscoveryPrivate *d)
    : KcpSocket(HostAddress::IPv4Protocol)
    , d(d)
{
}

bool LafdupKcpSocket::filter(char *data, qint32 *len, HostAddress *addr, quint16 *port)
{
    const QByteArray &packet = QByteArray::fromRawData(data, *len);
    if (packet.startsWith("\x1f\x0f")) {  // MagicCode
        d->handleDiscoveryRequest(packet, *addr, *port);
        return true;
    }
    if (packet.startsWith("\xcd\x1f\x0f")) {  // packet previous version sent.
        return true;
    }
    return false;
}

LafdupDiscovery::LafdupDiscovery(const QByteArray &uuid, quint16 port, LafdupPeer *parent)
    : d_ptr(new LafdupDiscoveryPrivate(this, uuid, port, parent))
{
    stop();
}

LafdupDiscovery::~LafdupDiscovery()
{
    stop();
}

bool LafdupDiscovery::start()
{
    Q_D(LafdupDiscovery);
    if (d->operations->has("serve_udp")) {
        if (Q_UNLIKELY(d->kcpSocket->state() != Socket::ListeningState)) {
            qCWarning(logger) << "invalid peer state, kcp socket is dead.";
        }
        if (Q_UNLIKELY(!d->operations->has("discovery"))) {
            qCWarning(logger) << "invalid peer state, discovery coroutine is dead.";
        }
        if (Q_UNLIKELY(!d->operations->has("serve_tcp"))) {
            qCWarning(logger) << "invalid peer state, tcp serveUdp coroutine is dead.";
        }
        return true;
    }

    if (!d->kcpSocket->bind(d->port, static_cast<Socket::BindMode>(Socket::ReuseAddressHint))) {
        qCWarning(logger) << "can not bind kcp server on port" << d->port;
        return false;
    }
    if (!d->kcpSocket->listen(50)) {
        qCWarning(logger) << "can not listen kcp server on port" << d->port;
        return false;
    }

    if (!d->tcpServer->bind(d->port, static_cast<Socket::BindMode>(Socket::ReuseAddressHint))) {
        qCWarning(logger) << "can not bind tcp server on port" << d->port;
        return false;
    }
    if (!d->tcpServer->listen(50)) {
        qCWarning(logger) << "can not listen tcp server on port" << d->port;
        return false;
    }

    d->operations->spawnWithName("serve_udp", [d] { d->serveUdp(); });
    d->operations->spawnWithName("serve_tcp", [d] { d->serveTcp(); });
    d->operations->spawnWithName("discovery", [d] { d->discovery(); });
    return true;
}

void LafdupDiscovery::stop()
{
    Q_D(LafdupDiscovery);
    d->operations->killall();
    d->kcpSocket.reset(new LafdupKcpSocket(d));
    d->kcpSocket->setOption(Socket::BroadcastSocketOption, true);
    d->tcpServer.reset(new Socket(HostAddress::IPv4Protocol));
}

void LafdupDiscovery::setExtraKnownPeers(const QSet<QPair<HostAddress, quint16>> &extraKnownPeers)
{
    Q_D(LafdupDiscovery);
    d->extraKnownPeers = extraKnownPeers;
}

QSet<QPair<HostAddress, quint16>> LafdupDiscovery::extraKnownPeers() const
{
    Q_D(const LafdupDiscovery);
    return d->extraKnownPeers;
}

QStringList LafdupDiscovery::allBoundAddresses()
{
    QStringList addresses;
    const auto &list = NetworkInterface::allAddresses();
    for (const HostAddress &addr : list) {
        if (!addr.isLoopback() && !addr.isMulticast() && addr.protocol() == HostAddress::IPv4Protocol) {
            addresses.append(addr.toString());
        }
    }
    return addresses;
}

quint16 LafdupDiscovery::port()
{
    Q_D(LafdupDiscovery);
    if (d->port == 0) {
        return d->kcpSocket->localPort();
    }
    return d->port;
}

quint16 LafdupDiscovery::defaultPort()
{
    return DefaultPort;
}

QByteArray LafdupDiscovery::uuid()
{
    Q_D(LafdupDiscovery);
    return d->uuid;
}

