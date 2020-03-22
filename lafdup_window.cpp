#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtimer.h>
#include <QtGui/qevent.h>
#include <QtGui/qclipboard.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qdesktopwidget.h>
#include "lafdup_window.h"
#include "lafdup_window_p.h"
#include "ui_main.h"
#include "ui_password.h"
#include "ui_peers.h"


using namespace qtng;


CopyPasteModel::CopyPasteModel()
{

}


int CopyPasteModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    } else {
        return copyPasteList.size();
    }
}


QVariant CopyPasteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    const CopyPaste &copyPaste = copyPasteList.at(index.row());
    if (role == Qt::DisplayRole) {
        const QString &cleanedText = copyPaste.text.trimmed().left(128).replace("\n", "").replace("\b*", " ").left(32);
        const QString &text = QStringLiteral("%2\n%1")
                .arg(copyPaste.timestamp.time().toString(Qt::ISODate))
                .arg(cleanedText.isEmpty() ? tr("<Spaces>") : cleanedText);
        return text;
    } else if (role == Qt::DecorationRole) {
        if (copyPaste.type == CopyPasteType::Incoming) {
            return QIcon(":/images/ic_file_download_black_48dp.png").pixmap(QSize(32, 32));
        } else {
            return QIcon(":/images/ic_file_upload_black_48dp.png").pixmap(QSize(32, 32));;
        }
    }

    return QVariant();
}


bool CopyPasteModel::checkLastCopyPaste(CopyPasteType type, const QString &text) const
{
    if (copyPasteList.isEmpty()) {
        return false;
    }
    const CopyPaste &action = copyPasteList.first();
    if (action.type != type) {
        return false;
    }
    return action.text == text;
}


QModelIndex CopyPasteModel::addCopyPaste(CopyPasteType type, const QDateTime &timestamp, const QString &text)
{
    for (const CopyPaste &action: copyPasteList) {
        if (action.type == type && action.timestamp == timestamp && action.text == text) {
            return QModelIndex();
        }
    }
    beginInsertRows(QModelIndex(), 0, 0);
    copyPasteList.prepend(CopyPaste(type, timestamp, text));
    endInsertRows();
    return createIndex(0, 0);
}


CopyPaste CopyPasteModel::copyPasteAt(const QModelIndex &index) const
{
    if (index.isValid()) {
        return copyPasteList.at(index.row());
    }
    return CopyPaste();
}


bool CopyPasteModel::removeCopyPaste(const QModelIndex &index)
{
    if (index.isValid()) {
        beginRemoveRows(QModelIndex(), index.row(), index.row());
        copyPasteList.removeAt(index.row());
        endRemoveRows();
        return true;
    }
    return false;
}


void CopyPasteModel::clearAll()
{
    if (!copyPasteList.isEmpty()) {
        beginResetModel();
        copyPasteList.clear();
        endResetModel();
    }
}


class CtrlEnterListener: public QObject
{
public:
    CtrlEnterListener(LafdupWindow *mainWindow)
        :QObject(mainWindow), mainWindow(mainWindow) {}
public:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    LafdupWindow *mainWindow;
};


bool CtrlEnterListener::eventFilter(QObject *, QEvent *event)
{
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent*>(event);
    if (keyEvent && keyEvent->type() == QEvent::KeyPress) {
        if ((keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return) && keyEvent->modifiers() & Qt::ControlModifier) {
            mainWindow->sendContent();
            return true;
        }
    }
    return false;
}

