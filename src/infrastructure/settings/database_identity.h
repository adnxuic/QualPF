#pragma once
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileInfo>
#include <string>
namespace qualpf::infrastructure {
// Stable physical-file identity: canonical location, size and modification
// stamp for every reference/index member. Selection counters are deliberately
// excluded.
inline std::string databaseFileIdentity(const std::string &base) {
    if (base.empty())
        return {};
    QByteArray evidence("qualpf.database.files.v1\n");
    for (const char *suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"}) {
        const QFileInfo info(QString::fromStdString(base) + suffix);
        evidence += info.exists() ? info.canonicalFilePath().toUtf8() : QByteArray(suffix);
        evidence += '\n';
        evidence += QByteArray::number(info.size());
        evidence += ':';
        evidence += QByteArray::number(info.lastModified().toMSecsSinceEpoch());
        evidence += '\n';
    }
    return QCryptographicHash::hash(evidence, QCryptographicHash::Sha256).toHex().toStdString();
}
} // namespace qualpf::infrastructure
