#include "models.h"

CopyPaste::CopyPaste(const Direction &direction, const QDateTime &timestamp, const QString &text)
    : direction(direction)
    , timestamp(timestamp)
    , mimeType(TextType)
    , text(text)
    , isTextHtml(false)
{
}

CopyPaste::CopyPaste()
    : direction(Incoming)
{
}

QVariantMap CopyPaste::saveState()
{
    QVariantMap state;
    //    if (type == CopyPasteType::Incoming) {
    //        state.insert("type", 1);
    //    } else {
    //        state.insert("type", 2);
    //    }
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
    //    bool ok;
    //    if (state.value("type").toInt(&ok) == 1) {
    //        type = Incoming;
    //    } else if (state.value("type").toInt(&ok) == 2) {
    //        type = Outgoing;
    //    } else {
    //        return false;
    //    }
    //    if (!ok) {
    //        return false;
    //    }
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
