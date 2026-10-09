#pragma once
#include "qualpf/application/database/cif_import_service.h"
#include "infrastructure/database/qt_sql/cif_database_writer.h"
#include <QString>
namespace qualpf::infrastructure {
using application::CifBackendResult;
using application::CifImportCounters;
using application::CifImportFailure;
using application::CifImportService;
using application::CifPreparation;
using application::PreparedCifImport;
[[nodiscard]] CifBackendResult cif_database_backend_from_environment();
// Qt IO/composition adapter. Numerical orchestration has one pure owner.
class FileCifSourceReader final : public application::CifSourceReader {
  public:
    std::variant<std::string, application::CifImportFailure> read(const std::string &, std::size_t) override;
};
class CifDatabaseImportService {
  public:
    CifDatabaseImportService() : service_(writer_) {}
    std::optional<CifImportFailure> create(const QString &base) {
        return service_.create(base.toUtf8().toStdString());
    }
    CifPreparation prepare_file(const QString &, std::int32_t, io::cif::BlockSelection = {});
    CifPreparation prepare_bytes(std::string_view bytes, std::string source, std::int32_t id,
                                 io::cif::BlockSelection selection = {}) {
        return service_.prepare_bytes(bytes, std::move(source), id, std::move(selection));
    }
    CifPreparation reload(std::string_view ticket, std::int32_t identity) {
        return service_.reload(ticket, identity);
    }
    std::optional<CifImportFailure> commit(const PreparedCifImport &value, std::stop_token stop = {}) {
        return service_.commit(value, stop);
    }
    void close() { service_.close(); }
    const CifImportCounters &counters() const { return service_.counters(); }

  private:
    database::CifDatabaseWriter writer_;
    CifImportService service_;
};
} // namespace qualpf::infrastructure
