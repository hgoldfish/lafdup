#ifndef LAFDUP_PEER_H
#define LAFDUP_PEER_H

#include <QtCore/qset.h>
#include "lafrpc.h"
#include "models.h"

class LafdupDiscovery;
class LafdupRemoteStub;

class LafdupPeer : public QObject
{
    Q_OBJECT
public:
    LafdupPeer(const QByteArray &uuid, quint16 port);
    virtual ~LafdupPeer() override;
public:
    bool start();
    void stop();
    void outgoing(const CopyPaste &copyPaste);
    void setPassword(QByteArray password);
    void setExtraKnownPeers(const QSet<QPair<qtng::HostAddress, quint16>> &extraKnownPeers);
    void setCacheDir(const QString &cacheDir);
    void setDeleteFilesTime(int minutes);
    void setSendFilesSize(float mb);
    void setIgnorePassword(bool ignorePassword);
    QStringList getAllBoundAddresses();
    quint16 getPort();
    static quint16 getDefaultPort();
signals:
    void incoming(const CopyPaste &copyPaste);
    void stateChanged(bool ok);
    void sendFeedBack(QString str);
    void sendAction();
protected:
    bool hasPeer(const qtng::HostAddress &remoteHost, quint16 port);
    bool hasPeer(const QString &peerName);
    void tryToConnectPeer(QString itsPeerName, qtng::HostAddress remoteHost, quint16 port);
    void tryToConnectPeer(QSharedPointer<qtng::KcpSocket> request);
private:
    void handleKcpRequestSync(QSharedPointer<qtng::KcpSocket> request, qtng::DataChannelPole pole,
                              const QString &itsPeerName);
    QSharedPointer<lafrpc::Peer> handleRequestSync(QSharedPointer<qtng::SocketLike> request, qtng::DataChannelPole pole,
                                                   const QString &itsPeerName, const QString &itsAddress);
    void _outgoingSync(CopyPaste copyPaste);
    bool canSendContent(const CopyPaste &copyPaste);

    bool findItem(const QDateTime &timestamp);
    bool findItem(const CopyPaste &currentItem);
    void writeInformation(const QDir destDir);
    void cleanFiles();
    void _cleanFiles(const QDir &dir, bool cleanAll);
    bool sendContentToPeer(QSharedPointer<lafrpc::Peer> peer, const CopyPaste &copyPaste, QString *errorString);
    bool resultFeedBack(QSharedPointer<lafrpc::Peer> peer, QVariant result, QString errorText,
                        const CopyPaste &copyPaste);
private:
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
    friend LafdupRemoteStub;
    friend LafdupDiscovery;
    friend struct MarkCleaning;
public:
    QSharedPointer<lafrpc::Rpc> rpc;
};

#endif
