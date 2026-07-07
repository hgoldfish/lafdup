#include "lafdupapplication.h"
#include "qtnetworkng.h"

using namespace qtng;

namespace {

const QByteArray storagePassword("lafdup");

QByteArray encryptPassword(const QByteArray &plain)
{
    Cipher cipher(Cipher::AES256, Cipher::CFB, Cipher::Encrypt);
    if (!cipher.setPassword(storagePassword, LafdupApplication::linkLayerSalt())) {
        return QByteArray();
    }
    QByteArray encrypted = cipher.addData(plain);
    encrypted += cipher.finalData();
    return encrypted;
}

QByteArray decryptPassword(const QByteArray &stored)
{
    Cipher cipher(Cipher::AES256, Cipher::CFB, Cipher::Decrypt);
    if (!cipher.setPassword(storagePassword, LafdupApplication::linkLayerSalt())) {
        return QByteArray();
    }
    QByteArray plain = cipher.addData(stored);
    plain += cipher.finalData();
    return plain;
}

}  // namespace

LafdupApplication::LafdupApplication(int &argc, char **argv)
    : QApplication(argc, argv)
    , translationPtr(nullptr)
{
}

LafdupApplication::~LafdupApplication()
{
    delete translationPtr;
}

QByteArray LafdupApplication::linkLayerSalt()
{
    return QByteArray("3.14159265358979323846");
}

QByteArray LafdupApplication::storePassword(const QString &password)
{
    const QByteArray plain = password.toUtf8();
    if (plain.isEmpty()) {
        return QByteArray();
    }
    return encryptPassword(plain);
}

QByteArray LafdupApplication::loadPasswordMaterial(const QByteArray &stored)
{
    if (stored.isEmpty()) {
        return QByteArray();
    }
    if (isLegacyStoredPassword(stored)) {
        return stored;
    }
    const QByteArray plain = decryptPassword(stored);
    if (!plain.isEmpty() && encryptPassword(plain) == stored) {
        return plain;
    }
    return stored;
}

bool LafdupApplication::hasStoredPassword(const QByteArray &stored)
{
    return !stored.isEmpty();
}

bool LafdupApplication::isLegacyStoredPassword(const QByteArray &stored)
{
    return stored.size() == 256;
}

void LafdupApplication::translationLanguage()
{
    if (nullptr != translationPtr) {
        removeTranslator(translationPtr);
        delete translationPtr;
        translationPtr = nullptr;
    }
    QSettings settings;
    QString languageSetting = settings.value("language").toString();
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
