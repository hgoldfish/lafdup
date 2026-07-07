#ifndef LAFDUP_DISCOVERY_H
#define LAFDUP_DISCOVERY_H

#include <QtCore/qscopedpointer.h>
#include "qtnetworkng.h"

class LafdupPeer;
class LafdupDiscoveryPrivate;

class LafdupDiscovery
{
public:
    LafdupDiscovery(const QByteArray &uuid, quint16 port, LafdupPeer *parent);
    ~LafdupDiscovery();
    bool start();
    void stop();
    void setExtraKnownPeers(const QSet<QPair<qtng::HostAddress, quint16>> &extraKnownPeers);
    QSet<QPair<qtng::HostAddress, quint16>> extraKnownPeers() const;
    QStringList allBoundAddresses();
    quint16 port();
    QByteArray uuid();
    static quint16 defaultPort();

private:
    Q_DECLARE_PRIVATE(LafdupDiscovery)
    QScopedPointer<LafdupDiscoveryPrivate> d_ptr;
};

#endif
