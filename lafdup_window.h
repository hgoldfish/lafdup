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
    virtual void showEvent(QShowEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void closeEvent(QCloseEvent *event) override;
public slots:
    void sendContent();
    void showAndGetFocus();
private slots:
    void updateClipboard(const QDateTime &timestamp, const QString &text);
    void onPeerStateChanged(bool ok);
    void managePeers();
    void setPassword();
    void useOldContent(const QModelIndex &current);
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
    bool started;
};

#endif
