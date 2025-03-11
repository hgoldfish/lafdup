#include <QtCore/qloggingcategory.h>
#include <QtCore/qsettings.h>
#include <QtCore/qscopeguard.h>
#include "discovery.h"
#include "peer_p.h"

static Q_LOGGING_CATEGORY(logger, "lafdup.peer") using namespace qtng;
using namespace lafrpc;

LafdupRemoteStub::LafdupRemoteStub(LafdupPeer *parent)
    : parent(parent)
{
}

bool LafdupRemoteStub::pasteText(const QDateTime &timestamp, const QString &text)
{
    if (!timestamp.isValid() || text.isEmpty()) {
        throw RpcRemoteException(tr("The time is wrong or the content is empty"));
    }
    if (parent->findItem(timestamp)) {
        throw RpcRemoteException(tr("The same content is sent repeatedly"));
    }
    CopyPaste item;
    item.direction = CopyPaste::Incoming;
    item.timestamp = timestamp;
    item.mimeType = TextType;
    item.text = text;
    parent->items.prepend(item);
    QPointer<LafdupPeer> peer(parent);
    callInEventLoopAsync([peer, item] {
        if (!peer.isNull()) {
            emit peer->incoming(item);
        }
    });
    return true;
}

bool LafdupRemoteStub::pasteCompText(const QDateTime &timestamp, const QString &text)
{
    if (!timestamp.isValid() || text.isEmpty()) {
        throw RpcRemoteException(tr("The time is wrong or the content is empty"));
    }
    if (parent->findItem(timestamp)) {
        throw RpcRemoteException(tr("The same content is sent repeatedly"));
    }
    PasteHashKey key(parent->rpc->getCurrentPeer()->name(), timestamp);
    CopyPaste item;
    item = pasteHash[key];
    item.direction = CopyPaste::Incoming;
    item.timestamp = timestamp;
    item.mimeType = CompType;
    item.text = text;
    pasteHash[key] = item;
    return true;
}

bool LafdupRemoteStub::pasteCompImage(const QDateTime &timestamp, QSharedPointer<lafrpc::RpcFile> image)
{
    if (!timestamp.isValid() || image.isNull() || image->name().isEmpty()) {
        throw RpcRemoteException(tr("The local file to send could not be found"));
    }
    QString name = parent->rpc->getCurrentPeer()->name();
    QByteArray imageData;
    bool ok = image->recvall(imageData);
    if (!ok) {
        qDebug() << "can not receive image data.";
        throw RpcRemoteException(tr("Failed to receive the picture"));
    }
    PasteHashKey key(name, timestamp);
    CopyPaste item;
    item = pasteHash[key];
    item.direction = CopyPaste::Incoming;
    item.timestamp = timestamp;
    item.mimeType = CompType;
    item.image = imageData;
    pasteHash[key] = item;
    return true;
}

bool LafdupRemoteStub::pasteCompFiles(const QDateTime &timestamp, QSharedPointer<lafrpc::RpcDir> rpcDir)
{
    if (rpcDir.isNull() || !rpcDir->isValid()) {
        throw RpcRemoteException(tr("The local file to send could not be found"));
    }
    if (parent->cacheDir.isEmpty()) {
        throw RpcRemoteException(tr("The storage path for the other party is empty"));
    }
    QDir cacheDir(parent->cacheDir);
    if (!cacheDir.isReadable()) {
        throw RpcRemoteException(tr("The storage path given by the other party is invalid"));
    }
    const QString &subdir = timestamp.toString("yyyyMMddHHmmss");
    if (!cacheDir.mkdir(subdir)) {
        throw RpcRemoteException(tr("Unable to create a folder on the other side to store files"));
    }
    QDir destDir(cacheDir.filePath(subdir));
    QString name = parent->rpc->getCurrentPeer()->name();
    bool ok = rpcDir->writeToPath(destDir.path());
    if (!ok) {
        throw RpcRemoteException(tr("Failed to save the file on the other party's computer"));
    }
    QStringList fullPaths;
    for (const QString &filePath : static_cast<const QStringList>(
                 destDir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::System))) {
        fullPaths.append(destDir.absoluteFilePath(filePath));
    }
    parent->writeInformation(destDir);
    PasteHashKey key(name, timestamp);
    CopyPaste item = pasteHash[key];
    item.direction = CopyPaste::Incoming;
    item.timestamp = timestamp;
    item.mimeType = CompType;
    item.files = fullPaths;
    pasteHash[key] = item;
    return true;
}

