#ifndef LAFDUP_WINDOW_H
#define LAFDUP_WINDOW_H
#include <QtCore/qabstractitemmodel.h>
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qsystemtrayicon.h>
#include "lafdup_peer.h"


namespace Ui {
class LafdupWindow;
}


class ActionModel;
class LafdupWindow: public QWidget
{
    Q_OBJECT
public:
    LafdupWindow();
    virtual ~LafdupWindow() override;
protected:
    void showEvent(QShowEvent *event);
    void closeEvent(QCloseEvent *event);
public slots:
    void sendContent();
private slots:
    void updateClipboard(const QDateTime &timestamp, const QString &text);
    void onPeerStateChanged(bool ok);
    void managePeers();
    void setPassword();
    void useOldContent(const QModelIndex &current);
    void closeForcely();
    void onClipboardChanged();
private:
    void updateMyIP();
    void loadPassword();
    void loadKnownPeers();
private:
    Ui::LafdupWindow *ui;
    QSharedPointer<LafdupPeer> peer;
    ActionModel *actionModel;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    bool exiting;
    bool started;
};

#endif
