#include "lafdup_window_p.h"
#include "lafdupapplication.h"
#include "ui_configure.h"
#include "ui_main.h"
#include "ui_password.h"
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qbuffer.h>
#include <QtCore/qmimedata.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtimer.h>
#include <QtGui/qclipboard.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qdesktopwidget.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qmenu.h>

using namespace qtng;

CopyPasteModel::CopyPasteModel() { }

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
        if (copyPaste.isText()) {
            const QString &cleanedText =
                    copyPaste.text.trimmed().left(128).replace("\n", "").replace("\b*", " ").left(32);
            const QString &text = QStringLiteral("%2\n%1").arg(copyPaste.timestamp.time().toString(Qt::ISODate),
                                                               cleanedText.isEmpty() ? tr("<Spaces>") : cleanedText);
            return text;
        } else if (copyPaste.isFile()) {
            QStringList fileNames;
            for (const QString &path : copyPaste.files) {
                QFileInfo fileInfo(path);
                fileNames.append(fileInfo.fileName());
            }
            return fileNames.join(",");
        } else if (copyPaste.isImage()) {
            return "Image";
        }
    } else if (role == Qt::DecorationRole) {
        // TODO support dark theme.
        if (copyPaste.direction == CopyPaste::Incoming) {
            return QIcon(":/images/ic_file_download_black_48dp.png").pixmap(QSize(32, 32));
        } else {
            return QIcon(":/images/ic_file_upload_black_48dp.png").pixmap(QSize(32, 32));
            ;
        }
    } else if (role == Qt::ToolTipRole) {
        if (copyPaste.isText()) {
            const QString &text =
                    QStringLiteral("%2\n%1").arg(copyPaste.timestamp.time().toString(Qt::ISODate),
                                                 copyPaste.text.isEmpty() ? tr("<Spaces>") : copyPaste.text);
            return text;
        } else if (copyPaste.isFile()) {
            QStringList fileNames;
            for (const QString &path : copyPaste.files) {
                QFileInfo fileInfo(path);
                fileNames.append(fileInfo.absoluteFilePath());
            }
            return fileNames.join("\n");
        } else if (copyPaste.isImage()) {
            return "Image";
        }
    }

    return QVariant();
}

// bool CopyPasteModel::checkLastCopyPaste(CopyPaste::Direction direction, const QString &text) const
//{
//     if (copyPasteList.isEmpty()) {
//         return false;
//     }
//     const CopyPaste &action = copyPasteList.first();
//     if (action.direction != direction) {
//         return false;
//     }
//     return action.textOrPath == text;
// }

QModelIndex CopyPasteModel::addCopyPaste(const CopyPaste &copyPaste)
{
    const auto &list = copyPasteList;
    for (const CopyPaste &old : list) {
        if (old.direction == copyPaste.direction && old.timestamp == copyPaste.timestamp) {
            return QModelIndex();
        }
    }
    beginInsertRows(QModelIndex(), 0, 0);
    copyPasteList.prepend(copyPaste);
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

class CtrlEnterListener : public QObject
{
public:
    CtrlEnterListener(LafdupWindow *mainWindow)
        : QObject(mainWindow)
        , mainWindow(mainWindow)
    {
    }
public:
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    LafdupWindow *mainWindow;
};

bool CtrlEnterListener::eventFilter(QObject *, QEvent *event)
{
    QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);
    if (keyEvent && keyEvent->type() == QEvent::KeyPress) {
        if ((keyEvent->key() == Qt::Key_Enter || keyEvent->key() == Qt::Key_Return)
            && keyEvent->modifiers() & Qt::ControlModifier) {
            mainWindow->sendContent();
            return true;
        }
    }
    return false;
}

