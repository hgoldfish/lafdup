#include <QtCore/qdatastream.h>
#include <QtCore/qloggingcategory.h>
#include "discovery.h"
#include "peer.h"

static Q_LOGGING_CATEGORY(logger, "lafdup.discovery") using namespace qtng;
const quint16 DefaultPort = 7951;
const quint16 MagicNumber = DefaultPort;
const quint8 CurrentVersion = 1;

LafdupKcpSocket::LafdupKcpSocket(LafdupDiscovery *parent)
    : KcpSocket(HostAddress::IPv4Protocol)
    , parent(parent)
{
}

bool LafdupKcpSocket::filter(char *data, qint32 *len, HostAddress *addr, quint16 *port)
{
    const QByteArray &packet = QByteArray::fromRawData(data, *len);
    if (packet.startsWith("\x1f\x0f")) {  // MagicCode
        parent->handleDiscoveryRequest(packet, *addr, *port);
        return true;
    }
    if (packet.startsWith("\xcd\x1f\x0f")) {  // packet previous version sent.
        return true;
    }
    return false;
}

LafdupDiscovery::LafdupDiscovery(const QByteArray &uuid, quint16 port, LafdupPeer *parent)
    : operations(new CoroutineGroup())
    , uuid(uuid)
    , parent(parent)
    , port(port)
{
    kcpSocket.reset(new LafdupKcpSocket(this));
    kcpSocket->setOption(Socket::BroadcastSocketOption, true);
}

LafdupDiscovery::~LafdupDiscovery()
{
    delete operations;
}

bool LafdupDiscovery::start()
{
    if (operations->has("serve")) {
        if (Q_UNLIKELY(kcpSocket->state() != Socket::ListeningState)) {
            qCWarning(logger) << "invalid peer state, kcp socket is dead.";
        }
        if (Q_UNLIKELY(!operations->has("discovery"))) {
            qCWarning(logger) << "invalid peer state, discovery coroutine is dead.";
        }
        return true;
    }

    if (!kcpSocket->bind(port)) {
        return false;
    }
    kcpSocket->listen(50);

    operations->spawnWithName("serve", [this] { serve(); });
    operations->spawnWithName("discovery", [this] { discovery(); });
    return true;
}

void LafdupDiscovery::stop()
{
    operations->killall();
    kcpSocket.reset(new LafdupKcpSocket(this));
}

void LafdupDiscovery::setExtraKnownPeers(const QSet<QPair<HostAddress, quint16>> &extraKnownPeers)
{
    this->extraKnownPeers = extraKnownPeers;
}

QSet<QPair<HostAddress, quint16>> LafdupDiscovery::getExtraKnownPeers()
{
    return extraKnownPeers;
}

QSet<HostAddress> getMyIPs()
{
    QSet<HostAddress> addresses;
    for (const HostAddress &addr : NetworkInterface::allAddresses()) {
        addresses.insert(addr);
    }
    return addresses;
}

QStringList LafdupDiscovery::getAllBoundAddresses()
{
    QStringList addresses;
    for (const HostAddress &addr : NetworkInterface::allAddresses()) {
        if (!addr.isLoopback() && !addr.isMulticast() && addr.protocol() == HostAddress::IPv4Protocol) {
            addresses.append(addr.toString());
        }
    }
    return addresses;
}

quint16 LafdupDiscovery::getPort()
{
    if (port == 0) {
        return kcpSocket->localPort();
    } else {
        return port;
    }
}

quint16 LafdupDiscovery::getDefaultPort()
{
    return DefaultPort;
}

QByteArray LafdupDiscovery::getUuid()
{
    return uuid;
}

void LafdupDiscovery::serve()
{
    while (true) {
        QSharedPointer<KcpSocket> request(kcpSocket->accept());
        if (request.isNull()) {
            return;
        }
        parent->tryToConnectPeer(request);
    }
}

void LafdupDiscovery::handleDiscoveryRequest(const QByteArray &packet, HostAddress addr, quint16 port)
{
    quint16 magicNumber;
    quint8 version;
    quint32 len;
    QByteArray uuid;

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
    uuid.resize(static_cast<int>(len));

    ds.readRawData(uuid.data(), static_cast<int>(len));
    if (ds.status() != QDataStream::Ok) {
        qCInfo(logger) << "got invalid discovery packet.";
        return;
    }

    if (uuid.isEmpty()) {
        qCInfo(logger) << "got datagram with empty uuid.";
        return;
    }

    if (uuid == this->uuid) {
        return;
    }
    const QString &peerName = QString::fromUtf8(uuid);
    knownPeers.insert(peerName, qMakePair(addr, port));
    if (parent->hasPeer(peerName)) {
        return;
    }
    if (parent->hasPeer(addr, port)) {
        return;
    }

    parent->tryToConnectPeer(peerName, addr, port);
}

static QSet<HostAddress> allBroadcastAddresses()
{
    QSet<HostAddress> addresses;
    addresses.insert(HostAddress::Broadcast);
    for (const NetworkInterface &interface : NetworkInterface::allInterfaces()) {
        for (const NetworkAddressEntry &entry : interface.addressEntries()) {
            const HostAddress &addr = entry.broadcast();
            if (!addr.isNull()) {
                addresses.insert(addr);
            }
        }
    }
    return addresses;
}

void LafdupDiscovery::discovery()
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
                qCDebug(logger) << "send to broadcast address: " << addr.toString();
            }
        }
        // prevent undefined behavior if addresses changed while broadcasting.
        QHash<QString, QPair<HostAddress, quint16>> addresses = this->knownPeers;
        for (const QString &peerName : addresses.keys()) {
            const QPair<HostAddress, quint16> &addr = addresses.value(peerName);
            if (parent->hasPeer(peerName)) {
                continue;
            }
            if (parent->hasPeer(addr.first, addr.second)) {
                continue;
            }
            qint32 bs = kcpSocket->udpSend(packet, addr.first, addr.second);
            if (bs != packet.size()) {
                qCDebug(logger) << "can not send packet to" << addr << kcpSocket->errorString();
            } else {
                qCDebug(logger) << "send to known peer: " << addr.first.toString() << ":" << addr.second;
            }
        }

        // prevent undefined behavior if addresses changed while broadcasting.
        QSet<QPair<HostAddress, quint16>> extraKnownPeers = this->extraKnownPeers;
        for (const QPair<HostAddress, quint16> &extraKnownPeer : extraKnownPeers) {
            if (knownPeers.values().contains(extraKnownPeer)) {
                continue;
            }
            if (parent->hasPeer(extraKnownPeer.first, extraKnownPeer.second)) {
                continue;
            }
            qint32 bs = kcpSocket->udpSend(packet, extraKnownPeer.first, extraKnownPeer.second);
            if (bs != packet.size()) {
                qCDebug(logger) << "can not send packet to" << extraKnownPeer.first.toString() << ":"
                                << extraKnownPeer.second;
            } else {
                qCDebug(logger) << "send to extra known peer: " << extraKnownPeer.first.toString() << ":"
                                << extraKnownPeer.second;
            }
        }
        Coroutine::sleep(5.0);
    }
}
