#pragma once

#include "qualpf/application/database/cif_card_record.h"
#include "qualpf/application/database/cif_record_writer.h"
#include <cstdint>
#include <stop_token>

namespace qualpf::application {

enum class CifDatabaseBackend { cpp, legacy };

struct CifImportFailure {
    std::string stage;
    std::string code;
    std::string source;
    std::string block;
    std::string field;
    std::string message;
    std::string core_reason;
    io::cif::Location location;
    std::vector<io::cif::StructureBlockCandidate> candidates;
    std::optional<io::cif::StructureProvenance> provenance;
    std::string core_field;
};

using CifBackendResult = std::variant<CifDatabaseBackend, CifImportFailure>;
[[nodiscard]] CifBackendResult cif_database_backend(std::string_view setting);

// Counts explicit calls at this Application boundary. The completed chain
// counter requires a successful immutable card from the unchanged Phase 13–18
// implementation. Internal ticket verification may repeat parsing/import.
struct CifImportCounters {
    std::uint64_t parser_calls = 0;
    std::uint64_t importer_calls = 0;
    std::uint64_t card_pipeline_calls = 0;
    std::uint64_t completed_phase13_18_chains = 0;
    std::uint64_t writer_calls = 0;
    std::uint64_t committed_cards = 0;
};

class PreparedCifImport {
  public:
    [[nodiscard]] const database::CifRecord &record() const { return record_; }
    [[nodiscard]] const CifCard &calculated() const { return calculated_; }
    [[nodiscard]] const std::string &ticket() const { return ticket_; }

  private:
    friend class CifImportService;
    PreparedCifImport(CifCard calculated, database::CifRecord record, std::string ticket)
        : calculated_(std::move(calculated)), record_(std::move(record)), ticket_(std::move(ticket)) {}
    CifCard calculated_;
    database::CifRecord record_;
    std::string ticket_;
};
using CifPreparation = std::variant<PreparedCifImport, CifImportFailure>;

// Sole default product owner for CIF -> selected structure -> complete card ->
// DTO -> attached SQLite transaction. A rejected preparation never touches SQL.
// Thread-confined; no SessionView, GUI state, or Legacy workspace is owned.
class CifSourceReader {
  public:
    virtual ~CifSourceReader() = default;
    virtual std::variant<std::string, CifImportFailure> read(const std::string &path,
                                                             std::size_t maximum) = 0;
};

class CifImportService {
  public:
    explicit CifImportService(CifRecordWriter &writer) : writer_(writer) {}
    [[nodiscard]] std::optional<CifImportFailure> create(const std::string &base);
    [[nodiscard]] CifPreparation prepare_source(CifSourceReader &, const std::string &path,
                                                std::int32_t identity,
                                                io::cif::BlockSelection selection = {});
    [[nodiscard]] CifPreparation prepare_bytes(std::string_view bytes, std::string source,
                                               std::int32_t identity, io::cif::BlockSelection selection = {});
    [[nodiscard]] CifPreparation reload(std::string_view ticket, std::int32_t identity);
    [[nodiscard]] std::optional<CifImportFailure> commit(const PreparedCifImport &prepared,
                                                         std::stop_token stop = {});
    void close() { writer_.close(); }
    [[nodiscard]] const CifImportCounters &counters() const { return counters_; }

  private:
    [[nodiscard]] CifPreparation finish(io::cif::ImportedCrystal imported, std::int32_t identity,
                                        std::string ticket);
    CifRecordWriter &writer_;
    CifImportCounters counters_;
};

} // namespace qualpf::application
