#include <QtCore/qcommandlineparser.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstylefactory.h>
#include "lafdup_window.h"
#include "qtnetworkng.h"

int main(int argc, char **argv)
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling, true);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps, true);

    QApplication app(argc, argv);
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

    LafdupWindow w;
    if (!parser.isSet(minimizedOption)) {
        w.showAndGetFocus();
    }

    return qtng::startQtLoop();
}
