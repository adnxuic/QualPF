#include "product_files.h"
#include "infrastructure/serialization/quantification.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
namespace qualpf::infrastructure {
Result<std::string> QtTextFiles::read(const std::string &path) const {
    QFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::ReadOnly)) return Result<std::string>{Error{ErrorCode::missing_data,"peaks.file",file.errorString().toStdString(),"file_read_failed"}};
    const auto bytes=file.readAll();
    if (file.error()!=QFileDevice::NoError) return Result<std::string>{Error{ErrorCode::missing_data,"peaks.file",file.errorString().toStdString(),"file_read_failed"}};
    return Result<std::string>{bytes.toStdString()};
}
Status QtTextFiles::writeAtomic(const std::string &path,const std::string &bytes) const {
    QSaveFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(),qint64(bytes.size()))!=qint64(bytes.size()) || !file.commit())
        return Status{Error{ErrorCode::invalid_input,"peaks.file",file.errorString().toStdString(),"file_write_failed"}};
    return Status{std::monostate{}};
}
}
