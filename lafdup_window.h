#ifndef LAFDUP_WINDOW_H
#define LAFDUP_WINDOW_H
#include <QtCore/qabstractitemmodel.h>
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qsystemtrayicon.h>
#include <QtCore/qcoreevent.h>
#include <QtWidgets/qmessagebox.h>
#include "peer.h"
namespace Ui {
class LafdupWindow;
}
class GuideDialog;
class MessageTips;
class CopyPasteModel;
class LafdupWindow : public QWidget
{
    Q_OBJECT
public:
    LafdupWindow();
    virtual ~LafdupWindow() override;
protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void changeEvent(QEvent *event) override;
    virtual void closeEvent(QCloseEvent *event) override;
    virtual void dropEvent(QDropEvent *event) override;
    virtual void dragEnterEvent(QDragEnterEvent *event) override;
    bool event(QEvent *e) override;
public slots:
    void sendContent();
    void showAndGetFocus();

private slots:
    void updateClipboard(const CopyPaste &copyPaste);
    void onPeerStateChanged(bool ok);
    void configure();
    void setPassword();
    void useOldContent(const QModelIndex &current);
    void onClipboardChanged();
    void showContextMenu(const QPoint &pos);
    void copyToRemote();
    void setToClipboard();
    void removeCopyPaste();
    void clearAll();
    void sendFiles();
    void sendFileFailedTips(QString name, QString address);
    void sendFeedBackTips(QString tips);
    void sendAction();
    void guideAction();
private:
    bool outgoing(const QString &text, bool ignoreLimits);
    bool outgoing(const QList<QUrl> urls, bool showError, bool ignoreLimits);
    bool outgoing(const QImage &image);
    void updateMyIP();
    void loadPassword();
    void loadConfiguration(bool withPassword);
private:
    Ui::LafdupWindow *ui;
    QSharedPointer<LafdupPeer> peer;
    CopyPasteModel *copyPasteModel;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu;
    bool started;
};

#endif
