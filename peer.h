#ifndef LAFDUP_PEER_H
#define LAFDUP_PEER_H

#include <QtCore/qobject.h>
#include <QtCore/qscopedpointer.h>
#include <QtCore/qset.h>
#include "qtnetworkng.h"
#include "models.h"

class LafdupDiscovery;
class LafdupPeerPrivate;
namespace lafrpc {class Peer;}
class LafdupPeer : public QObject
{
    Q_OBJECT
public:
    LafdupPeer(const QByteArray &uuid, quint16 port);
    ~LafdupPeer() override;
    bool start();
    void stop();
    void outgoing(const CopyPaste &copyPaste, bool unlimited);
    void setPassword(QByteArray password);
    void setExtraKnownPeers(const QSet<QPair<qtng::HostAddress, quint16>> &extraKnownPeers);
    void setCacheDir(const QString &cacheDir);
    void setDeleteFilesTime(int minutes);
    void setSendFilesSize(float mb);
    void setIgnorePassword(bool ignorePassword);
    QStringList allBoundAddresses();
    quint16 port();
    static quint16 defaultPort();
signals:
    void incoming(const CopyPaste &copyPaste);
    void stateChanged(bool ok);
protected:
    bool hasPeer(const qtng::HostAddress &remoteHost, quint16 port);
    bool hasPeer(const QString &peerName);
    void tryToConnectPeer(QString itsPeerName, qtng::HostAddress remoteHost, quint16 port);
    void tryToConnectPeer(QSharedPointer<qtng::KcpSocket> request);
    void tryToConnectPeer(QSharedPointer<qtng::Socket> request);
private:
    void handleKcpRequestSync(QSharedPointer<qtng::KcpSocket> request, qtng::DataChannelPole pole,
                              const QString &itsPeerName);
    QSharedPointer<lafrpc::Peer> handleRequestSync(QSharedPointer<qtng::SocketLike> request, qtng::DataChannelPole pole,
                                                   const QString &itsPeerName, const QString &itsAddress);
    Q_DECLARE_PRIVATE(LafdupPeer)
    QScopedPointer<LafdupPeerPrivate> d_ptr;
    friend class LafdupRemoteStub;
    friend class LafdupDiscoveryPrivate;
};

#endif
