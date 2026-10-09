#pragma once

#include "qualpf/application/database/cif_batch.h"
#include "cif_database_import_service.h"
#include <functional>

namespace qualpf::infrastructure {
using application::CifDatabaseBackend;
using application::CifImportCounters;
using application::CifImportFailure;
struct CifSourceRequest {
    QString path;
    std::int32_t identity = 0;
    io::cif::BlockSelection selection;
};
using application::CifBatchErrorPolicy;
using CifBlockSelector = std::function<std::optional<std::string>(
    const QString &, const std::vector<io::cif::StructureBlockCandidate> &)>;
struct CifProductOptions {
    bool inorganic_only = false;
    CifBatchErrorPolicy errors = CifBatchErrorPolicy::skip_invalid;
    CifBlockSelector select_block;
    std::stop_token stop;
};
struct CifCommittedSource {
    QString path;
    std::int32_t identity;
    std::string selected_block;
    std::string source_sha256;
    std::string structure_identity;
};
struct CifProductReport {
    CifDatabaseBackend backend = CifDatabaseBackend::cpp;
    std::size_t discovered = 0;
    std::size_t committed = 0;
    std::size_t skipped = 0;
    bool completed = false;
    bool cancelled = false;
    std::vector<CifImportFailure> failures;
    std::optional<CifImportFailure> fatal;
    std::vector<CifCommittedSource> sources;
    CifImportCounters cpp;
    std::uint64_t legacy_native_calls = 0;
    [[nodiscard]] bool succeeded() const { return completed && !cancelled && !fatal; }
};
using CifProductProgress = std::function<void(const QString &, const CifProductReport &)>;

// Directory discovery preserves the existing filename-derived integer identity.
// A supplied manifest may assign explicit IDs; neither route uses scan order.
[[nodiscard]] std::variant<std::vector<CifSourceRequest>, CifImportFailure>
cif_sources_in_directory(const QString &directory, bool recursive);
[[nodiscard]] QString describe_cif_failure(const CifImportFailure &failure);
} // namespace qualpf::infrastructure

namespace qualpf::infrastructure {
[[nodiscard]] CifProductReport build_cif_sources(const QString &, const std::vector<CifSourceRequest> &,
                                                 CifProductOptions = {}, CifProductProgress = {});
}
