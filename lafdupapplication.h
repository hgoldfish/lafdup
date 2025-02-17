#ifndef LAFDUPAPPLICATION_H
#define LAFDUPAPPLICATION_H

#include <QtCore/qobject.h>
#include <QtCore/qsettings.h>
#include <QtCore/qtranslator.h>
#include <QtCore//qlocale.h>
#include <QtWidgets/qapplication.h>

#define lpp static_cast<LafdupApplication *>(QCoreApplication::instance())

class LafdupApplication : public QApplication
{
    Q_OBJECT
public:
    LafdupApplication(int &argc, char **argv);
    ~LafdupApplication();
    void translationLanguage();
public:
    QString languageStr;
private:
    QTranslator *translationPtr = nullptr;
};

#endif  // LAFDUPAPPLICATION_H