bool LafdupRemoteStub::pasteEnd(const QDateTime &timeStamp)
{
    PasteHashKey key(parent->rpc->getCurrentPeer()->name(), timeStamp);
    CopyPaste item = pasteHash[key];
    if (!parent->findItem(item)) {
        parent->items.prepend(item);
    } else {
        return false;
    }
    QPointer<LafdupPeer> parentRef(parent);
    callInEventLoopAsync([parentRef, item] {
        if (!parentRef.isNull()) {
            emit parentRef->incoming(item);
        }
    });
    return true;
}

bool LafdupRemoteStub::pasteFiles(const QDateTime &timestamp, QSharedPointer<RpcDir> rpcDir)
{
    if (parent->findItem(timestamp) || rpcDir.isNull() || !rpcDir->isValid()) {
        throw RpcRemoteException(tr("The local file to send could not be found"));
    }
    if (parent->cacheDir.isEmpty()) {
        throw RpcRemoteException(tr("The storage path for the other party is empty"));
    }
    QDir cacheDir(parent->cacheDir);
    if (!cacheDir.isReadable()) {
        throw RpcRemoteException(tr("The storage path given by the other party is invalid"));
    }
    const QString &subdir = timestamp.toString("yyyyMMddHHmmss");
    if (!cacheDir.mkdir(subdir)) {
        throw RpcRemoteException(tr("Unable to create a folder on the other side to store files"));
    }
    QDir destDir(cacheDir.filePath(subdir));
    bool ok = rpcDir->writeToPath(destDir.path());
    if (!ok) {
        throw RpcRemoteException(tr("Failed to save the file on the other party's computer"));
    }

    CopyPaste item;
    item.direction = CopyPaste::Incoming;
    item.timestamp = timestamp;
    item.mimeType = BinaryType;
    QStringList fullPaths;
    for (const QString &filePath : static_cast<const QStringList>(
                 destDir.entryList(QDir::NoDotAndDotDot | QDir::Dirs | QDir::Files | QDir::Hidden | QDir::System))) {
        fullPaths.append(destDir.absoluteFilePath(filePath));
    }
    item.files = fullPaths;
    parent->items.prepend(item);
    QPointer<LafdupPeer> parentRef(parent);
    callInEventLoopAsync([parentRef, item] {
        if (!parentRef.isNull()) {
            emit parentRef->incoming(item);
        }
    });
    parent->writeInformation(destDir);
    return true;
}

bool LafdupRemoteStub::pasteImage(const QDateTime &timestamp, QSharedPointer<RpcFile> image)
{
    if (!timestamp.isValid() || image.isNull() || image->name().isEmpty()) {
        throw RpcRemoteException(tr("The local file to send could not be found"));
    }
    if (parent->findItem(timestamp)) {
        throw RpcRemoteException(tr("The same content is sent repeatedly"));
    }
    QByteArray imageData;
    bool ok = image->recvall(imageData);
    if (!ok) {
        qDebug() << "can not receive image data.";
        throw RpcRemoteException(tr("Failed to receive the picture"));
    }
    CopyPaste item;
    item.timestamp = timestamp;
    item.mimeType = ImageType;
    item.image = imageData;
    parent->items.prepend(item);
    QPointer<LafdupPeer> parentRef(parent);
    callInEventLoopAsync([parentRef, item] {
        if (!parentRef.isNull()) {
            emit parentRef->incoming(item);
        }
    });
    return true;
}

bool LafdupRemoteStub::ping()
{
    return true;
}

QDateTime LafdupRemoteStub::getCurrentTime()
{
    return QDateTime::currentDateTime();
}

LafdupPeer::LafdupPeer(const QByteArray &uuid, quint16 port)
    : stub(new LafdupRemoteStub(this))
    , deleteFilesTime(5)
    , sendFilesSize(10.0)
    , ignorePassword(false)
    , cleaningFiles(false)
    , operations(new CoroutineGroup())
{
    lafrpc::registerClass<CopyPaste>();
    rpc = Rpc::builder(MessagePack).myPeerName(uuid).create();
    Q_ASSERT(!rpc.isNull());
    rpc->registerInstance(stub, "lafdup");
    discovery.reset(new LafdupDiscovery(uuid, port, this));
}

LafdupPeer::~LafdupPeer()
{
    delete operations;
}

