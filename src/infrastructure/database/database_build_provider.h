#pragma once
#include "qualpf/application/database_build.h"
namespace qualpf::infrastructure {
class QtDatabaseBuildProvider final : public application::DatabaseBuildProvider {
public:
    std::unique_ptr<application::PdfBuildSource> preparePdf(const std::string &, const std::string &) override;
    application::CifDiscovery discoverCif(const std::string &, bool) const override;
    application::CifBackendResult backend() const override;
    std::unique_ptr<application::CifRecordWriter> makeWriter() override;
    std::unique_ptr<application::CifSourceReader> makeReader() override;
    std::string describe(const application::CifImportFailure &) const override;
    void diagnostic(const application::CifImportFailure &) const override;
};
} // namespace qualpf::infrastructure
