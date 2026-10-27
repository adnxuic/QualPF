#pragma once

#include <QByteArray>
#include <QStringList>

namespace qualpf::infrastructure {
// Query fragments retain their existing predicates and LIKE wildcards. Only
// values cross this boundary: never interpret an identifier or restraint as SQL.
inline QString sqlTextLiteral(QString value) {
    if (value.contains(QChar(u'\0')))
        return QStringLiteral("CAST(X'%1' AS TEXT)").arg(QString::fromLatin1(value.toUtf8().toHex()));
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + value + QLatin1Char('\'');
}

inline QString sqlTextList(const QStringList &values) {
    QStringList quoted;
    quoted.reserve(values.size());
    for (const auto &value : values)
        quoted.append(sqlTextLiteral(value));
    return quoted.join(QLatin1Char(','));
}
} // namespace qualpf::infrastructure
