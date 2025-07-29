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
    void showContextMenu(const QPoint &pos);
    void copyToRemote();
    void setToClipboard();
    void removeCopyPaste();
    void clearAll();
    void sendFiles();
    void sendFeedBackTips(QString tips);
    void sendAction();
    void saveTextToLocal();
    void saveFilesToLocal();
    void savaImageToLocal();
    void setWindowTop(int state);
public slots:
    void onClipboardChanged();
private:
    bool outgoing(const CopyPaste &copyPaste);
    bool outgoing(const QString &text, bool ignoreLimits);
    bool outgoing(const QList<QUrl> urls, bool showError, bool ignoreLimits);
    bool isExcelDataCopied(const QMimeData *mimeData);
    bool copyFolder(const QString &fromDir, const QString &toDir);
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