bool LafdupPeer::start()
{
    if (!discovery->start()) {
        return false;
    }
    const QString &serverAddress = QStringLiteral("tcp://0.0.0.0:%1").arg(discovery->getPort());
    if (!rpc->startServer(serverAddress, false)) {
        return false;
    }
    operations->spawnWithName("clean_files", [this] { cleanFiles(); });
    QPointer<LafdupPeer> self(this);
    callInEventLoopAsync([self] {
        if (!self.isNull()) {
            emit self->stateChanged(true);
        }
    });
    return true;
}

void LafdupPeer::stop()
{
    const QString &serverAddress = QStringLiteral("tcp://0.0.0.0:%1").arg(discovery->getPort());
    rpc->stopServer(serverAddress);
    discovery->stop();
    operations->kill("clean_files");
    QPointer<LafdupPeer> self(this);
    callInEventLoopAsync([self] {
        if (!self.isNull()) {
            emit self->stateChanged(false);
        }
    });
}

struct PopulateResult
{
    PopulateResult()
        : totalSize(0)
    {
    }
    QList<RpcDirFileEntry> entries;
    quint64 totalSize;
};

class VirtualRpcDirFileProvider : public lafrpc::RpcDirFileProvider
{
public:
    virtual ~VirtualRpcDirFileProvider() override;
public:
    virtual QSharedPointer<qtng::FileLike> getFile(const QString &filePath, QIODevice::OpenMode mode) override;
    QString makePath(const QString &filePath);
public:
    void addPath(const QString &filePath);
    void addFileInfo(const QFileInfo &fileInfo);
    PopulateResult populate();
public:
    QList<QFileInfo> fileInfoList;
};

static inline bool lessThan(QSharedPointer<Peer> peer1, QSharedPointer<Peer> peer2)
{
    const QString &address1 = peer1->address();
    const QString &address2 = peer2->address();
    if (address1.startsWith("tcp://") && address2.startsWith("kcp://")) {
        return true;
    } else {
        return false;
    }
}

static bool isPassword(const QString &text)
{
    if (text.size() > 18) {
        return false;
    }
    QString validChars("ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                       "abcdefghijklmnopqrstuvwxyz"
                       "1234567890"
                       "!@#$%^&*()-=_+,./<>?;:'\"[]{}~`\\|");
    for (QChar c : text) {
        if (!validChars.contains(c)) {
            return false;
        }
    }
    return true;
}

void LafdupPeer::_outgoingSync(CopyPaste copyPaste)
{
    if (!canSendContent(copyPaste)) {
        return;
    }
    const QList<QString> peerList = rpc->getAllPeerNames();
    QSet<QString> peerNames(peerList.begin(), peerList.end());
    if (peerNames.isEmpty()) {
        emit sendFeedBack(tr("Failed to send, no one accepted"));
        return;
    }
    emit sendAction();
    QList<QSharedPointer<Coroutine>> coroutines;
    QList<QSharedPointer<QString>> errorStrings;
    for (const QString &peerName : qAsConst(peerNames)) {
        QSharedPointer<Peer> peer = rpc->get(peerName);
        QSharedPointer<QString> errorString(new QString());
        QSharedPointer<Coroutine> c = operations->spawn(
                [this, peer, copyPaste, errorString] { sendContentToPeer(peer, copyPaste, errorString.data()); });
        if (c) {
            coroutines.append(c);
            errorStrings.append(errorString);
        }
    }
    for (QSharedPointer<Coroutine> c : coroutines) {
        c->join();
    }
    QStringList errors;
    for (const QSharedPointer<QString> &errorString : qAsConst(errorStrings)) {
        if (errorString->isEmpty()) {
            continue;
        }
        errors.append(*errorString);
    }
    if (errors.isEmpty()) {
        emit sendFeedBack(tr("Sent successfully"));
        if (!findItem(copyPaste)) {
            items.prepend(copyPaste);
            qDebug() << "incoming";
            emit incoming(copyPaste);
        }
    } else {
        emit sendFeedBack(errors.join("\n"));
    }
}

