#include "cif_database_import_service.h"
#include <QFile>
namespace qualpf::infrastructure {
std::variant<std::string, CifImportFailure> FileCifSourceReader::read(const std::string &source,
                                                                      std::size_t maximum) {
    QFile file(QString::fromUtf8(source.data(), static_cast<qsizetype>(source.size())));
    const auto failed = [&] {
        return CifImportFailure{
            "read", "cif_read_failure", source, {}, "file", file.errorString().toStdString(), {}, {}, {}, {}};
    };
    if (!file.open(QIODevice::ReadOnly))
        return failed();
    const auto bytes = file.read(static_cast<qint64>(maximum) + 1);
    if (file.error() != QFileDevice::NoError)
        return failed();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}
CifBackendResult cif_database_backend_from_environment() {
    return application::cif_database_backend(qEnvironmentVariable("QUALPF_CIF_DB_BACKEND").toStdString());
}
CifPreparation CifDatabaseImportService::prepare_file(const QString &path, std::int32_t identity,
                                                      io::cif::BlockSelection selection) {
    FileCifSourceReader reader;
    return service_.prepare_source(reader, path.toUtf8().toStdString(), identity, std::move(selection));
}
} // namespace qualpf::infrastructure