LafdupWindow::LafdupWindow()
    : ui(new Ui::LafdupWindow())
    , peer(new LafdupPeer())
    , copyPasteModel(new CopyPasteModel())
    , trayIcon(new QSystemTrayIcon())
{
    ui->setupUi(this);
    ui->txtContent->installEventFilter(new CtrlEnterListener(this));

    trayMenu = new QMenu();
    trayMenu->addAction(ui->actionShow);
    trayMenu->addAction(ui->actionExit);
    trayIcon->setIcon(QIcon(":/images/bluefish.png"));
    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    ui->lstCopyPaste->setModel(copyPasteModel);
    ui->lstCopyPaste->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
    connect(ui->lstCopyPaste, SIGNAL(customContextMenuRequested(QPoint)), SLOT(showContextMenu(QPoint)));
    connect(ui->lstCopyPaste, SIGNAL(activated(QModelIndex)), SLOT(setToClipboard()));

    connect(ui->btnManageKnownPeer, SIGNAL(clicked(bool)), SLOT(managePeers()));
    connect(ui->btnSend, SIGNAL(clicked(bool)), SLOT(sendContent()));
    connect(ui->btnSetPassword, SIGNAL(clicked(bool)), SLOT(setPassword()));
    connect(peer.data(), SIGNAL(incoming(QDateTime,QString)), SLOT(updateClipboard(QDateTime,QString)));
    connect(peer.data(), SIGNAL(stateChanged(bool)), SLOT(onPeerStateChanged(bool)));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(showAndGetFocus()));
    connect(ui->actionShow, SIGNAL(triggered(bool)), SLOT(showAndGetFocus()));
    connect(ui->actionExit, SIGNAL(triggered(bool)), QCoreApplication::instance(), SLOT(quit()));
    connect(ui->actionCopyToRemote, SIGNAL(triggered(bool)), SLOT(copyToRemote()));
    connect(ui->actionSetToClipboard, SIGNAL(triggered(bool)), SLOT(setToClipboard()));
    connect(ui->actionRemove, SIGNAL(triggered(bool)), SLOT(removeCopyPaste()));
    connect(ui->actionClearAll, SIGNAL(triggered(bool)), SLOT(clearAll()));

    const QClipboard *clipboard = QApplication::clipboard();
    connect(clipboard, SIGNAL(dataChanged()), SLOT(onClipboardChanged()));

    loadPassword();
    loadKnownPeers();
    started = peer->start();
}


LafdupWindow::~LafdupWindow()
{
    delete trayMenu;
    delete trayIcon;
    delete ui;
    delete copyPasteModel;
}


void LafdupWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (started) {
        updateMyIP();
    } else {
        ui->lblInformation->setText(tr("Can not bind to port %1. Will not synchronize from other phone/pc.").arg(peer->getPort()));
    }
    ui->txtContent->setFocus();
}


void LafdupWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    QWindowStateChangeEvent *wsce = dynamic_cast<QWindowStateChangeEvent*>(event);
    if (wsce) {
        if (!(wsce->oldState() & Qt::WindowMinimized) && (windowState() & Qt::WindowMinimized)) {
            QTimer::singleShot(0, this, SLOT(hide()));
            trayIcon->showMessage(windowTitle(), tr("Sync Clipboard is minimized to tray icon."));
        }
    }
}


void LafdupWindow::closeEvent(QCloseEvent *event)
{
    QWidget::closeEvent(event);
    QCoreApplication::instance()->quit();
}


void LafdupWindow::sendContent()
{
    const QString& text = ui->txtContent->toPlainText();
    if (text.isEmpty()) {
        // may I support sending empty content for clearing clipboard?
        QMessageBox::information(this, windowTitle(), tr("Can not send empty content."));
        return;
    }

    const QDateTime &now = QDateTime::currentDateTime();
    bool ok = peer->outgoing(now, text);
    if (!ok) {
        QMessageBox::information(this, windowTitle(), tr("Can not send content."));
        return;
    }
    const QModelIndex &current = copyPasteModel->addCopyPaste(CopyPasteType::Outgoing, now, text);
    ui->lstCopyPaste->setCurrentIndex(current);
    ui->txtContent->clear();
    ui->txtContent->setFocus();
}


void LafdupWindow::onClipboardChanged()
{
    const QClipboard *clipboard = QApplication::clipboard();
    const QString &text = clipboard->text();
    if (text.isEmpty()) {
        return;
    }
    bool found = copyPasteModel->checkLastCopyPaste(CopyPasteType::Incoming, text);
    if (found) {
        return;
    }
    const QDateTime &now = QDateTime::currentDateTime();
    bool ok = peer->outgoing(now, text);
    if (!ok) {
        //QMessageBox::information(this, windowTitle(), tr("Can not send content."));
        return;
    }
    const QModelIndex &current = copyPasteModel->addCopyPaste(CopyPasteType::Outgoing, now, text);
    ui->lstCopyPaste->setCurrentIndex(current);
    ui->txtContent->clear();
    ui->txtContent->setFocus();
}