bool LafdupPeer::canSendContent(const CopyPaste &copyPaste)
{
    if (copyPaste.mimeType == TextType) {
        if (!copyPaste.ignoreLimits && ignorePassword && isPassword(copyPaste.text)) {
            return false;
        }
    } else if (copyPaste.mimeType == BinaryType) {
        if (!copyPaste.ignoreLimits && qFuzzyIsNull(sendFilesSize)) {
            return false;
        }
        QSharedPointer<VirtualRpcDirFileProvider> provider(new VirtualRpcDirFileProvider());
        for (const QString &filePath : copyPaste.files) {
            provider->addPath(filePath);
        }

        PopulateResult populateResult = provider->populate();
        if (!copyPaste.ignoreLimits && populateResult.totalSize >= static_cast<quint64>(sendFilesSize * 1024 * 1024)) {
            return false;
        }
    } else if (copyPaste.mimeType == ImageType) {
        QByteArray imageData = copyPaste.image;
        if (imageData.isEmpty()) {
            return false;
        }
    } else if (copyPaste.mimeType == CompType) {
        if (copyPaste.text.isEmpty() && copyPaste.image.isEmpty() && copyPaste.files.isEmpty()) {
            return false;
        }
        if (!copyPaste.text.isEmpty()) {
            if (!copyPaste.ignoreLimits && ignorePassword && isPassword(copyPaste.text)) {
                return false;
            }
        }
        if (!copyPaste.files.isEmpty()) {
            if (!copyPaste.ignoreLimits && qFuzzyIsNull(sendFilesSize)) {
                return false;
            }
            QSharedPointer<VirtualRpcDirFileProvider> provider(new VirtualRpcDirFileProvider());
            for (const QString &filePath : copyPaste.files) {
                provider->addPath(filePath);
            }

            PopulateResult populateResult = provider->populate();
            if (!copyPaste.ignoreLimits
                && populateResult.totalSize >= static_cast<quint64>(sendFilesSize * 1024 * 1024)) {
                return false;
            }
        }
    } else {
        return false;
    }
    return true;
}

void LafdupPeer::outgoing(const CopyPaste &copyPaste)
{
    if (findItem(copyPaste.timestamp)) {
        return;
    }
    operations->spawn([this, copyPaste]() { _outgoingSync(copyPaste); });
}

QString makeAddress(const QString &prefix, const HostAddress &addr, quint16 port)
{
    if (addr.protocol() == HostAddress::IPv6Protocol) {
        return QStringLiteral("%1://[%2]:%3").arg(prefix, addr.toString()).arg(port);
    } else {
        return QStringLiteral("%1://%2:%3").arg(prefix, addr.toString()).arg(port);
    }
}

bool LafdupPeer::hasPeer(const HostAddress &remoteHost, quint16 port)
{
    const QString &kcpAddress = makeAddress("kcp", remoteHost, port);
    const QString &tcpAddress = makeAddress("tcp", remoteHost, port);
    for (const QSharedPointer<Peer> &peer : static_cast<const QList<QSharedPointer<Peer>>>(rpc->getAllPeers())) {
        if (peer->address() == kcpAddress || peer->address() == tcpAddress) {
            return true;
        }
    }
    return false;
}

bool LafdupPeer::hasPeer(const QString &peerName)
{
    return !rpc->get(peerName).isNull();
}

void LafdupPeer::tryToConnectPeer(QString itsPeerName, HostAddress remoteHost, quint16 port)
{
    operations->spawn([this, itsPeerName, remoteHost, port] {
        for (QSharedPointer<Peer> oldPeer : static_cast<const QList<QSharedPointer<Peer>>>(rpc->getAll(itsPeerName))) {
            if (!oldPeer.isNull()) {
                try {
                    oldPeer->call("lafdup.ping");
                    break;
                } catch (RpcException &) {
                    oldPeer->close();
                }
            }
        }
        const QString &tcpAddress = makeAddress("tcp", remoteHost, port);
        if (connectingPeers.contains(tcpAddress)) {
            return;
        }
        connectingPeers.insert(tcpAddress);

        QSharedPointer<Peer> peer;
        try {
            Timeout timeout(5.0);
            QSharedPointer<Socket> request = QSharedPointer<Socket>(Socket::createConnection(remoteHost, port));
            if (!request.isNull()) {
                peer = handleRequestSync(asSocketLike(request), qtng::PositivePole, itsPeerName, tcpAddress);
                if (!peer.isNull() && peer->name() != itsPeerName) {
                    peer->close();
                    peer.clear();
                }
            }
            connectingPeers.remove(tcpAddress);
        } catch (TimeoutException &) {
            connectingPeers.remove(tcpAddress);
            // pass and go on.
        }

        if (!peer.isNull()) {
            return;
        }

        const QString &kcpAddress = makeAddress("kcp", remoteHost, port);
        if (connectingPeers.contains(kcpAddress)) {
            return;
        }
        connectingPeers.insert(kcpAddress);
        try {
            Timeout timeout(5.0);
            QSharedPointer<KcpSocket> kcpSocket(new KcpSocket(HostAddress::IPv4Protocol));
            kcpSocket->setOption(Socket::BroadcastSocketOption, true);
            if (kcpSocket->connect(remoteHost, port)) {
                handleKcpRequestSync(kcpSocket, PositivePole, itsPeerName);
            }
            connectingPeers.remove(kcpAddress);
        } catch (...) {
            connectingPeers.remove(kcpAddress);
            throw;
        }
    });
}