LafdupWindow::LafdupWindow()
    : ui(new Ui::LafdupWindow())
    , peer(new LafdupPeer(lafrpc::createUuid(), LafdupPeer::getDefaultPort()))
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

    connect(ui->btnConfigure, SIGNAL(clicked()), SLOT(configure()));
    connect(ui->btnSendText, SIGNAL(clicked(bool)), SLOT(sendContent()));
    connect(ui->btnSendFiles, SIGNAL(clicked(bool)), SLOT(sendFiles()));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), SLOT(showAndGetFocus()));
    connect(ui->actionShow, SIGNAL(triggered(bool)), SLOT(showAndGetFocus()));
    connect(ui->actionExit, SIGNAL(triggered(bool)), QCoreApplication::instance(), SLOT(quit()));
    connect(ui->actionCopyToRemote, SIGNAL(triggered(bool)), SLOT(copyToRemote()));
    connect(ui->actionSetToClipboard, SIGNAL(triggered(bool)), SLOT(setToClipboard()));
    connect(ui->actionRemove, SIGNAL(triggered(bool)), SLOT(removeCopyPaste()));
    connect(ui->actionClearAll, SIGNAL(triggered(bool)), SLOT(clearAll()));

    QClipboard *clipboard = QApplication::clipboard();
    connect(clipboard, SIGNAL(dataChanged()), SLOT(onClipboardChanged()));

    connect(peer.data(), &LafdupPeer::stateChanged, this, &LafdupWindow::onPeerStateChanged);
    connect(peer.data(), &LafdupPeer::incoming, this, &LafdupWindow::updateClipboard);
    connect(peer.data(), &LafdupPeer::sendFileFailed, this, &LafdupWindow::sendFileFailedTips);

    loadConfiguration(true);
    started = peer->start();
    setAcceptDrops(true);
    ui->txtContent->setAcceptDrops(false);
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
        ui->lblInformation->setText(
                tr("Can not bind to port %1. Will not synchronize from other phone/pc.").arg(peer->getPort()));
    }
    ui->txtContent->setFocus();
}

void LafdupWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    QWindowStateChangeEvent *wsce = dynamic_cast<QWindowStateChangeEvent *>(event);
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
    QTimer::singleShot(0, QCoreApplication::instance(), SLOT(quit()));
}

void LafdupWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        outgoing(mimeData->urls(), true, true);
        event->acceptProposedAction();
    }
}

void LafdupWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        event->acceptProposedAction();
    }
}

bool LafdupWindow::event(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    return QWidget::event(e);
}

bool LafdupWindow::outgoing(const QString &text, bool ignoreLimits)
{
    const QDateTime &now = QDateTime::currentDateTime();
    CopyPaste copyPaste;
    copyPaste.direction = CopyPaste::Outgoing;
    copyPaste.mimeType = TextType;
    copyPaste.text = text;
    copyPaste.timestamp = now;
    copyPaste.ignoreLimits = ignoreLimits;
    peer->outgoing(copyPaste);
    return true;
}

bool LafdupWindow::outgoing(const QList<QUrl> urls, bool showError, bool ignoreLimits)
{
    if (urls.isEmpty()) {
        return false;
    }
    bool notLocal = false;
    bool fileMoved = false;
    QStringList files;
    for (const QUrl &url : urls) {
        if (!url.isLocalFile()) {
            notLocal = true;
            break;
        } else if (!QFileInfo(url.toLocalFile()).isReadable()) {
            fileMoved = true;
            break;
        } else {
            files.append(url.toLocalFile());
        }
    }
    if (notLocal && showError) {
        QMessageBox::information(this, windowTitle(), tr("can not send urls as files. this is a programming bug."));
        return false;
    }
    if (fileMoved && showError) {
        QMessageBox::information(this, windowTitle(), tr("can not send files. some file is moved."));
        return false;
    }
    CopyPaste copyPaste;
    copyPaste.timestamp = QDateTime::currentDateTime();
    copyPaste.direction = CopyPaste::Outgoing;
    copyPaste.mimeType = BinaryType;
    copyPaste.files = files;
    copyPaste.ignoreLimits = ignoreLimits;
    peer->outgoing(copyPaste);
    return true;
}

static inline QByteArray saveImage(const QImage &image)
{
    QBuffer buf;
    buf.open(QIODevice::WriteOnly);
    if (!image.save(&buf, "png")) {
        return QByteArray();
    } else {
        return buf.data();
    }
}

static inline QImage loadImage(QByteArray data)
{
    QBuffer buf(&data);
    buf.open(QIODevice::ReadOnly);
    QImage image;
    image.load(&buf, "png");
    return image;
}

