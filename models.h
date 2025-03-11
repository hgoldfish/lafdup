#ifndef LAFDUP_MODELS_H
#define LAFDUP_MODELS_H

#include <QtCore/qstring.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qmetaobject.h>
#include <QtCore/qsharedpointer.h>

const QString TextType = "text/plain";
const QString BinaryType = "application/octet-stream";
const QString ImageType = "image/png";
const QString CompType = "comp";

struct CopyPaste
{
public:
    enum Direction { Incoming, Outgoing };
public:
    CopyPaste(const Direction &direction, const QDateTime &timestamp, const QString &text);
    CopyPaste();
public:
    Direction direction;
    QDateTime timestamp;
    QString mimeType;
    QString text;
    QByteArray image;
    QStringList files;
    bool ignoreLimits;
public:
    bool isText() const { return mimeType == TextType; }
    bool isFile() const { return mimeType == BinaryType; }
    bool isImage() const { return mimeType == ImageType; }
    bool isComp() const { return mimeType == CompType; }
public:
    QVariantMap saveState();
    bool restoreState(const QVariantMap &state);
    static QString lafrpcKey() { return "lafdup.CopyPaste"; }
};
Q_DECLARE_METATYPE(CopyPaste);
Q_DECLARE_METATYPE(QSharedPointer<CopyPaste>);

#endif