void LafdupPeer::tryToConnectPeer(QSharedPointer<qtng::KcpSocket> request)
{
    operations->spawn([this, request] { handleKcpRequestSync(request, NegativePole, QString()); });
}

void LafdupPeer::handleKcpRequestSync(QSharedPointer<qtng::KcpSocket> request, DataChannelPole pole,
                                      const QString &itsPeerName)
{
    const QString &address = makeAddress("kcp", request->peerAddress(), request->peerPort());
    request->setSendQueueSize(1024);
    request->setMode(KcpSocket::Ethernet);
    handleRequestSync(asSocketLike(request), pole, itsPeerName, address);
}

QSharedPointer<Peer> LafdupPeer::handleRequestSync(QSharedPointer<qtng::SocketLike> request, qtng::DataChannelPole pole,
                                                   const QString &itsPeerName, const QString &itsAddress)
{
    QSharedPointer<SocketChannel> channel;
    if (!cipher.isNull()) {
        QSharedPointer<SocketLike> encryptedChannel = encrypted(cipher, request);
        channel.reset(new SocketChannel(encryptedChannel, pole));
    } else {
        channel.reset(new SocketChannel(request, pole));
    }
    QSharedPointer<Peer> peer;
    try {
        qtng::Timeout timeout(5.0);
        Q_UNUSED(timeout);
        qDebug() << "got kcp peer:" << itsAddress << pole;
        peer = rpc->preparePeer(channel, itsPeerName, itsAddress);
        qDebug() << "got rpc peer:" << !peer.isNull();
    } catch (TimeoutException &) {
        qDebug() << "got rpc peer timeout:" << false;
    }
    return peer;
}

void LafdupPeer::setExtraKnownPeers(const QSet<QPair<HostAddress, quint16>> &extraKnownPeers)
{
    discovery->setExtraKnownPeers(extraKnownPeers);
}

void LafdupPeer::setPassword(QByteArray password)
{
    operations->spawn([this, password] {
        cipher.reset(new Cipher(Cipher::AES256, Cipher::CFB, Cipher::Encrypt));
        QByteArray salt("3.14159265358979323846");
        cipher->setPassword(password, salt);
        if (!cipher->isValid()) {
            return;
        }
        for (QSharedPointer<Peer> peer : static_cast<const QList<QSharedPointer<Peer>>>(rpc->getAllPeers())) {
            peer->close();
        }
    });
}

void LafdupPeer::setCacheDir(const QString &cacheDir)
{
    if (!this->cacheDir.isEmpty() && this->cacheDir != cacheDir) {
        QDir cacheDir(this->cacheDir);
        if (cacheDir.isReadable()) {
            operations->spawn([this, cacheDir] { _cleanFiles(cacheDir, true); });
        }
    }
    this->cacheDir = cacheDir;
}

void LafdupPeer::setDeleteFilesTime(int minutes)
{
    if (minutes >= 0) {
        this->deleteFilesTime = minutes;
    } else {
        this->deleteFilesTime = 0;
    }
}

void LafdupPeer::setSendFilesSize(float mb)
{
    if (mb < 0) {
        this->sendFilesSize = 0.0;
    } else {
        this->sendFilesSize = mb;
    }
}

void LafdupPeer::setIgnorePassword(bool ignorePassword)
{
    this->ignorePassword = ignorePassword;
}

QStringList LafdupPeer::getAllBoundAddresses()
{
    return discovery->getAllBoundAddresses();
}

quint16 LafdupPeer::getPort()
{
    return discovery->getPort();
}

quint16 LafdupPeer::getDefaultPort()
{
    return LafdupDiscovery::getDefaultPort();
}

bool LafdupPeer::findItem(const QDateTime &timestamp)
{
    for (const CopyPaste &item : qAsConst(items)) {
        if (qAbs(item.timestamp.msecsTo(timestamp)) <= 50) {
            return true;
        }
    }
    return false;
}

bool LafdupPeer::findItem(const CopyPaste &currentItem)
{
    for (const CopyPaste &item : qAsConst(items)) {
        if (item.text == currentItem.text && item.image == currentItem.image && item.files == currentItem.files
            && qAbs(item.timestamp.msecsTo(currentItem.timestamp)) <= 50) {
            return true;
        }
    }
    return false;
}

