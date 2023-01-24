#ifndef LAFDUP_DISCOVERY_H
#define LAFDUP_DISCOVERY_H

#include "lafrpc.h"


class LafdupDiscovery;
class LafdupKcpSocket: public qtng::KcpSocket
{
public:
    LafdupKcpSocket(LafdupDiscovery *parent);
    virtual bool filter(char *data, qint32 *len, qtng::HostAddress *addr, quint16 *port) override;
private:
    LafdupDiscovery *parent;
};


class LafdupPeer;
class LafdupDiscovery
{
public:
    LafdupDiscovery(const QByteArray &uuid, quint16 port, LafdupPeer *parent);
    ~LafdupDiscovery();
public:
    bool start();
    void stop();
    void setExtraKnownPeers(const QSet<QPair<qtng::HostAddress, quint16>> &extraKnownPeers);
    QSet<QPair<qtng::HostAddress, quint16>> getExtraKnownPeers();
    QStringList getAllBoundAddresses();
    quint16 getPort();
    QByteArray getUuid();
    static quint16 getDefaultPort();
    QSharedPointer<qtng::KcpSocket> connect(const qtng::HostAddress &addr, quint16 port);
private:
    void serve();
    void discovery();
    void handleDiscoveryRequest(const QByteArray &packet, qtng::HostAddress addr, quint16 port);
private:
    QSharedPointer<LafdupKcpSocket> kcpSocket;
    qtng::CoroutineGroup *operations;
    QHash<QString, QPair<qtng::HostAddress, quint16>> knownPeers;
    QSet<QPair<qtng::HostAddress, quint16>> extraKnownPeers;
    QByteArray uuid;
    LafdupPeer *parent;
    quint16 port;
    friend class LafdupKcpSocket;
};

#endif
