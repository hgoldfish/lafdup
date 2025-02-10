#ifndef LAFDUP_WINDOW_P_H
#define LAFDUP_WINDOW_P_H

#include <QtCore/qdatetime.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtWidgets/qdialog.h>
#include "lafdup_window.h"

namespace Ui {
class PasswordDialog;
class ConfigureDialog;
}  // namespace Ui

class CopyPasteModel : public QAbstractListModel
{
    Q_OBJECT
public:
    CopyPasteModel();
public:
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
public:
    //    bool checkLastCopyPaste(CopyPaste::Direction direction, const QString &text) const;
    QModelIndex addCopyPaste(const CopyPaste &copyPaste);
    CopyPaste copyPasteAt(const QModelIndex &index) const;
    bool removeCopyPaste(const QModelIndex &index);
    void clearAll();
private:
    QList<CopyPaste> copyPasteList;
};

class PeerModel : public QAbstractListModel
{
public:
    void setPeers(const QStringList &peers);
    QStringList getPeers() const { return peers; }
    QModelIndex addPeer(const QString &peer);
    bool removePeer(const QModelIndex &index);
public:
    virtual int rowCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role) const override;
private:
    QStringList peers;
};

class PasswordDialog : public QDialog
{
    Q_OBJECT
public:
    PasswordDialog(QWidget *parent);
    virtual ~PasswordDialog() override;
public:
    virtual void accept() override;
private:
    Ui::PasswordDialog *ui;
};

class ConfigureDialog : public QDialog
{
    Q_OBJECT
public:
    ConfigureDialog(QWidget *parent);
    virtual ~ConfigureDialog() override;
public:
    virtual void accept() override;
    bool isPasswordChanged();
private slots:
    void onCacheDirectoryChanged(const QString &text);
    void selectCacheDirectory();
    void onDeleteFilesChanged(bool checked);
    void onSendFilesChanged(bool checked);
    void onOnlySendSmallFileChanged(bool checked);
    void removeSelectedPeer();
    void addPeer();
    void onChangelanguage();
private:
    void loadSettings();
private:
    Ui::ConfigureDialog *ui;
    PeerModel *peerModel;

};

#endif
