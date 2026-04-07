#include "models.h"

CopyPaste::CopyPaste(const Direction &direction, const QDateTime &timestamp, const QString &text)
    : direction(direction)
    , timestamp(timestamp)
    , mimeType(TextType)
    , text(text)
{
}

CopyPaste::CopyPaste()
    : direction(Incoming)
{
}

QVariantMap CopyPaste::saveState()
{
    QVariantMap state;
    state.insert("timestamp", timestamp);
    state.insert("mime_type", mimeType);
    if (mimeType == TextType) {
        state.insert("text", text);
    } else if (mimeType == ImageType) {
        state.insert("image", image);
    } else if (mimeType == BinaryType) {
        state.insert("files", files);
    } else {
    }
    return state;
}

bool CopyPaste::restoreState(const QVariantMap &state)
{
    timestamp = state.value("timestamp").toDateTime();
    mimeType = state.value("mime_type").toString();
    text = state.value("text").toString();
    image = state.value("image").toByteArray();
    files = state.value("files").toStringList();
    if (!timestamp.isValid() || mimeType.isEmpty()) {
        return false;
    }
    if (mimeType == TextType) {
        return !text.isEmpty();
    } else if (mimeType == ImageType) {
        return !image.isEmpty();
    } else if (mimeType == BinaryType) {
        return !files.isEmpty();
    } else {
        return false;
    }
}
