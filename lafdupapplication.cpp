#include "lafdupapplication.h"

LafdupApplication::LafdupApplication(int &argc, char **argv)
    : QApplication(argc, argv)
{
}

LafdupApplication::~LafdupApplication()
{
    if (Q_NULLPTR != translation_ptr) {
        delete translation_ptr;
        translation_ptr = Q_NULLPTR;
    }
}

void LafdupApplication::translationLanguage()
{
    if (Q_NULLPTR != translation_ptr) {
        qApp->removeTranslator(translation_ptr);
        delete translation_ptr;
        translation_ptr = Q_NULLPTR;
    }
    QSettings settings("/lafdup.ini", QSettings::IniFormat);
    settings.beginGroup("Language");
    QString languageSetting = settings.value("languageValue").toString();
    settings.endGroup();
    translation_ptr = new QTranslator;
    if (languageSetting == "Chinese_CN") {
        if (translation_ptr->load(QLocale(), QLatin1String("lafdup"), QLatin1String("_"),
                                  QLatin1String(":/translations")))
            qApp->installTranslator(translation_ptr);
    } else {
        if (translation_ptr->load(QLocale(), QLatin1String("lafdup"), QLatin1String("_"),
                                  QLatin1String(":/translations")))
            qApp->removeTranslator(translation_ptr);
    }
}
