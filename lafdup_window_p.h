#ifndef LAFDUP_WINDOW_P_H
#define LAFDUP_WINDOW_P_H

#include <QtCore/qdatetime.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtWidgets/qdialog.h>


namespace Ui {
class PasswordDialog;
class ManagePeersDialog;
}

enum CopyPasteType
{
    Incoming, Outgoing
};


class CopyPaste
{
public:
    CopyPaste(CopyPasteType type, QDateTime timestamp, QString text)
        :type(type), timestamp(timestamp), text(text) {}
    CopyPaste() {}
public:
    CopyPasteType type;
    QDateTime timestamp;
    QString text;
};


class CopyPasteModel: public QAbstractListModel
{
public:
    CopyPasteModel();
public:
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
public:
    bool checkLastCopyPaste(CopyPasteType type, const QString &text) const;
    QModelIndex addCopyPaste(CopyPasteType type, const QDateTime &timestamp, const QString &text);
    CopyPaste copyPasteAt(const QModelIndex &index) const;
private:
    QList<CopyPaste> copyPasteList;
};


class PeerModel: public QAbstractListModel
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


class PasswordDialog: public QDialog
{
public:
    PasswordDialog(QWidget *parent);
    virtual ~PasswordDialog() override;
public:
    virtual void accept() override;
private:
    Ui::PasswordDialog *ui;
};


class ManagePeersDialog: public QDialog
{
    Q_OBJECT
public:
    ManagePeersDialog(QWidget *parent);
    virtual ~ManagePeersDialog() override;
public:
    virtual void accept() override;
private slots:
    void removeSelectedPeer();
    void addPeer();
private:
    Ui::ManagePeersDialog *ui;
    PeerModel *peerModel;
};


#endif
