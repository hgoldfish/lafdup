#ifndef LAFDUP_DISCOVERY_P_H
#define LAFDUP_DISCOVERY_P_H

#include "discovery.h"

class LafdupPeer;
class LafdupDiscovery;
class LafdupDiscoveryPrivate;

class LafdupKcpSocket : public qtng::KcpSocket
{
public:
    LafdupKcpSocket(LafdupDiscoveryPrivate *d);
    bool filter(char *data, qint32 *len, qtng::HostAddress *addr, quint16 *port) override;

private:
    LafdupDiscoveryPrivate *d;
};

class LafdupDiscoveryPrivate
{
public:
    LafdupDiscoveryPrivate(LafdupDiscovery *q, const QByteArray &uuid, quint16 port, LafdupPeer *parent);
    ~LafdupDiscoveryPrivate();

    void serveUdp();
    void serveTcp();
    void discovery();
    void handleDiscoveryRequest(const QByteArray &packet, qtng::HostAddress addr, quint16 port);

    QSharedPointer<LafdupKcpSocket> kcpSocket;
    QSharedPointer<qtng::Socket> tcpServer;
    qtng::CoroutineGroup *operations;
    QHash<QString, QPair<qtng::HostAddress, quint16>> knownPeers;
    QSet<QPair<qtng::HostAddress, quint16>> extraKnownPeers;
    QByteArray uuid;
    LafdupPeer *local;
    quint16 port;

    LafdupDiscovery *q_ptr;
    Q_DECLARE_PUBLIC(LafdupDiscovery)
};

#endif
