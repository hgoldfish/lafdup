#include "lafdupapplication.h"

LafdupApplication::LafdupApplication(int &argc, char **argv)
    : QApplication(argc, argv)
    , translationPtr(nullptr)
{
}

LafdupApplication::~LafdupApplication()
{
    delete translationPtr;
}

void LafdupApplication::translationLanguage()
{
    if (nullptr != translationPtr) {
        removeTranslator(translationPtr);
        delete translationPtr;
        translationPtr = nullptr;
    }
    QSettings settings("lafdup.ini", QSettings::IniFormat);
    settings.beginGroup("Language");
    QString languageSetting = settings.value("languageValue").toString();
    settings.endGroup();
    translationPtr = new QTranslator;
    languageStr = languageSetting;
    if (languageSetting == "Chinese_CN") {
        if (translationPtr->load(QLocale(), QLatin1String("lafdup"), QLatin1String("_"),
                                  QLatin1String(":/translations")))
            installTranslator(translationPtr);
    } else {
        if (translationPtr->load(QLocale(), QLatin1String("lafdup"), QLatin1String("_"),
                                  QLatin1String(":/translations")))
            removeTranslator(translationPtr);
    }
}
