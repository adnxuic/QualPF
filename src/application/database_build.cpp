#include "qualpf/application/database_build.h"

namespace qualpf::application {
std::unique_ptr<PdfDatabaseBuild> DatabaseBuildUseCases::preparePdf(const std::string &base, const std::string &source) {
    auto reader = provider_.preparePdf(base, source);
    return reader ? std::make_unique<PdfDatabaseBuild>(std::move(reader)) : nullptr;
}
CifBatchReport DatabaseBuildUseCases::buildCif(const std::string &base, const std::vector<CifBatchRequest> &sources,
                                             CifBatchOptions options, CifBatchProgress progress) {
    const auto selected = provider_.backend();
    if (const auto *error = std::get_if<CifImportFailure>(&selected)) {
        CifBatchReport report;
        report.fatal = *error;
        return report;
    }
    auto writer = provider_.makeWriter();
    auto reader = provider_.makeReader();
    return runCifBatch(*writer, *reader, base, sources, std::get<CifDatabaseBackend>(selected),
                       std::move(options), std::move(progress), [this](const auto &error) { provider_.diagnostic(error); });
}
std::string cifClassificationName(io::cif::BlockClassification classification) {
    return std::string(io::cif::classification_name(classification));
}
} // namespace qualpf::application
