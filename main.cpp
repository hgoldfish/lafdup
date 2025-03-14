﻿#include <QtCore/qcommandlineparser.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstylefactory.h>
#include <QtCore/qsystemsemaphore.h>
#include <QtCore/qsharedmemory.h>
#include "lafdup_window.h"
#include "lafdupapplication.h"
#include "qtnetworkng.h"
#include "lafdup_window_p.h"


int main(int argc, char **argv)
{
    qSetMessagePattern("[%{time yyyyMMdd h:mm:ss.zzz}] - %{message}");
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    LafdupApplication app(argc, argv);
    app.setOrganizationDomain("lafdup.gigacores.com");
    app.setOrganizationName("GigaCores");
    app.setApplicationDisplayName("Sync Clipboard");
    app.setApplicationName("SyncClipboard");
    app.setApplicationVersion("1.0");
    app.setWindowIcon(QIcon(":/images/bluefish.png"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Keep clipboard synchroized to other PCs.");
    parser.addHelpOption();
    QCommandLineOption minimizedOption("m", "minimized after started.");
    parser.addOption(minimizedOption);
    parser.process(app);

#ifdef Q_OS_WIN
    QFont f = app.font();
    f.setFamily("微软雅黑");
    app.setFont(f);
    app.setStyle(QStyleFactory::create("fusion"));
#endif
    app.translationLanguage();

    QSystemSemaphore sema("systemSemaphore", 1, QSystemSemaphore::Open);
    sema.acquire();
    QSharedMemory mem("memory");
    if (!mem.create(1)) {
        QMessageBox::information(nullptr, QObject::tr("tips"),
                                 QObject::tr("The program is running, please exit first if necessary"));
        sema.release();
        return 0;
    }
    sema.release();

    QSettings *settings = new QSettings;
    if (settings->value("password").toString().isEmpty()) {
        GuideDialog guide;
        guide.show();
        int result = guide.exec();
        if (result != QDialog::Accepted) {
            return 0;
        }
    }

    LafdupWindow w;
    if (settings->value("isMinimized").toBool()) {
        w.showMinimized();
    } else {
        w.showAndGetFocus();
    }
    delete settings;
    return qtng::startQtLoop();
}