void LafdupWindow::updateClipboard(const QDateTime &timestamp, const QString &text)
{
    const QModelIndex &current = copyPasteModel->addCopyPaste(CopyPasteType::Incoming, timestamp, text);
    if (current.isValid()) {
        ui->lstCopyPaste->setCurrentIndex(current);
        QApplication::clipboard()->setText(text);
    }
}


void LafdupWindow::showContextMenu(const QPoint &)
{
    const QModelIndex &current = ui->lstCopyPaste->currentIndex();
    if (!current.isValid()) {
        return;
    }
    QMenu menu;
    menu.addAction(ui->actionSetToClipboard);
    menu.addAction(ui->actionCopyToRemote);
    menu.addSeparator();
    menu.addAction(ui->actionRemove);
    menu.addAction(ui->actionClearAll);
    menu.exec(QCursor::pos());
}


void LafdupWindow::copyToRemote()
{
    const QModelIndex &current = ui->lstCopyPaste->currentIndex();
    if (!current.isValid()) {
        return;
    }
    const CopyPaste &copyPaste = copyPasteModel->copyPasteAt(current);
    if (copyPaste.text.isEmpty()) {
        return;
    }

    const QDateTime &now = QDateTime::currentDateTime();
    bool ok = peer->outgoing(now, copyPaste.text);
    if (!ok) {
        //QMessageBox::information(this, windowTitle(), tr("Can not send content."));
        return;
    }
    const QModelIndex &t = copyPasteModel->addCopyPaste(CopyPasteType::Outgoing, now, copyPaste.text);
    ui->lstCopyPaste->setCurrentIndex(t);
}


void LafdupWindow::setToClipboard()
{
    const QModelIndex &current = ui->lstCopyPaste->currentIndex();
    if (!current.isValid()) {
        return;
    }
    const CopyPaste &copyPaste = copyPasteModel->copyPasteAt(current);
    if (!copyPaste.text.isEmpty()) {
        const QClipboard *clipboard = QApplication::clipboard();
        disconnect(clipboard, SIGNAL(dataChanged()), this, SLOT(onClipboardChanged()));
        QApplication::clipboard()->setText(copyPaste.text);
        connect(clipboard, SIGNAL(dataChanged()), SLOT(onClipboardChanged()));
    }
}


void LafdupWindow::removeCopyPaste()
{
    const QModelIndex &current = ui->lstCopyPaste->currentIndex();
    if (!current.isValid()) {
        return;
    }
    copyPasteModel->removeCopyPaste(current);
}


void LafdupWindow::clearAll()
{
    copyPasteModel->clearAll();
}


void LafdupWindow::onPeerStateChanged(bool ok)
{
    if (!ok) {
        qDebug() << "peer is down.";
    }
}


void LafdupWindow::managePeers()
{
    ManagePeersDialog d(this);
    if (d.exec() == QDialog::Accepted) {
        loadKnownPeers();
    }
}


void LafdupWindow::setPassword()
{
    PasswordDialog d(this);
    if (d.exec() == QDialog::Accepted) {
        loadPassword();
    }
}


void LafdupWindow::useOldContent(const QModelIndex &current)
{
    if (!current.isValid()) {
        return;
    }
}


void LafdupWindow::updateMyIP()
{
    QString text = tr("My IP Addresses:");
    text.append("\n");
    for (const QString &ip: peer->getAllBoundAddresses()) {
        text.append(ip);
        text.append("\n");
    }
    ui->lblInformation->setText(text);
}


void LafdupWindow::loadPassword()
{
    QSettings settings;
    const QString &password = settings.value("password").toString();
    if (password.isEmpty()) {
        setPassword();
    } else {
        QSharedPointer<Cipher> cipher(new Cipher(Cipher::AES256, Cipher::CFB, Cipher::Encrypt));
        const QByteArray &salt = "31415926535";
        cipher->setPassword(password.toUtf8(), salt, MessageDigest::Sha256, 10000);
        if (cipher->isValid()) {
            peer->setCipher(cipher);
        } else {
            QMessageBox::information(this, windowTitle(), tr("There is some error on cipher, data is not protected."));
        }
    }
}


