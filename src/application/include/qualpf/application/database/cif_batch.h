#pragma once
#include "qualpf/application/database/cif_import_service.h"
#include <functional>
namespace qualpf::application {
struct CifBatchRequest {
    std::string path;
    std::int32_t identity = 0;
    io::cif::BlockSelection selection;
};
enum class CifBatchErrorPolicy { skip_invalid, stop_on_error };
struct CifBatchOptions {
    bool inorganic_only = false;
    CifBatchErrorPolicy errors = CifBatchErrorPolicy::skip_invalid;
    std::function<std::optional<std::string>(const std::string &,
                                             const std::vector<io::cif::StructureBlockCandidate> &)>
        select_block;
    std::stop_token stop;
};
struct CifBatchCommittedSource {
    std::string path;
    std::int32_t identity;
    std::string selected_block, source_sha256, structure_identity;
};
struct CifBatchReport {
    CifDatabaseBackend backend = CifDatabaseBackend::cpp;
    std::size_t discovered = 0, committed = 0, skipped = 0;
    bool completed = false, cancelled = false;
    std::vector<CifImportFailure> failures;
    std::optional<CifImportFailure> fatal;
    std::vector<CifBatchCommittedSource> sources;
    CifImportCounters cpp;
    std::uint64_t legacy_native_calls = 0;
    [[nodiscard]] bool succeeded() const { return completed && !cancelled && !fatal; }
};
using CifBatchProgress = std::function<void(const std::string &, const CifBatchReport &)>;
// One use case for CLI and GUI; IO and diagnostics are supplied ports.
[[nodiscard]] CifBatchReport runCifBatch(CifRecordWriter &, CifSourceReader &, const std::string &basePath,
                                         const std::vector<CifBatchRequest> &sources,
                                         CifDatabaseBackend backend, CifBatchOptions options = {},
                                         CifBatchProgress progress = {},
                                         std::function<void(const CifImportFailure &)> diagnostic = {});
} // namespace qualpf::application