bool LafdupWindow::outgoing(const QImage &image)
{
    if (image.isNull()) {
        return false;
    }
    CopyPaste copyPaste;
    copyPaste.timestamp = QDateTime::currentDateTime();
    copyPaste.direction = CopyPaste::Outgoing;
    copyPaste.mimeType = ImageType;
    copyPaste.image = saveImage(image);
    copyPaste.ignoreLimits = false;
    peer->outgoing(copyPaste);
    return true;
}

void LafdupWindow::sendContent()
{
    const QString &text = ui->txtContent->toPlainText();
    if (text.isEmpty()) {
        // may I support sending empty content for clearing clipboard?
        QMessageBox::information(this, windowTitle(), tr("Can not send empty content."));
        return;
    }
    if (outgoing(text, true)) {
        ui->txtContent->clear();
        ui->txtContent->setFocus();
    }
}

void LafdupWindow::sendFiles()
{
    const QList<QUrl> &fileUrls = QFileDialog::getOpenFileUrls(this, tr("select files to send."));
    if (fileUrls.isEmpty()) {
        return;
    }
    outgoing(fileUrls, true, true);
}

void LafdupWindow::sendFileFailedTips(QString name, QString address)
{
    QString tipStr =
            tr("send  %1 to %2 failed \n The peer party may not set the path for receiving files").arg(name, address);
    MessageTips::showMessageTips(tipStr, this);
}

void LafdupWindow::onClipboardChanged()
{
    const QClipboard *clipboard = QApplication::clipboard();
    const QMimeData *mimeData = clipboard->mimeData();
    if (mimeData->hasImage()) {
        outgoing(mimeData->imageData().value<QImage>());
    } else if (mimeData->hasUrls()) {
        outgoing(mimeData->urls(), false, false);
    } else if (mimeData->hasText()) {
        outgoing(mimeData->text(), false);
    }
}

struct DisableSyncClipboard
{
    DisableSyncClipboard(QClipboard *clipboard, LafdupWindow *window);
    ~DisableSyncClipboard();
    QClipboard *clipboard;
    LafdupWindow *window;
};

DisableSyncClipboard::DisableSyncClipboard(QClipboard *clipboard, LafdupWindow *window)
    : clipboard(clipboard)
    , window(window)
{
    QObject::disconnect(clipboard, SIGNAL(dataChanged()), window, nullptr);
}

DisableSyncClipboard::~DisableSyncClipboard()
{
    QPointer<LafdupWindow> windowRef(window);
    QTimer::singleShot(100, windowRef, [windowRef] {
        if (windowRef.isNull()) {
            return;
        }
        QClipboard *clipboard = QApplication::clipboard();
        QObject::connect(clipboard, SIGNAL(dataChanged()), windowRef.data(), SLOT(onClipboardChanged()));
    });
}

static bool updateClipboard(const CopyPaste &copyPaste, LafdupWindow *window)
{
    QClipboard *clipboard = QApplication::clipboard();
    DisableSyncClipboard _(clipboard, window);
    if (copyPaste.isFile()) {
        QList<QUrl> urls;
        for (const QString &file : copyPaste.files) {
            urls.append(QUrl::fromLocalFile(file));
        }
        QMimeData *mimeData = new QMimeData();
        mimeData->setUrls(urls);
        clipboard->setMimeData(mimeData);
    } else if (copyPaste.isImage()) {
        const QImage &image = loadImage(copyPaste.image);
        if (image.isNull()) {
            return false;
        }
        clipboard->setImage(image);
    } else if (copyPaste.isText()) {
        clipboard->setText(copyPaste.text);
    } else {
        return false;
    }
    return true;
}

void LafdupWindow::updateClipboard(const CopyPaste &copyPaste)
{
    const QModelIndex &current = copyPasteModel->addCopyPaste(copyPaste);
    if (current.isValid()) {
        ui->lstCopyPaste->setCurrentIndex(current);
        if (copyPaste.direction == CopyPaste::Incoming) {
            ::updateClipboard(copyPaste, this);
        }
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
    CopyPaste copyPaste = copyPasteModel->copyPasteAt(current);
    copyPaste.timestamp = QDateTime::currentDateTime();
    copyPaste.direction = CopyPaste::Outgoing;
    peer->outgoing(copyPaste);
}

void LafdupWindow::setToClipboard()
{
    const QModelIndex &current = ui->lstCopyPaste->currentIndex();
    if (!current.isValid()) {
        return;
    }
    const CopyPaste &copyPaste = copyPasteModel->copyPasteAt(current);
    ::updateClipboard(copyPaste, this);
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
        qDebug("peer is down.");
    }
}

