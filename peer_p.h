#ifndef LAFDUP_PEER_PH
#define LAFDUP_PEER_PH

#include "peer.h"

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
    bool pasteCompText(const QDateTime &timestamp, const QString &text,const bool &isHasTextHtml);
    bool pasteCompImage(const QDateTime &timestamp, QSharedPointer<lafrpc::RpcFile> image);
    bool pasteCompFiles(const QDateTime &timestamp, QSharedPointer<lafrpc::RpcDir> rpcDir);
    bool pasteEnd(const QDateTime &time);
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

#endif
