#include <QtCore/qloggingcategory.h>
#include <QtCore/qsettings.h>
#include <QtCore/qscopedvaluerollback.h>
#include "discovery.h"
#include "lafdupapplication.h"
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
    if (parent->d_func()->findItem(timestamp)) {
        throw RpcRemoteException(tr("The same content is sent repeatedly"));
    }
    CopyPaste item;
    item.direction = CopyPaste::Incoming;
    item.timestamp = timestamp;
    item.mimeType = TextType;
    item.text = text;
    parent->d_func()->items.prepend(item);
    QPointer<LafdupPeer> peer(parent);
    callInEventLoopAsync([peer, item] {
        if (!peer.isNull()) {
            emit peer->incoming(item);
        }
    });
    return true;
}

bool LafdupRemoteStub::pasteFiles(const QDateTime &timestamp, QSharedPointer<RpcDir> rpcDir)
{
    if (parent->d_func()->findItem(timestamp)) {
        throw RpcRemoteException(tr("The same content is sent repeatedly"));
    }
    if (rpcDir.isNull() || !rpcDir->isValid()) {
        throw RpcRemoteException(tr("The local file to send could not be found"));
    }
    if (parent->d_func()->cacheDir.isEmpty()) {
        throw RpcRemoteException(tr("The storage path for the other party is empty"));
    }
    QDir cacheDir(parent->d_func()->cacheDir);
    if (!cacheDir.isReadable()) {
        throw RpcRemoteException(tr("The storage path given by the other party is invalid"));
    }
    // Include milliseconds so two sends in the same second do not collide.
    // Dedup window is 50ms, but directory names used to be second-precision only.
    QString subdir = timestamp.toString(QStringLiteral("yyyyMMddHHmmsszzz"));
    if (cacheDir.exists(subdir)) {
        subdir += QLatin1Char('_') + QString::number(QDateTime::currentMSecsSinceEpoch());
    }
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
    parent->d_func()->items.prepend(item);
    QPointer<LafdupPeer> parentRef(parent);
    callInEventLoopAsync([parentRef, item] {
        if (!parentRef.isNull()) {
            emit parentRef->incoming(item);
        }
    });
    parent->d_func()->writeInformation(destDir);
    return true;
}

