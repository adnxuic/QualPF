#include "product_files.h"
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
namespace qualpf::infrastructure {
Result<std::string> ProjectFileWriter::prepare(application::ProductHandle source,const application::MetadataObject &defaults,const std::string &created) const {
    auto encoded=encodeProject(std::move(source),defaults,QString::fromStdString(created));
    if (!encoded) return Result<std::string>{encoded.error()};
    return Result<std::string>{QJsonDocument(encoded.value()).toJson().toStdString()};
}
Status ProjectFileWriter::commit(const std::string &path,const std::string &bytes) const {
    QSaveFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly|QIODevice::Text))
        return Status{Error{ErrorCode::invalid_input,"project.file",path,"project_open_failed"}};
    if (file.write(bytes.data(),qint64(bytes.size()))!=qint64(bytes.size()) || !file.commit())
        return Status{Error{ErrorCode::invalid_input,"project.file",path,"project_save_failed"}};
    return Status{std::monostate{}};
}
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
