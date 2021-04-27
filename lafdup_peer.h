#ifndef LAFDUP_PEER_H
#define LAFDUP_PEER_H
#include <QtCore/qobject.h>
#include "qtnetworkng.h"

class LafdupPeerPrivate;
class LafdupPeer: public QObject
{
    Q_OBJECT
public:
    LafdupPeer();
    ~LafdupPeer() override;
public:
    bool start();
    void stop();
    void setCipher(QSharedPointer<qtng::Cipher> cipher);
    QSharedPointer<qtng::Cipher> cipher() const;
    void setKnownPeers(const QList<qtng::HostAddress> &knownPeers);
    QStringList getAllBoundAddresses();
    quint16 getPort();
signals:
    void incoming(const QDateTime &timestamp, const QString &text);
    void stateChanged(bool ok);
public slots:
    bool outgoing(const QDateTime &timestamp, const QString &text);
private:
    LafdupPeerPrivate *dd_ptr;
    Q_DECLARE_PRIVATE_D(dd_ptr, LafdupPeer)
};

#endif
