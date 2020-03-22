#include <QtCore/qdatetime.h>
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
    void discovery();
    void handleRequest(const QByteArray &packet, const QHostAddress &addr, quint16 port);
    void parseAndHandleDataPacket(MsgPackStream &mps, const QHostAddress &addr, quint16 port);
    void parseAndHandleDetectivePacket(const QHostAddress &addr, quint16 port);
    bool outgoing(const QDateTime &timestamp, const QString &text);
    void broadcast(const QByteArray &plain, const QByteArray &hash, const QDateTime &timestamp);
    void sendPacket(const QByteArray &packet);
    QByteArray makeDataPacketVersion1(QSharedPointer<Cipher> lastCipher,
                                      const QByteArray &plain, const QByteArray &hash, const QDateTime &timestamp);
    QByteArray makeDataPacketVersion2(QSharedPointer<Cipher> lastCipher,
                                      const QByteArray &plain, const QByteArray &hash, const QDateTime &timestamp);
    QByteArray makeDetectivePacket();
public:
    QSharedPointer<Socket> socket;
    CoroutineGroup *operations;
    QSharedPointer<Cipher> cipher;
    QList<QHostAddress> knownPeers;
    QSet<QPair<QHostAddress, quint16>> extraKnownPeers;
    QList<QByteArray> oldHashes;
    quint32 myId;
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
    qsrand(static_cast<uint>(QDateTime::currentMSecsSinceEpoch()));
    myId = static_cast<quint32>(qrand()) & 0xfffffff0;
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

    operations->spawnWithName("serve", [this] { serve(); });
    operations->spawnWithName("discovery", [this] { discovery(); });
    return true;
}


void LafdupPeerPrivate::stop()
{
    operations->killall();
    socket.reset(new Socket(Socket::IPv4Protocol, Socket::UdpSocket));
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
        handleRequest(QByteArray::fromRawData(buf.data(), bs), addr, port);
    }
}


inline QByteArray concat(const QDateTime &timestamp, const QByteArray &hash)
{
    return timestamp.toString().toUtf8() + hash;
}


void LafdupPeerPrivate::handleRequest(const QByteArray &packet, const QHostAddress &addr, quint16 port)
{
    quint16 magicCode;
    quint8 version;

    MsgPackStream mps(packet);
    mps >> magicCode >> version;
    if (magicCode != DefaultPort || (version != 1 && version != 2)|| mps.status() != MsgPackStream::Ok) {
        qDebug() << "invalid packet.";
        return;
    }

    if (version == 1) {
        parseAndHandleDataPacket(mps, addr, port);
    } else if (version == 2) {
        quint32 t;
        mps >> t;
        if (mps.status() != MsgPackStream::Ok) {
            qDebug() << "invalid packet.";
            return;
        }
        quint32 itsId = t & 0xfffffff0;
        quint32 command = t & 0xf;
        if (itsId == myId) {
            return;
        }

        if (command == 1) {
            parseAndHandleDataPacket(mps, addr, port);
        } else if (command == 2) {
            parseAndHandleDetectivePacket(addr, port);
        }
    }

}


void LafdupPeerPrivate::parseAndHandleDataPacket(MsgPackStream &mps,  const QHostAddress &addr, quint16 port) {
    Q_Q(LafdupPeer);
    QDateTime timestamp;
    QByteArray encrypted;
    QByteArray itsHash;
    mps >> timestamp >> itsHash >> encrypted;
    if (encrypted.isEmpty() || itsHash.isEmpty() || mps.status() != MsgPackStream::Ok || !timestamp.isValid()) {
        qDebug() << "payload is invalid.";
        return;
    }


    if (!oldHashes.isEmpty() && oldHashes.contains(concat(timestamp, itsHash))) {
        return;
    }
    oldHashes.prepend(itsHash);

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

    extraKnownPeers.insert(qMakePair(addr, port));
    operations->kill("broadcast");

    QPointer<LafdupPeer> self(q);
    callInEventLoopAsync([self, timestamp, text] {
        if (self.isNull()) {
            return;
        }
        emit self->incoming(timestamp, text);
    });
}


void LafdupPeerPrivate::parseAndHandleDetectivePacket(const QHostAddress &addr, quint16 port)
{
    extraKnownPeers.insert(qMakePair(addr, port));
}