bool LafdupRemoteStub::pasteImage(const QDateTime &timestamp, QSharedPointer<RpcFile> image)
{
    if (!timestamp.isValid() || image.isNull() || image->name().isEmpty()) {
        throw RpcRemoteException(tr("The local file to send could not be found"));
    }
    if (parent->d_func()->findItem(timestamp)) {
        throw RpcRemoteException(tr("The same content is sent repeatedly"));
    }
    QByteArray imageData;
    bool ok = image->recvall(imageData);
    if (!ok) {
        qCDebug(logger) << "can not receive image data.";
        throw RpcRemoteException(tr("Failed to receive the picture"));
    }
    CopyPaste item;
    item.timestamp = timestamp;
    item.mimeType = ImageType;
    item.image = imageData;
    parent->d_func()->items.prepend(item);
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
    : d_ptr(new LafdupPeerPrivate(this, uuid, port))
{
}

LafdupPeer::~LafdupPeer()
{
}

bool LafdupPeer::start()
{
    Q_D(LafdupPeer);
    if (!d->discovery->start()) {
        return false;
    }
    d->operations->spawnWithName("clean_files", [d] { d->cleanFiles(); });
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
    Q_D(LafdupPeer);
    d->discovery->stop();
    d->operations->kill("clean_files");
    QPointer<LafdupPeer> self(this);
    callInEventLoopAsync([self] {
        if (!self.isNull()) {
            emit self->stateChanged(false);
        }
    });
}

void LafdupPeer::outgoing(const CopyPaste &copyPaste, bool unlimited)
{
    Q_D(LafdupPeer);
    d->outgoing(copyPaste, unlimited);
}

void LafdupPeer::setPassword(QByteArray password)
{
    Q_D(LafdupPeer);
    d->setPassword(password);
}

void LafdupPeer::setExtraKnownPeers(const QSet<QPair<HostAddress, quint16>> &extraKnownPeers)
{
    Q_D(LafdupPeer);
    d->discovery->setExtraKnownPeers(extraKnownPeers);
}

void LafdupPeer::setCacheDir(const QString &cacheDir)
{
    Q_D(LafdupPeer);
    d->setCacheDir(cacheDir);
}

void LafdupPeer::setDeleteFilesTime(int minutes)
{
    Q_D(LafdupPeer);
    if (minutes >= 0) {
        d->deleteFilesTime = minutes;
    } else {
        d->deleteFilesTime = 0;
    }
}

void LafdupPeer::setSendFilesSize(float mb)
{
    Q_D(LafdupPeer);
    if (mb < 0) {
        d->sendFilesSize = 0.0;
    } else {
        d->sendFilesSize = mb;
    }
}

void LafdupPeer::setIgnorePassword(bool ignorePassword)
{
    Q_D(LafdupPeer);
    d->ignorePassword = ignorePassword;
}

QStringList LafdupPeer::allBoundAddresses()
{
    Q_D(LafdupPeer);
    return d->discovery->allBoundAddresses();
}

quint16 LafdupPeer::port()
{
    Q_D(LafdupPeer);
    return d->discovery->port();
}

quint16 LafdupPeer::defaultPort()
{
    return LafdupDiscovery::defaultPort();
}

LafdupPeerPrivate::LafdupPeerPrivate(LafdupPeer *q, const QByteArray &uuid, quint16 port)
    : stub(new LafdupRemoteStub(q))
    , deleteFilesTime(5)
    , sendFilesSize(10.0)
    , ignorePassword(false)
    , cleaningFiles(false)
    , operations(new CoroutineGroup())
    , q_ptr(q)
{
    lafrpc::registerClass<CopyPaste>();
    rpc = Rpc::builder(MessagePack).myPeerName(uuid).create();
    Q_ASSERT(!rpc.isNull());
    rpc->registerInstance(stub, "lafdup");
    discovery.reset(new LafdupDiscovery(uuid, port, q));
}

LafdupPeerPrivate::~LafdupPeerPrivate()
{
    delete operations;
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

void LafdupPeerPrivate::_outgoingSync(CopyPaste copyPaste, bool force)
{
    if (!canSendContent(copyPaste, force)) {
        return;
    }
    const QList<QString> peerList = rpc->getAllPeerNames();
    QSet<QString> peerNames(peerList.begin(), peerList.end());
    CoroutineGroup workers;
    QList<QSharedPointer<QString>> errorStrings;
    for (const QString &peerName : qAsConst(peerNames)) {
        QSharedPointer<Peer> peer = rpc->get(peerName);
        QSharedPointer<QString> errorString(new QString());
        workers.spawn([this, peer, copyPaste, errorString] { sendContentToPeer(peer, copyPaste, errorString.data()); });
        errorStrings.append(errorString);
    }
    workers.joinall();
    QStringList errors;
    for (const QSharedPointer<QString> &errorString : qAsConst(errorStrings)) {
        if (errorString->isEmpty()) {
            continue;
        }
        errors.append(*errorString);
    }
    if (errors.isEmpty()) {
        if (!findItem(copyPaste)) {
            items.prepend(copyPaste);
            emit q_ptr->incoming(copyPaste);
        }
    } else {
        qCWarning(logger) << "send content failed:" << errors.join(QStringLiteral("; "));
    }
}

bool LafdupPeerPrivate::canSendContent(const CopyPaste &copyPaste, bool unlimited)
{
    if (copyPaste.mimeType == TextType) {
        if (!unlimited && ignorePassword && isPassword(copyPaste.text)) {
            return false;
        }
    } else if (copyPaste.mimeType == BinaryType) {
        if (qFuzzyIsNull(sendFilesSize)) {
            return false;
        }
        if (unlimited) {
            return true;
        }
        QSharedPointer<VirtualRpcDirFileProvider> provider(new VirtualRpcDirFileProvider());
        for (const QString &filePath : copyPaste.files) {
            provider->addPath(filePath);
        }

        PopulateResult populateResult = provider->populate();
        if (populateResult.totalSize >= static_cast<quint64>(sendFilesSize * 1024 * 1024)) {
            return false;
        }
    } else if (copyPaste.mimeType == ImageType) {
        QByteArray imageData = copyPaste.image;
        if (imageData.isEmpty()) {
            return false;
        }
    } else {
        return false;
    }
    return true;
}

void LafdupPeerPrivate::outgoing(const CopyPaste &copyPaste, bool unlimited)
{
    if (findItem(copyPaste.timestamp)) {
        return;
    }
    operations->spawn([this, copyPaste, unlimited]() { _outgoingSync(copyPaste, unlimited); });
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
    Q_D(const LafdupPeer);
    const QString &kcpAddress = makeAddress("kcp", remoteHost, port);
    const QString &tcpAddress = makeAddress("tcp", remoteHost, port);
    for (const QSharedPointer<Peer> &peer : static_cast<const QList<QSharedPointer<Peer>>>(d->rpc->getAllPeers())) {
        if (peer->address() == kcpAddress || peer->address() == tcpAddress) {
            return true;
        }
    }
    return false;
}

bool LafdupPeer::hasPeer(const QString &peerName)
{
    Q_D(const LafdupPeer);
    return !d->rpc->get(peerName).isNull();
}

void LafdupPeer::tryToConnectPeer(QString itsPeerName, HostAddress remoteHost, quint16 port)
{
    Q_D(LafdupPeer);
    d->operations->spawn([this, d, itsPeerName, remoteHost, port] {
        for (QSharedPointer<Peer> oldPeer : static_cast<const QList<QSharedPointer<Peer>>>(d->rpc->getAll(itsPeerName))) {
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
        if (d->connectingPeers.contains(tcpAddress)) {
            return;
        }
        d->connectingPeers.insert(tcpAddress);

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
            d->connectingPeers.remove(tcpAddress);
        } catch (TimeoutException &) {
            d->connectingPeers.remove(tcpAddress);
            // pass and go on.
        }

        if (!peer.isNull()) {
            return;
        }

        const QString &kcpAddress = makeAddress("kcp", remoteHost, port);
        if (d->connectingPeers.contains(kcpAddress)) {
            return;
        }
        d->connectingPeers.insert(kcpAddress);
        try {
            Timeout timeout(5.0);
            QSharedPointer<KcpSocket> kcpSocket(new KcpSocket(HostAddress::IPv4Protocol));
            kcpSocket->setOption(Socket::BroadcastSocketOption, true);
            if (kcpSocket->connect(remoteHost, port)) {
                handleKcpRequestSync(kcpSocket, PositivePole, itsPeerName);
            }
            d->connectingPeers.remove(kcpAddress);
        } catch (...) {
            d->connectingPeers.remove(kcpAddress);
            throw;
        }
    });
}

void LafdupPeer::tryToConnectPeer(QSharedPointer<qtng::KcpSocket> request)
{
    Q_D(LafdupPeer);
    qCDebug(logger) << "try to connect peer via kcp:" << request->peerAddress() << request->peerPort();
    // Accept side must not expect a peer name; remote UUID comes from preparePeer().
    d->operations->spawn([this, d, request] { handleKcpRequestSync(request, NegativePole, QString()); });
}

void LafdupPeer::tryToConnectPeer(QSharedPointer<qtng::Socket> request)
{
    Q_D(LafdupPeer);
    const QString &address = makeAddress("tcp", request->peerAddress(), request->peerPort());
    d->operations->spawn([this, d, request, address] {
        handleRequestSync(asSocketLike(request), NegativePole, QString(), address);
    });
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
    Q_D(LafdupPeer);
    QSharedPointer<SocketChannel> channel;
    if (!d->cipher.isNull()) {
        QSharedPointer<SocketLike> encryptedChannel = encrypted(d->cipher, request);
        channel.reset(new SocketChannel(encryptedChannel, pole));
    } else {
        channel.reset(new SocketChannel(request, pole));
    }
    if (!itsAddress.startsWith("kcp")) {
        channel->setKeepaliveTimeout(30);
    }
    QSharedPointer<Peer> peer;
    try {
        qtng::Timeout timeout(5.0);
        Q_UNUSED(timeout);
        qCDebug(logger) << "got kcp peer:" << itsAddress << pole;
        peer = d->rpc->preparePeer(channel, itsPeerName, itsAddress);
        qCDebug(logger) << "got rpc peer:" << !peer.isNull();
    } catch (TimeoutException &) {
        qCDebug(logger) << "got rpc peer timeout:" << false;
    }
    return peer;
}

void LafdupPeerPrivate::setPassword(QByteArray password)
{
    operations->spawn([this, password] {
        if (password.isEmpty()) {
            cipher.clear();
            for (QSharedPointer<Peer> peer : static_cast<const QList<QSharedPointer<Peer>>>(rpc->getAllPeers())) {
                peer->close();
            }
            return;
        }
        cipher.reset(new Cipher(Cipher::AES256, Cipher::CFB, Cipher::Encrypt));
        if (!cipher->setPassword(password, LafdupApplication::linkLayerSalt())) {
            cipher.clear();
            return;
        }
        for (QSharedPointer<Peer> peer : static_cast<const QList<QSharedPointer<Peer>>>(rpc->getAllPeers())) {
            peer->close();
        }
    });
}

void LafdupPeerPrivate::setCacheDir(const QString &cacheDir)
{
    if (!this->cacheDir.isEmpty() && this->cacheDir != cacheDir) {
        QDir oldCacheDir(this->cacheDir);
        if (oldCacheDir.isReadable()) {
            operations->spawn([this, oldCacheDir] { _cleanFiles(oldCacheDir, true); });
        }
    }
    this->cacheDir = cacheDir;
}

bool LafdupPeerPrivate::findItem(const QDateTime &timestamp)
{
    for (const CopyPaste &item : qAsConst(items)) {
        if (qAbs(item.timestamp.msecsTo(timestamp)) <= 50) {
            return true;
        }
    }
    return false;
}

bool LafdupPeerPrivate::findItem(const CopyPaste &currentItem)
{
    for (const CopyPaste &item : qAsConst(items)) {
        if (item.text == currentItem.text && item.image == currentItem.image && item.files == currentItem.files
            && qAbs(item.timestamp.msecsTo(currentItem.timestamp)) <= 50) {
            return true;
        }
    }
    return false;
}

void LafdupPeerPrivate::writeInformation(const QDir destDir)
{
    const QString &iniFilePath = destDir.filePath("lafdup.ini");
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup("clean_files");
    settings.setValue("created", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
}

void LafdupPeerPrivate::cleanFiles()
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

void LafdupPeerPrivate::_cleanFiles(const QDir &dir, bool cleanAll)
{
    if (!cleanAll && deleteFilesTime == 0) {
        return;
    }
    if (cleaningFiles) {
        return;
    }
    QScopedValueRollback<bool> svr(cleaningFiles, true);
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
            const QDateTime &timestamp =
                    QDateTime::fromString(settings.value("created", now).toString(), "yyyy-MM-dd hh:mm:ss");
            if (!timestamp.isValid()) {
                continue;
            }
            qCDebug(logger) << "want to clean deprecated files:" << now << timestamp << timestamp.msecsTo(now) << (deleteFilesTime * 60 * 1000);
            if (timestamp.msecsTo(now) < (deleteFilesTime * 60 * 1000)) {
                qCDebug(logger) << "skip directory" << subdir->path();
                continue;
            }
        }
        callInThread([subdir] {
            if (!subdir->removeRecursively()) {
                qCDebug(logger) << "can not remove directory:" << subdir;
            } else {
                qCDebug(logger) << "remove directory:" << subdir << " sucess";
            }
        });
    }
}

bool LafdupPeerPrivate::sendContentToPeer(QSharedPointer<lafrpc::Peer> peer, const CopyPaste &copyPaste, QString *errorString)
{
    float seconds = 20.0;
    if (copyPaste.mimeType == BinaryType) {
        seconds = 60.0 * 20; // 20 minutes
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
        t->join();
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
            return false;
        } catch (TimeoutException &e) {
            *errorString = e.what();
            return false;
        }
        t->join();
    }
    return result;
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
                    qCDebug(logger) << "invalid path:" << filePath;
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
