#ifndef LAFDUPAPPLICATION_H
#define LAFDUPAPPLICATION_H

#include <QApplication>
#include <QTranslator>
#include <QObject>
#include <QSettings>
#include <QLocale>
#define lpp static_cast<LafdupApplication *>(QCoreApplication::instance())
class LafdupApplication : public QApplication
{
    Q_OBJECT
public:
    LafdupApplication(int &argc, char **argv);
    ~LafdupApplication();
    void translationLanguage();

private:
    QTranslator *translation_ptr{ Q_NULLPTR };
};

#endif  // LAFDUPAPPLICATION_H