bool LafdupPeerPrivate::outgoing(const QDateTime &timestamp, const QString &text)
{
    if (text.isEmpty()) {
        return false;
    }

    const QByteArray &plain = text.toUtf8();
    const QByteArray &hash = MessageDigest::digest(plain, MessageDigest::Sha256);
    if (hash.isEmpty()) {
        qDebug() << "cipher is bad.";
        return false;
    }
    oldHashes.prepend(concat(timestamp, hash));
    operations->spawnWithName("broadcast", [this, plain, hash, timestamp] {
        broadcast(plain, hash, timestamp);
    }, true);
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


QByteArray LafdupPeerPrivate::makeDataPacketVersion1(QSharedPointer<Cipher> lastCipher,
                                                     const QByteArray &plain, const QByteArray &hash,
                                                     const QDateTime &timestamp)
{
    QByteArray packet;
    QByteArray encrypted;
    if (lastCipher.isNull()) {
        encrypted = plain;
    } else {
        QScopedPointer<Cipher> outgoingCipher(lastCipher->copy(Cipher::Encrypt));
        encrypted = outgoingCipher->addData(plain);
        encrypted.append(outgoingCipher->finalData());
        if (encrypted.isEmpty()) {
            qDebug() << "cipher is bad.";
            return QByteArray();
        }
    }

    MsgPackStream mps(&packet, QIODevice::WriteOnly);
    mps << DefaultPort << static_cast<quint8>(1) << timestamp << hash << encrypted;
    if (mps.status() != MsgPackStream::Ok) {
        qDebug() << "can not serialize packet.";
        return QByteArray();
    }
    return packet;
}


QByteArray LafdupPeerPrivate::makeDataPacketVersion2(QSharedPointer<Cipher> lastCipher,
                                                     const QByteArray &plain, const QByteArray &hash,
                                                     const QDateTime &timestamp)
{
    QByteArray packet;
    QByteArray encrypted;
    if (lastCipher.isNull()) {
        encrypted = plain;
    } else {
        QScopedPointer<Cipher> outgoingCipher(lastCipher->copy(Cipher::Encrypt));
        encrypted = outgoingCipher->addData(plain);
        encrypted.append(outgoingCipher->finalData());
        if (encrypted.isEmpty()) {
            qDebug() << "cipher is bad.";
            return QByteArray();
        }
    }

    MsgPackStream mps(&packet, QIODevice::WriteOnly);
    quint32 header = myId | 1;
    mps << DefaultPort << static_cast<quint8>(2) << header << timestamp << hash << encrypted;
    if (mps.status() != MsgPackStream::Ok) {
        qDebug() << "can not serialize packet.";
        return QByteArray();
    }
    return packet;
}


QByteArray LafdupPeerPrivate::makeDetectivePacket()
{
    QByteArray packet;
    MsgPackStream mps(&packet, QIODevice::WriteOnly);
    quint32 header = myId | 2;
    mps << DefaultPort << static_cast<quint8>(2) << header;
    if (mps.status() != MsgPackStream::Ok) {
        qDebug() << "can not serialize packet.";
        return QByteArray();
    }
    return packet;
}


void LafdupPeerPrivate::broadcast(const QByteArray &plain, const QByteArray &hash, const QDateTime &timestamp)
{
    QSharedPointer<Cipher> lastCipher = cipher;
    QByteArray dataPacketVersion1;
    QByteArray dataPacketVersion2;

    while (true) {
        bool chiperChanged = lastCipher != cipher;
        if (chiperChanged) {
            lastCipher = cipher;
        }

        if (dataPacketVersion1.isEmpty() || chiperChanged) {
            dataPacketVersion1 = makeDataPacketVersion1(lastCipher, plain, hash, timestamp);
            if (dataPacketVersion1.isEmpty()) {
                return;
            }
            if (dataPacketVersion1.size() >= 1024 * 64) {
                qDebug() << "packet is too large to send.";
                return;
            }
        }
        if (dataPacketVersion2.isEmpty() || chiperChanged) {
            dataPacketVersion2 = makeDataPacketVersion2(lastCipher, plain, hash, timestamp);
            if (dataPacketVersion2.isEmpty()) {
                return;
            }
            if (dataPacketVersion2.size() >= 1024 * 64) {
                qDebug() << "packet is too large to send.";
                return;
            }
        }
        sendPacket(dataPacketVersion1);
        sendPacket(dataPacketVersion2);
        Coroutine::sleep(1.0f);
    }
}


void LafdupPeerPrivate::discovery()
{
    QByteArray detectivePacket = makeDetectivePacket();
    while (true) {
        sendPacket(detectivePacket);
        Coroutine::sleep(5.0);
    }
}


void LafdupPeerPrivate::sendPacket(const QByteArray &packet)
{
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

    // prevent undefined behavior if addresses changed while broadcasting.
    QSet<QPair<QHostAddress, quint16>> extraKnownPeers = this->extraKnownPeers;
    for (const QPair<QHostAddress, quint16> &extraKnownPeer: extraKnownPeers) {
        qint32 bs = socket->sendto(packet, extraKnownPeer.first, extraKnownPeer.second);
        if (bs != packet.size()) {
            qDebug() << "can not send packet to" << extraKnownPeer.first.toString() << ":" << extraKnownPeer.second;
        } else {
            qDebug() << "send to" << extraKnownPeer.first.toString() << ":" << extraKnownPeer.second;
        }
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


bool LafdupPeer::outgoing(const QDateTime &timestamp, const QString &text)
{
    Q_D(LafdupPeer);
    return d->outgoing(timestamp, text);
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
