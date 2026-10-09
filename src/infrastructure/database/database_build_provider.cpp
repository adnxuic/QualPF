#include "database_build_provider.h"
#include "cif_database_product.h"
#include "qt_sql/pdf2_database_writer.h"
#include "qt_sql/sql_schema_builder.h"
#include "infrastructure/io/pdf2/pdf2reader.h"
#include <QDebug>
#include <QFileInfo>

namespace qualpf::infrastructure {
namespace {
class QtPdfBuildSource final : public application::PdfBuildSource {
    std::unique_ptr<SqlSchemaBuilder> schema_;
    Pdf2Reader reader_;
    Pdf2DatabaseWriter writer_;
    QString path_;
public:
    QtPdfBuildSource(std::unique_ptr<SqlSchemaBuilder> schema, QString path)
        : schema_(std::move(schema)), writer_(schema_.get()), path_(std::move(path)) {
        QObject::connect(&reader_, &Pdf2Reader::cardReady, &writer_, &Pdf2DatabaseWriter::onCardReady);
        QObject::connect(&reader_, &Pdf2Reader::finished, &writer_, &Pdf2DatabaseWriter::onFinished);
    }
    std::int64_t size() const override { return QFileInfo(path_).size(); }
    bool parse(application::PdfBuildProgress progress) override {
        const auto connection = QObject::connect(&reader_, &Pdf2Reader::progress, &reader_,
            [progress = std::move(progress)](int cards, qint64 bytes, qint64 total) {
                if (progress) progress(cards, bytes, total);
            });
        const bool result = reader_.parse(path_);
        QObject::disconnect(connection);
        return result;
    }
    void cancel() override { reader_.cancel(); }
};
}
std::unique_ptr<application::PdfBuildSource> QtDatabaseBuildProvider::preparePdf(const std::string &base, const std::string &source) {
    auto schema = std::make_unique<SqlSchemaBuilder>();
    if (!schema->create(QString::fromStdString(base), SqlSchemaBuilder::DbType::Pdf2)) return nullptr;
    return std::make_unique<QtPdfBuildSource>(std::move(schema), QString::fromStdString(source));
}
application::CifDiscovery QtDatabaseBuildProvider::discoverCif(const std::string &directory, bool recursive) const {
    const auto discovered = cif_sources_in_directory(QString::fromStdString(directory), recursive);
    if (const auto *error = std::get_if<application::CifImportFailure>(&discovered)) return *error;
    std::vector<application::CifBatchRequest> sources;
    for (const auto &source : std::get<std::vector<CifSourceRequest>>(discovered))
        sources.push_back({source.path.toUtf8().toStdString(), source.identity, source.selection});
    return sources;
}
application::CifBackendResult QtDatabaseBuildProvider::backend() const { return cif_database_backend_from_environment(); }
std::unique_ptr<application::CifRecordWriter> QtDatabaseBuildProvider::makeWriter() { return std::make_unique<database::CifDatabaseWriter>(); }
std::unique_ptr<application::CifSourceReader> QtDatabaseBuildProvider::makeReader() { return std::make_unique<FileCifSourceReader>(); }
std::string QtDatabaseBuildProvider::describe(const application::CifImportFailure &error) const { return describe_cif_failure(error).toStdString(); }
void QtDatabaseBuildProvider::diagnostic(const application::CifImportFailure &error) const { qWarning().noquote() << describe_cif_failure(error); }
} // namespace qualpf::infrastructure