void LafdupPeer::writeInformation(const QDir destDir)
{
    const QString &iniFilePath = destDir.filePath("lafdup.ini");
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup("clean_files");
    settings.setValue("created", QDateTime::currentDateTime());
}

void LafdupPeer::cleanFiles()
{
    while (true) {
        if (this->cacheDir.isEmpty()) {
            Coroutine::sleep(30.0);
            continue;
        }
        QDir cacheDir(this->cacheDir);
        if (!cacheDir.isReadable()) {
            Coroutine::sleep(30.0);
            continue;
        }
        _cleanFiles(cacheDir, false);
        try {
            Coroutine::sleep(30.0);
        } catch (CoroutineException &) {
            return;
        }
    }
}

void LafdupPeer::_cleanFiles(const QDir &dir, bool cleanAll)
{
    if (!cleanAll && deleteFilesTime == 0) {
        return;
    }
    if (cleaningFiles) {
        return;
    }
    cleaningFiles = true;
    auto cleanup = qScopeGuard([this] { cleaningFiles = false; });
    const QDateTime &now = QDateTime::currentDateTime();
    for (const QFileInfo &fileInfo :
         static_cast<const QFileInfoList>(dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))) {
        if (!fileInfo.isWritable()) {
            continue;
        }
        QSharedPointer<QDir> subdir(new QDir(fileInfo.filePath()));
        const QString &iniFilePath = subdir->filePath("lafdup.ini");
        if (!QFileInfo::exists(iniFilePath)) {
            continue;
        }
        if (!cleanAll) {
            Q_ASSERT(deleteFilesTime != 0);
            QSettings settings(iniFilePath, QSettings::IniFormat);
            settings.beginGroup("clean_files");
            const QDateTime &timestamp = settings.value("created", now).toDateTime();
            if (!timestamp.isValid()) {
                continue;
            }
            qDebug() << now << timestamp << timestamp.msecsTo(now) << (deleteFilesTime * 60 * 1000);
            if (timestamp.msecsTo(now) < (deleteFilesTime * 60 * 1000)) {
                qDebug() << "skip directory" << subdir->path();
                continue;
            }
        }
        callInThread([subdir] {
            if (!subdir->removeRecursively()) {
                qDebug() << "can not remove directory:" << subdir;
            }
        });
    }
}

