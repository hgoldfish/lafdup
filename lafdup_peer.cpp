#include <QtNetwork/qnetworkinterface.h>
#include "lafdup_peer.h"

using namespace qtng;


const quint16 DefaultPort = 7951;


class LafdupPeerPrivate
{
public:
    LafdupPeerPrivate(LafdupPeer *q);
    ~LafdupPeerPrivate();
public:
    bool start();
    void stop();
    void serve();
    void handleRequest(const QByteArray &packet);
    bool send(const QString &text);
    void broadcast(const QByteArray &packet);
public:
    QSharedPointer<Socket> socket;
    CoroutineGroup *operations;
    QSharedPointer<Cipher> cipher;
    QList<QHostAddress> knownPeers;
    QSet<QByteArray> oldHashes;
private:
    LafdupPeer * const q_ptr;
    Q_DECLARE_PUBLIC(LafdupPeer)
};


LafdupPeerPrivate::LafdupPeerPrivate(LafdupPeer *q)
    : socket(new Socket(Socket::IPv4Protocol, Socket::UdpSocket))
    , operations(new CoroutineGroup)
    , q_ptr(q)
{
    socket->setOption(Socket::BroadcastSocketOption, true);
}


LafdupPeerPrivate::~LafdupPeerPrivate()
{
    socket->close();
    delete operations;
}


bool LafdupPeerPrivate::start()
{
    if (!socket->bind(DefaultPort))
        return false;

    QSharedPointer<Coroutine> t = operations->spawnWithName("serve", [this] { serve(); });
    return !t.isNull();
}


void LafdupPeerPrivate::stop()
{
    operations->killall();
}


void LafdupPeerPrivate::serve()
{
    Q_Q(LafdupPeer);
    QHostAddress addr;
    quint16 port;
    QByteArray buf(1024 * 64, Qt::Uninitialized);
    QPointer<LafdupPeer> self(q);
    while (true) {
        qint32 bs = socket->recvfrom(buf.data(), buf.size(), &addr, &port);
        if (bs < 0) {
            callInEventLoopAsync([self] {
                if (self.isNull())
                    return;
                emit self->stateChanged(false);
            });
            return;
        } else if (bs == 0) {
            continue;
        }
        handleRequest(QByteArray::fromRawData(buf.data(), bs));
    }
}


void LafdupPeerPrivate::handleRequest(const QByteArray &packet)
{
    Q_Q(LafdupPeer);
    quint16 magicCode;
    quint8 version;

    MsgPackStream mps(packet);
    mps >> magicCode >> version;
    if (magicCode != DefaultPort || version != 1 || mps.status() != MsgPackStream::Ok) {
        qDebug() << "invalid packet.";
        return;
    }

    QDateTime timestamp;
    QByteArray encrypted;
    QByteArray itsHash;
    mps >> timestamp >> itsHash >> encrypted;
    if (encrypted.isEmpty() || itsHash.isEmpty() || mps.status() != MsgPackStream::Ok || !timestamp.isValid()) {
        qDebug() << "payload is invalid.";
        return;
    }

    if (oldHashes.contains(itsHash)) {
        return;
    }
    oldHashes.insert(itsHash);

    QByteArray plain;
    if (!cipher.isNull()) {
        QSharedPointer<Cipher> incomingCipher(cipher->copy(Cipher::Decrypt));
        plain = incomingCipher->addData(encrypted);
        plain.append(incomingCipher->finalData());
    } else {
        plain = encrypted;
    }

    const QByteArray &myHash = MessageDigest::digest(plain, MessageDigest::Sha256);
    if (itsHash != myHash) {
        qDebug() << "hash is mismatch. may be not my packet.";
        return;
    }

    QString text = QString::fromUtf8(plain);
    if (text.isEmpty()) {
        qDebug() << "text is empty.";
        return;
    }


    operations->kill("broadcast");

    QPointer<LafdupPeer> self(q);
    callInEventLoopAsync([self, timestamp, text] {
        if (self.isNull()) {
            return;
        }
        emit self->incoming(timestamp, text);
    });
}


