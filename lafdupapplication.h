#ifndef LAFDUPAPPLICATION_H
#define LAFDUPAPPLICATION_H

#include <QtCore/qbytearray.h>
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
    static QByteArray linkLayerSalt();
    static QByteArray storePassword(const QString &password);
    static QByteArray loadPasswordMaterial(const QByteArray &stored);
    static bool hasStoredPassword(const QByteArray &stored);
    static bool isLegacyStoredPassword(const QByteArray &stored);
public:
    QString languageStr;
private:
    QTranslator *translationPtr;
};

#endif  // LAFDUPAPPLICATION_H
