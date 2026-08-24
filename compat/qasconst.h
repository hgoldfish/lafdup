#ifndef LAFDUP_QASCONST_COMPAT_H
#define LAFDUP_QASCONST_COMPAT_H

// qAsConst() 是 Qt 5.7 才引入的 API。Windows XP 构建使用 Qt 5.6.3，
// 通过 -include 强制注入此兼容实现；Qt >= 5.7 时使用 QtCore 原生实现。
#include <QtCore/qglobal.h>

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
template <typename T> Q_DECL_CONSTEXPR const T &qAsConst(T &t) Q_DECL_NOTHROW
{
    return t;
}
template <typename T> void qAsConst(const T &&) = delete;
#endif

#endif  // LAFDUP_QASCONST_COMPAT_H