bool LafdupPeerPrivate::send(const QString &text)
{
    if (text.isEmpty()) {
        return false;
    }

    QByteArray packet;
    MsgPackStream mps(&packet, QIODevice::WriteOnly);

    const QByteArray &plain = text.toUtf8();
    const QByteArray &hash = MessageDigest::digest(plain, MessageDigest::Sha256);

    QByteArray encrypted;
    if (cipher.isNull()) {
        encrypted = plain;
    } else {
        QSharedPointer<Cipher> outgoingCipher(this->cipher->copy(Cipher::Encrypt));
        encrypted = outgoingCipher->addData(plain);
        encrypted.append(outgoingCipher->finalData());
        qDebug() << encrypted.size();
    }

    if (hash.isEmpty() || encrypted.isEmpty()) {
        qDebug() << "cipher is bad.";
        return false;
    }

    mps << DefaultPort << (quint8) 1 << QDateTime::currentDateTime() << hash << encrypted;
    if (mps.status() != MsgPackStream::Ok) {
        qDebug() << "can not serialize packet.";
        return false;
    }

    if (packet.size() >= 1024 * 64) {
        return false;
    }

    oldHashes.insert(hash);
    operations->spawnWithName("broadcast", [this, packet] { broadcast(packet); }, true);
    return true;
}


static QSet<QHostAddress> allBroadcastAddresses()
{
    QSet<QHostAddress> addresses;
    addresses.insert(QHostAddress::Broadcast);
    for (const QNetworkInterface &interface: QNetworkInterface::allInterfaces()) {
        for (const QNetworkAddressEntry &entry: interface.addressEntries()) {
            const QHostAddress &addr = entry.broadcast();
            if (!addr.isNull()) {
                addresses.insert(addr);
            }
        }
    }
    return addresses;
}

void LafdupPeerPrivate::broadcast(const QByteArray &packet)
{
    while (true) {
        const QSet<QHostAddress> &broadcastList = allBroadcastAddresses();
        for (const QHostAddress &addr: broadcastList) {
            qint32 bs = socket->sendto(packet, addr, DefaultPort);
            if (bs != packet.size()) {
                qDebug() << "can not send packet to" << addr;
            } else {
                qDebug() << "send to" << addr;
            }
        }
        // prevent undefined behavior if addresses changed while broadcasting.
        QList<QHostAddress> addresses = this->knownPeers;
        for (const QHostAddress &addr: addresses) {
            qint32 bs = socket->sendto(packet, addr, DefaultPort);
            if (bs != packet.size()) {
                qDebug() << "can not send packet to" << addr;
            } else {
                qDebug() << "send to" << addr;
            }
        }
        Coroutine::sleep(1.0);
    }
}


LafdupPeer::LafdupPeer()
    :dd_ptr(new LafdupPeerPrivate(this))
{}


LafdupPeer::~LafdupPeer()
{
    delete dd_ptr;
}


bool LafdupPeer::start()
{
    Q_D(LafdupPeer);
    return d->start();
}


void LafdupPeer::stop()
{
    Q_D(LafdupPeer);
    d->stop();
}


void LafdupPeer::setCipher(QSharedPointer<Cipher> cipher)
{
    Q_D(LafdupPeer);
    d->cipher = cipher;
}


QSharedPointer<Cipher> LafdupPeer::cipher() const
{
    Q_D(const LafdupPeer);
    return d->cipher;
}


void LafdupPeer::setKnownPeers(const QList<QHostAddress> &knownPeers)
{
    Q_D(LafdupPeer);
    d->knownPeers = knownPeers;
}


bool LafdupPeer::outgoing(const QString &text)
{
    Q_D(LafdupPeer);
    return d->send(text);
}


QStringList LafdupPeer::getAllBoundAddresses()
{
    QStringList addresses;
    for (const QHostAddress &addr: QNetworkInterface::allAddresses()) {
        if (!addr.isLoopback() && !addr.isMulticast() && addr.protocol() == QAbstractSocket::IPv4Protocol) {
            addresses.append(addr.toString());
        }
    }
    return addresses;
}

quint16 LafdupPeer::getPort()
{
    return DefaultPort;
}
