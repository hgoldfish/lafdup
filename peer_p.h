#ifndef LAFDUP_PEER_PH
#define LAFDUP_PEER_PH

#include "peer.h"

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
};

#endif
