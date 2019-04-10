#ifndef LAFDUP_WINDOW_P_H
#define LAFDUP_WINDOW_P_H

#include <QtWidgets/qdialog.h>


namespace Ui {
class PasswordDialog;
class ManagePeersDialog;
}

enum ActionType
{
    Incoming, Outgoing
};


class Action
{
public:
    Action(ActionType type, QDateTime timestamp, QString text)
        :type(type), timestamp(timestamp), text(text) {}
    Action() {}
public:
    ActionType type;
    QDateTime timestamp;
    QString text;
};


class ActionModel: public QAbstractListModel
{
public:
    ActionModel();
public:
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;
public:
    bool checkLastAction(const QString &text) const;
    QModelIndex addAction(ActionType type, const QDateTime &timestamp, const QString &text);
private:
    QList<Action> actions;
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
    ~ManagePeersDialog();
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