bool LafdupPeer::sendContentToPeer(QSharedPointer<lafrpc::Peer> peer, const CopyPaste &copyPaste, QString *errorString)
{
    float seconds = 20.0;
    if (copyPaste.mimeType == BinaryType) {
        if (copyPaste.ignoreLimits) {
            seconds = 60.0 * 60 * 24;  // one day!
        } else {
            seconds = 60.0 * 60;  // one hour
        }
    }
    Timeout timeout(seconds);
    Q_UNUSED(timeout);
    bool result = false;
    if (copyPaste.mimeType == TextType) {
        try {
            result = peer->call("lafdup.pasteText", copyPaste.timestamp, copyPaste.text).toBool();
        } catch (RpcException &e) {
            *errorString = e.what();
        } catch (TimeoutException &e) {
            *errorString = e.what();
        }
    } else if (copyPaste.mimeType == BinaryType) {
        QSharedPointer<VirtualRpcDirFileProvider> provider(new VirtualRpcDirFileProvider());
        for (const QString &filePath : copyPaste.files) {
            provider->addPath(filePath);
        }
        PopulateResult populateResult = provider->populate();
        QSharedPointer<RpcDir> rpcDir(new RpcDir());
        rpcDir->setName("paste");
        rpcDir->setEntries(populateResult.entries);
        rpcDir->setSize(populateResult.totalSize);
        QSharedPointer<Coroutine> t = operations->spawn([rpcDir, provider] { rpcDir->readFrom(provider); });
        try {
            result = peer->call("lafdup.pasteFiles", copyPaste.timestamp, QVariant::fromValue(rpcDir)).toBool();
        } catch (RpcException &e) {
            *errorString = e.what();
        } catch (TimeoutException &e) {
            *errorString = e.what();
        }
        t->kill();
    } else if (copyPaste.mimeType == ImageType) {
        QByteArray imageData = copyPaste.image;
        QSharedPointer<RpcFile> rpcFile(new RpcFile());
        rpcFile->setName("image.png");
        rpcFile->setSize(static_cast<quint64>(imageData.size()));
        QSharedPointer<Coroutine> t = operations->spawn([rpcFile, imageData] { rpcFile->sendall(imageData); });
        try {
            result = peer->call("lafdup.pasteImage", copyPaste.timestamp, QVariant::fromValue(rpcFile)).toBool();
        } catch (RpcException &e) {
            *errorString = e.what();
            return result;
        } catch (TimeoutException &e) {
            *errorString = e.what();
            return result;
        }
        t->kill();
    } else if (copyPaste.mimeType == CompType) {
        if (!copyPaste.text.isEmpty()) {
            try {
                result = peer->call("lafdup.pasteCompText", copyPaste.timestamp, copyPaste.text).toBool();
            } catch (RpcException &e) {
                *errorString = e.what();
                return result;
            } catch (TimeoutException &e) {
                *errorString = e.what();
                return result;
            }
        }
        if (!copyPaste.image.isNull()) {
            QByteArray imageData = copyPaste.image;
            QSharedPointer<RpcFile> rpcFile(new RpcFile());
            rpcFile->setName("image.png");
            rpcFile->setSize(static_cast<quint64>(imageData.size()));
            QSharedPointer<Coroutine> t1 = operations->spawn([rpcFile, imageData] { rpcFile->sendall(imageData); });
            auto cleanup = qScopeGuard([&t1]() {
                if (t1) {
                    t1->kill();
                }
            });
            try {
                result =
                        peer->call("lafdup.pasteCompImage", copyPaste.timestamp, QVariant::fromValue(rpcFile)).toBool();
            } catch (RpcException &e) {
                *errorString = e.what();
                return result;
            } catch (TimeoutException &e) {
                *errorString = e.what();
                return result;
            }
        }
        if (!copyPaste.files.isEmpty()) {
            QSharedPointer<VirtualRpcDirFileProvider> provider(new VirtualRpcDirFileProvider());
            for (const QString &filePath : copyPaste.files) {
                provider->addPath(filePath);
            }
            PopulateResult populateResult = provider->populate();
            QSharedPointer<RpcDir> rpcDir(new RpcDir());
            rpcDir->setName("paste");
            rpcDir->setEntries(populateResult.entries);
            rpcDir->setSize(populateResult.totalSize);
            QSharedPointer<Coroutine> t2 = operations->spawn([rpcDir, provider] { rpcDir->readFrom(provider); });
            auto cleanup = qScopeGuard([&t2]() {
                if (t2) {
                    t2->kill();
                }
            });
            try {
                result = peer->call("lafdup.pasteCompFiles", copyPaste.timestamp, QVariant::fromValue(rpcDir)).toBool();
            } catch (RpcException &e) {
                *errorString = e.what();
                return result;
            } catch (TimeoutException &e) {
                *errorString = e.what();
                return result;
            }
        }
        try {
            result = peer->call("lafdup.pasteEnd", copyPaste.timestamp).toBool();
        } catch (RpcException &e) {
            *errorString = e.what();
            return result;
        } catch (TimeoutException &e) {
            *errorString = e.what();
            return result;
        }
    }
    return result;
}

bool LafdupPeer::resultFeedBack(QSharedPointer<lafrpc::Peer> peer, QVariant result, QString errorText,
                                const CopyPaste &copyPaste)
{
    QPointer<LafdupPeer> self(this);
    if (self.isNull())
        return false;
    callInEventLoopAsync([self, peer, result, errorText, copyPaste] {
        if (!result.toBool()) {
            qCDebug(logger) << "can not paste to:" << peer->name() << peer->address();
            emit self->sendFeedBack(errorText);
        } else {
            emit self->sendFeedBack(tr("Sent successfully"));
            if (!self->findItem(copyPaste.timestamp)) {
                self->items.prepend(copyPaste);
                emit self->incoming(copyPaste);
            }
        }
    });
    if (result.toBool()) {
        return true;
    } else {
        return false;
    }
}
VirtualRpcDirFileProvider::~VirtualRpcDirFileProvider() { }

QSharedPointer<qtng::FileLike> VirtualRpcDirFileProvider::getFile(const QString &filePath, QIODevice::OpenMode mode)
{
    Q_ASSERT(mode == QIODevice::ReadOnly);
    const QString &fullFilePath = makePath(filePath);
    if (fullFilePath.isEmpty()) {
        return QSharedPointer<FileLike>();
    } else {
        QSharedPointer<QFile> file(new QFile(fullFilePath));
        if (file->open(mode)) {
            return FileLike::rawFile(file);
        } else {
            return QSharedPointer<FileLike>();
        }
    }
}