void LafdupWindow::configure()
{
    ConfigureDialog d(this);
    if (d.exec() == QDialog::Accepted) {
        loadConfiguration(d.isPasswordChanged());
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
    const auto &list = peer->getAllBoundAddresses();
    for (const QString &ip : list) {
        text.append(ip);
        text.append("\n");
    }
    ui->lblInformation->setText(text);
}

void LafdupWindow::loadPassword()
{
    QSettings settings;
    const QByteArray &password = settings.value("password").toByteArray();
    if (password.isEmpty()) {
        setPassword();
    } else {
        peer->setPassword(password);
    }
}

void LafdupWindow::loadConfiguration(bool withPassword)
{
    bool ok;
    QSettings settings;
    const QStringList &knownPeers = settings.value("known_peers").toStringList();
    if (!knownPeers.isEmpty()) {
        QSet<QPair<HostAddress, quint16>> addresses;
        for (int i = 0; i < knownPeers.size(); ++i) {
            HostAddress addr(knownPeers.at(i));
            if (!addr.isNull()) {
                addresses.insert(qMakePair(addr, peer->getDefaultPort()));
            }
        }
        peer->setExtraKnownPeers(addresses);
    }
    if (withPassword) {
        const QByteArray &password = settings.value("password").toByteArray();
        if (!password.isEmpty()) {
            peer->setPassword(password);
        }
    }

    const QString &cacheDirPath = settings.value("cache_dir").toString();
    QFileInfo cacheDir(cacheDirPath);
    if (cacheDir.isWritable() && cacheDir.isDir()) {
        peer->setCacheDir(cacheDirPath);
        bool deleteFiles = settings.value("delete_files", true).toBool();
        if (deleteFiles) {
            int deleteFilesTime = settings.value("delete_files_time", 30).toInt(&ok);
            if (!ok) {
                deleteFilesTime = 30;
            }
            if (deleteFilesTime < 1) {
                deleteFilesTime = 1;
            }
            peer->setDeleteFilesTime(deleteFilesTime);
        } else {
            peer->setDeleteFilesTime(0);
        }
    }

    bool sendFiles = settings.value("send_files", true).toBool();
    if (sendFiles) {
        bool onlySendSmallFiles = settings.value("only_send_small_files", true).toBool();
        if (!onlySendSmallFiles) {
            peer->setSendFilesSize(float(INT_MAX));  // unlimited
        } else {
            float sendFilesSize = settings.value("send_files_size", 100.0).toFloat(&ok);
            if (!ok) {
                sendFilesSize = 100.0;
            }
            peer->setSendFilesSize(sendFilesSize);
        }
    } else {
        peer->setSendFilesSize(0.0);
    }

    bool ignorePassword = settings.value("ignore_password", false).toBool();
    peer->setIgnorePassword(ignorePassword);
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
    : QDialog(parent)
    , ui(new Ui::PasswordDialog)
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
    QByteArray salt("3.1415926535897932384626433");
    settings.setValue("password", qtng::PBKDF2_HMAC(256, password.toUtf8(), salt));
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

ConfigureDialog::ConfigureDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConfigureDialog)
    , peerModel(new PeerModel)
{
    ui->setupUi(this);
    moveToCenter(this);
    loadSettings();

    connect(ui->btnAdd, SIGNAL(clicked(bool)), SLOT(addPeer()));
    connect(ui->btnRemove, SIGNAL(clicked(bool)), SLOT(removeSelectedPeer()));
    connect(ui->txtCacheDirectory, &QLineEdit::textChanged, this, &ConfigureDialog::onCacheDirectoryChanged);
    connect(ui->btnSelectCacheDirectory, &QPushButton::clicked, this, &ConfigureDialog::selectCacheDirectory);
    connect(ui->chkDeleteFiles, &QCheckBox::stateChanged, this, &ConfigureDialog::onDeleteFilesChanged);
    connect(ui->chkSendFiles, &QCheckBox::stateChanged, this, &ConfigureDialog::onSendFilesChanged);
    connect(ui->chkOnlySendSmallFile, &QCheckBox::stateChanged, this, &ConfigureDialog::onOnlySendSmallFileChanged);
    connect(ui->btnChangeLanguage, &QPushButton::clicked, this, &ConfigureDialog::onChangelanguage);
}

ConfigureDialog::~ConfigureDialog()
{
    delete peerModel;
    delete ui;
}

void ConfigureDialog::accept()
{
    const QString &password = ui->txtPassword->text();

    const QStringList &peers = peerModel->getPeers();
    QSettings settings;
    settings.setValue("known_peers", peers);
    if (!password.isEmpty()) {
        settings.setValue("password", password);
    }
    settings.setValue("cache_dir", ui->txtCacheDirectory->text());
    settings.setValue("delete_files", ui->chkDeleteFiles->isChecked());
    settings.setValue("delete_files_time", ui->spinDeleteFilesTime->value());
    settings.setValue("send_files", ui->chkSendFiles->isChecked());
    settings.setValue("only_send_small_files", ui->chkOnlySendSmallFile->isChecked());
    settings.setValue("send_files_size", ui->spinSendFileSize->value());
    settings.setValue("ignore_password", ui->chkIgnorePassword->isChecked());
    QDialog::accept();
}

void ConfigureDialog::loadSettings()
{
    QSettings settings;
    const QStringList &knownPeers = settings.value("known_peers").toStringList();
    const QString &cacheDirPath = settings.value("cache_dir").toString();
    bool deleteFiles = settings.value("delete_files", true).toBool();
    bool ok;
    int deleteFilesTime = settings.value("delete_files_time", 30).toInt(&ok);
    if (!ok) {
        deleteFilesTime = 30;
    }
    if (deleteFilesTime < 1) {
        deleteFilesTime = 1;
    }
    bool sendFiles = settings.value("send_files", true).toBool();
    bool onlySendSmallFiles = settings.value("only_send_small_files", true).toBool();
    int sendFilesSize = settings.value("send_files_size", 100.0).toInt(&ok);
    if (!ok) {
        sendFilesSize = 100.0;
    }
    bool ignorePassword = settings.value("ignore_password", false).toBool();
    peerModel->setPeers(knownPeers);
    ui->lstIPs->setModel(peerModel);
    ui->txtCacheDirectory->setText(cacheDirPath);
    ui->chkDeleteFiles->setChecked(deleteFiles);
    ui->spinDeleteFilesTime->setValue(deleteFilesTime);
    ui->chkSendFiles->setChecked(sendFiles);
    ui->chkOnlySendSmallFile->setChecked(onlySendSmallFiles);
    ui->spinSendFileSize->setValue(sendFilesSize);
    if (!cacheDirPath.isEmpty()) {
        ui->chkDeleteFiles->setEnabled(true);
        ui->spinDeleteFilesTime->setEnabled(true);
    }
    if (!sendFiles) {
        ui->chkOnlySendSmallFile->setEnabled(false);
        ui->spinSendFileSize->setEnabled(false);
    }
    ui->chkIgnorePassword->setChecked(ignorePassword);
    if (lpp->languageStr == "Chinese_CN") {
        ui->cbBLanguage->setCurrentIndex(1);
    } else {
        ui->cbBLanguage->setCurrentIndex(0);
    }
}

bool ConfigureDialog::isPasswordChanged()
{
    return !ui->txtPassword->text().isEmpty();
}

void ConfigureDialog::onCacheDirectoryChanged(const QString &text)
{
    QFileInfo fileInfo(text);
    bool ok = fileInfo.isDir() && fileInfo.isWritable();

    ui->chkDeleteFiles->setEnabled(ok);
    ui->spinDeleteFilesTime->setEnabled(ok && ui->chkDeleteFiles->isChecked());
}

void ConfigureDialog::selectCacheDirectory()
{
    const QString &path = QFileDialog::getExistingDirectory(this, tr("Select Cache Directory to Receive Files."));
    if (path.isEmpty()) {
        return;
    }
    ui->txtCacheDirectory->setText(path);
}

void ConfigureDialog::onDeleteFilesChanged(bool checked)
{
    ui->spinDeleteFilesTime->setEnabled(checked);
}

void ConfigureDialog::onSendFilesChanged(bool checked)
{
    ui->chkOnlySendSmallFile->setEnabled(checked);
    if (!checked) {
        ui->spinSendFileSize->setEnabled(false);
    } else {
        ui->spinSendFileSize->setEnabled(ui->chkOnlySendSmallFile->isChecked());
    }
}

void ConfigureDialog::onOnlySendSmallFileChanged(bool checked)
{
    ui->spinSendFileSize->setEnabled(checked);
}

void ConfigureDialog::addPeer()
{
    const QString &ip = ui->txtIP->text();
    HostAddress addr(ip);
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

void ConfigureDialog::onChangelanguage()
{
    int ret = ui->cbBLanguage->currentIndex();
    QString language;
    switch (ret) {
    case 0:
        language = "English";
        break;
    case 1:
        language = "Chinese_CN";
        break;
    default:
        break;
    }
    QMessageBox::StandardButton result = QMessageBox::question(
            this, tr("changeLanguage"), tr("Whether or not to change the language used by the program"));
    if (result == QMessageBox::Yes) {
        QSettings setting;
        setting.setValue("language", language);
        lpp->translationLanguage();
        ui->retranslateUi(this);
    }
}

void ConfigureDialog::removeSelectedPeer()
{
    const QModelIndex &current = ui->lstIPs->currentIndex();
    if (current.isValid()) {
        peerModel->removePeer(current);
    }
}

MessageTips::MessageTips(const QString &str, QWidget *partent)
    : QWidget(partent)
    , showStr(str)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    this->setAttribute(Qt::WA_DeleteOnClose);
    this->setFixedHeight(50);
    QFontMetrics fontMetrics(lpp->font());
    int textWidth = fontMetrics.horizontalAdvance(showStr);
    if (textWidth > 200) {
        this->setFixedWidth(textWidth + 50);
    } else {
        this->setFixedWidth(200);
    }
}

MessageTips::~MessageTips()
{
    if (mtimer != nullptr) {
        delete mtimer;
        mtimer = nullptr;
    }
}

void MessageTips::prepare()
{
    this->setWindowOpacity(opacity);
    connect(this, SIGNAL(finished()), this, SLOT(onFinished()));
    mtimer = new QTimer(this);  // 隐藏的定时器
    mtimer->setTimerType(Qt::PreciseTimer);
    connect(mtimer, &QTimer::timeout, this, &MessageTips::opacityTimer);
    this->move(this->parentWidget()->x() + (this->parentWidget()->width()) / 2 - this->width() / 2,
               this->parentWidget()->y() + (this->parentWidget()->height()) / 2 - this->height() / 2);
    QTimer::singleShot(showTime, this, [this] { mtimer->start(closeTime); });  // 执行延时自动关闭
}

void MessageTips::setCloseTimeSpeed(int time, double speed)
{
    if (speed > 0 && speed <= 1) {
        closeSpeed = speed;
    }
    closeTime = time;
}

void MessageTips::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setBrush(QBrush(bgColor));  // 窗体的背景色
    painter.setPen(QPen(frameColor, frameSize));  // 窗体边框的颜色和笔画大小
    QRectF rect(0, 0, this->width(), this->height());
    painter.drawRoundedRect(rect, 10, 10);
    painter.setFont(lpp->font());
    painter.setPen(QPen(textColor, frameSize));
    painter.drawText(rect, Qt::AlignHCenter | Qt::AlignVCenter, showStr);
}

void MessageTips::opacityTimer()
{
    opacity = opacity - closeSpeed;
    if (opacity <= 0) {
        mtimer->stop();
        this->close();
        return;
    } else{
        this->setWindowOpacity(opacity);
	}
}

void MessageTips::timerDelayShutdown()
{
    mtimer->start(closeTime);
}

void MessageTips::showMessageTips(QString str, QWidget *parent)
{
    MessageTips *mMessageTips = new MessageTips(str, parent);
    mMessageTips->setShowTime(1500);
    mMessageTips->prepare();
    mMessageTips->show();
}

void MessageTips::setShowTime(int time)
{
    showTime = time;
}