void LafdupWindow::loadKnownPeers()
{
    QSettings settings;
    const QStringList &knownPeers = settings.value("known_peers").toStringList();
    if (!knownPeers.isEmpty()) {
        QList<QHostAddress> addresses;
        for (int i = 0; i < knownPeers.size(); ++i) {
            QHostAddress addr(knownPeers.at(i));
            if (!addr.isNull()) {
                addresses.append(addr);
            }
        }
        peer->setKnownPeers(addresses);
    }
}


void moveToCenter(QWidget * const widget)
{
    QRect r = widget->geometry();
    r.moveCenter(QApplication::desktop()->screenGeometry(widget).center());
    widget->setGeometry(r);
}


void LafdupWindow::showAndGetFocus()
{
    if (windowState() & Qt::WindowMinimized) {
        setWindowState(windowState() ^ Qt::WindowMinimized);
    }
    show();
    activateWindow();
    raise();
    moveToCenter(this);
}



PasswordDialog::PasswordDialog(QWidget *parent)
    :QDialog(parent), ui(new Ui::PasswordDialog)
{
    ui->setupUi(this);
}


PasswordDialog::~PasswordDialog()
{
    delete ui;
}


void PasswordDialog::accept()
{
    const QString &password = ui->txtPassword->text();
    if (password.isEmpty()) {
        QMessageBox::information(this, windowTitle(), tr("Password is empty."));
        ui->txtPassword->setFocus();
        return;
    }
    QSettings settings;
    settings.setValue("password", password);
    QDialog::accept();
}



void PeerModel::setPeers(const QStringList &peers)
{
    beginResetModel();
    this->peers = peers;
    endResetModel();
}


QModelIndex PeerModel::addPeer(const QString &peer)
{
    for (int i = 0; i < peers.size(); ++i) {
        if (peers.at(i) == peer) {
            return createIndex(i, 0);
        }
    }
    beginInsertRows(QModelIndex(), peers.size(), peers.size());
    peers.append(peer);
    endInsertRows();
    return createIndex(peer.size() - 1, 0);
}


bool PeerModel::removePeer(const QModelIndex &index)
{
    if (index.isValid()) {
        beginRemoveRows(QModelIndex(), index.row(), index.row());
        peers.removeAt(index.row());
        endRemoveRows();
        return true;
    } else {
        return false;
    }
}


int PeerModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    } else {
        return peers.size();
    }
}


QVariant PeerModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid() && role == Qt::DisplayRole) {
        return peers.at(index.row());
    }
    return QVariant();
}


ManagePeersDialog::ManagePeersDialog(QWidget *parent)
    :QDialog(parent), ui(new Ui::ManagePeersDialog), peerModel(new PeerModel)
{
    ui->setupUi(this);
    connect(ui->btnAdd, SIGNAL(clicked(bool)), SLOT(addPeer()));
    connect(ui->btnRemove, SIGNAL(clicked(bool)), SLOT(removeSelectedPeer()));
    QSettings settings;
    const QStringList &knownPeers = settings.value("known_peers").toStringList();
    peerModel->setPeers(knownPeers);
    ui->lstIPs->setModel(peerModel);
}


ManagePeersDialog::~ManagePeersDialog()
{
    delete ui;
    delete peerModel;
}


void ManagePeersDialog::accept()
{
    const QStringList &peers = peerModel->getPeers();
    QSettings settings;
    settings.setValue("known_peers", peers);
    QDialog::accept();
}


void ManagePeersDialog::addPeer()
{
    const QString &ip = ui->txtIP->text();
    QHostAddress addr(ip);
    if (addr.isNull()) {
        QMessageBox::information(this, windowTitle(), tr("%1 is not a valid IP.").arg(ip));
        ui->txtIP->selectAll();
        ui->txtIP->setFocus();
        return;
    }
    const QModelIndex &current = peerModel->addPeer(addr.toString());
    if (current.isValid()) {
        ui->lstIPs->setCurrentIndex(current);
    }
}


void ManagePeersDialog::removeSelectedPeer()
{
    const QModelIndex &current = ui->lstIPs->currentIndex();
    if (current.isValid()) {
        peerModel->removePeer(current);
    }
}
