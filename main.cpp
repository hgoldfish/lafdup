#include <QtWidgets/QApplication>
#include "lafdup_window.h"
#include "qtnetworkng.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setOrganizationDomain("com.gigacores.lafdup");
    app.setOrganizationName("GigaCores");
    app.setApplicationDisplayName("Sync Clipboard");
    app.setApplicationName("SyncClipboard");
    app.setApplicationVersion("1.0");
    app.setWindowIcon(QIcon(":/images/bluefish.png"));

#ifdef Q_OS_WIN
    QFont f = app.font();
    f.setFamily("微软雅黑");
    app.setFont(f);
#endif
    LafdupWindow w;
    w.showAndGetFocus();

    return qtng::startQtLoop();
}
