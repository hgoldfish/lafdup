#ifndef LAFDUP_PEER_PH
#define LAFDUP_PEER_PH

#include "lafrpc.h"
#include "peer.h"
#include "discovery.h"

struct PasteHashKey
{
    PasteHashKey(QString _name, QDateTime _time)
    {
        name = _name;
        time = _time;
    };
    QString name;
    QDateTime time;
};

class LafdupRemoteStub : public QObject
{
    Q_OBJECT
public:
    LafdupRemoteStub(LafdupPeer *parent);
public slots:
    bool pasteText(const QDateTime &timestamp, const QString &text);
    bool pasteFiles(const QDateTime &timestamp, QSharedPointer<lafrpc::RpcDir> rpcDir);
    bool pasteImage(const QDateTime &timestamp, QSharedPointer<lafrpc::RpcFile> image);
    bool ping();
    QDateTime getCurrentTime();
private:
    LafdupPeer *parent;
    QHash<PasteHashKey, CopyPaste> pasteHash;
};

inline bool operator==(const PasteHashKey &p1, const PasteHashKey &p2)
{
    return (p1.name == p2.name) && (p1.time == p2.time);
};

inline uint qHash(const PasteHashKey &key, uint seed) noexcept
{
    uint hash = qHash(key.name, seed);
    hash ^= qHash(key.time, hash);
    return hash;
}

class LafdupPeerPrivate
{
public:
    LafdupPeerPrivate(LafdupPeer *q, const QByteArray &uuid, quint16 port);
    ~LafdupPeerPrivate();

    void outgoing(const CopyPaste &copyPaste, bool unlimited);
    void setPassword(QByteArray password);
    void setCacheDir(const QString &cacheDir);

    void _outgoingSync(CopyPaste copyPaste, bool force);
    bool canSendContent(const CopyPaste &copyPaste, bool force);
    bool findItem(const QDateTime &timestamp);
    bool findItem(const CopyPaste &currentItem);
    void writeInformation(const QDir destDir);
    void cleanFiles();
    void _cleanFiles(const QDir &dir, bool cleanAll);
    bool sendContentToPeer(QSharedPointer<lafrpc::Peer> peer, const CopyPaste &copyPaste, QString *errorString);

    QSharedPointer<LafdupDiscovery> discovery;
    QSharedPointer<LafdupRemoteStub> stub;
    QSharedPointer<qtng::Cipher> cipher;
    QList<CopyPaste> items;
    QSet<QString> connectingPeers;
    QString cacheDir;
    int deleteFilesTime;
    float sendFilesSize;
    bool ignorePassword;
    bool cleaningFiles;
    qtng::CoroutineGroup *operations;
    QSharedPointer<lafrpc::Rpc> rpc;

    LafdupPeer *q_ptr;
    Q_DECLARE_PUBLIC(LafdupPeer)
};

#endif