QString VirtualRpcDirFileProvider::makePath(const QString &filePath)
{
    if (filePath.isEmpty()) {
        return QString();
    }
    const QStringList &parts = filePath.split("/");
    Q_ASSERT(!parts.isEmpty());
    const QString &name = parts.at(0);
    const QStringList &subpaths = parts.mid(1);

    for (const QFileInfo &fileInfo : qAsConst(this->fileInfoList)) {
        if (fileInfo.fileName() == name) {
            QString path = fileInfo.filePath();
            if (subpaths.isEmpty()) {
                return path;
            } else {
                if (!fileInfo.isDir()) {
                    qDebug() << "invalid path:" << filePath;
                } else {
                    QDir dir(path);
                    return dir.filePath(subpaths.join("/"));
                }
            }
        }
    }
    return QString();
}

void VirtualRpcDirFileProvider::addPath(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    if (fileInfo.isReadable()) {
        fileInfoList.append(fileInfo);
    }
}

void VirtualRpcDirFileProvider::addFileInfo(const QFileInfo &fileInfo)
{
    if (fileInfo.isReadable()) {
        fileInfoList.append(fileInfo);
    }
}

static void _populate(const QDir &dir, const QString &relativePath, PopulateResult &result)
{
    QDir::Filters filters = QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden;
    //    QDir::Filters filters = QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot;
    for (const QFileInfo &fileInfo : static_cast<const QFileInfoList>(dir.entryInfoList(filters, QDir::DirsFirst))) {
        RpcDirFileEntry entry;
        const QString &name = fileInfo.fileName();
        if (Q_UNLIKELY(name.contains("/"))) {
            continue;
        }
        entry.path = relativePath.isEmpty() ? name : relativePath + "/" + name;
        quint64 size = static_cast<quint64>(fileInfo.size());
        if (fileInfo.isSymLink()) {
            QFile file(fileInfo.absoluteFilePath());
            file.open(QIODevice::ReadOnly);
            size = static_cast<quint64>(file.size());
            file.close();
        }
        entry.size = size;
        entry.isdir = fileInfo.isDir();
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        entry.created = fileInfo.birthTime();
#else
        entry.created = fileInfo.created();
#endif
        entry.lastModified = fileInfo.lastModified();
        entry.lastAccess = fileInfo.lastRead();
        result.entries.append(entry);
        result.totalSize += entry.size;
        if (fileInfo.isDir()) {
            _populate(QDir(fileInfo.filePath()), entry.path, result);
        }
    }
}

static PopulateResult populate(const QDir &dir, const QString &name)
{
    PopulateResult result;
    _populate(dir, name, result);
    return result;
}

PopulateResult VirtualRpcDirFileProvider::populate()
{
    PopulateResult result;
    for (const QFileInfo &fileInfo : qAsConst(fileInfoList)) {
        if (fileInfo.isDir()) {
            QString name = fileInfo.fileName();
            RpcDirFileEntry entry;
            entry.path = name;
            entry.size = 0;
            entry.isdir = true;
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
            entry.created = fileInfo.birthTime();
#else
            entry.created = fileInfo.created();
#endif
            entry.lastModified = fileInfo.lastModified();
            entry.lastAccess = fileInfo.lastRead();
            result.entries.append(entry);

            QDir dir(fileInfo.filePath());
            PopulateResult part = qtng::callInThread<PopulateResult>([dir, name] { return ::populate(dir, name); });
            result.entries.append(part.entries);
            result.totalSize += part.totalSize;
        } else if (fileInfo.isFile()) {
            RpcDirFileEntry entry;
            const QString &name = fileInfo.fileName();
            if (Q_UNLIKELY(name.contains("/"))) {
                continue;
            }
            quint64 size = static_cast<quint64>(fileInfo.size());
            if (fileInfo.isSymLink()) {
                QFile file(fileInfo.absoluteFilePath());
                file.open(QIODevice::ReadOnly);
                size = static_cast<quint64>(file.size());
                file.close();
            }
            entry.path = name;
            entry.size = size;
            entry.isdir = fileInfo.isDir();
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
            entry.created = fileInfo.birthTime();
#else
            entry.created = fileInfo.created();
#endif
            entry.lastModified = fileInfo.lastModified();
            entry.lastAccess = fileInfo.lastRead();
            result.entries.append(entry);
            result.totalSize += entry.size;
        } else {
            qWarning() << "unknown file type:" << fileInfo.path();
        }
    }
    return result;
}
