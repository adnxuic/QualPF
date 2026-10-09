#include "qualpf/application/database/cif_batch.h"
#include <exception>
namespace qualpf::application {
CifBatchReport runCifBatch(CifRecordWriter &writer, CifSourceReader &reader, const std::string &basePath,
                           const std::vector<CifBatchRequest> &sources, CifDatabaseBackend backend,
                           CifBatchOptions options, CifBatchProgress progress,
                           std::function<void(const CifImportFailure &)> diagnostic) {
    CifBatchReport report;
    report.backend = backend;
    CifImportService service(writer);
    const auto cancelled = [&] {
        report.cancelled = options.stop.stop_requested();
        return report.cancelled;
    };
    if (cancelled())
        return report;
    if (auto error = service.create(basePath)) {
        report.fatal = *error;
        return report;
    }
    const auto fail = [&](CifImportFailure error) {
        if (diagnostic)
            diagnostic(error);
        report.failures.push_back(std::move(error));
    };
    try {
        for (const auto &request : sources) {
            if (cancelled())
                break;
            ++report.discovered;
            if (progress)
                progress(request.path, report);
            if (cancelled())
                break;
            { // Existing qualified canonical C++ owner.
                auto prepared =
                    service.prepare_source(reader, request.path, request.identity, request.selection);
                if (const auto *error = std::get_if<CifImportFailure>(&prepared);
                    error && error->code == "multiple_structure_blocks" && options.select_block) {
                    const auto chosen = options.select_block(request.path, error->candidates);
                    if (cancelled())
                        break;
                    if (chosen) {
                        prepared = service.prepare_source(
                            reader, request.path, request.identity,
                            {qualpf::io::cif::BlockSelectionPolicy::specified_block, *chosen});
                    }
                }
                if (const auto *error = std::get_if<CifImportFailure>(&prepared)) {
                    fail(*error);
                } else {
                    const auto &card = std::get<PreparedCifImport>(prepared);
                    if (options.inorganic_only && card.record().subfile != "inorganic") {
                        ++report.skipped;
                    } else if (auto error = service.commit(card, options.stop)) {
                        if (error->code == "cancelled") {
                            report.cancelled = true;
                            break;
                        }
                        if (error->code == "rollback_failure")
                            report.fatal = *error;
                        fail(*error);
                    } else {
                        ++report.committed;
                        const auto &r = card.record();
                        report.sources.push_back(
                            {request.path, r.identity, r.block, r.source_sha256, r.structure_identity});
                    }
                }
            }
            report.cpp = service.counters();
            if (progress)
                progress(request.path, report);
            if (report.fatal ||
                (options.errors == CifBatchErrorPolicy::stop_on_error && !report.failures.empty()))
                break;
        }
    } catch (const std::exception &error) {
        report.fatal = CifImportFailure{"application",
                                        "cif_callback_or_operation_failure",
                                        {},
                                        {},
                                        "callback",
                                        error.what(),
                                        {},
                                        {},
                                        {},
                                        {}};
    } catch (...) {
        report.fatal = CifImportFailure{"application",
                                        "cif_callback_or_operation_failure",
                                        {},
                                        {},
                                        "callback",
                                        "Unknown exception",
                                        {},
                                        {},
                                        {},
                                        {}};
    }
    service.close();
    report.cpp = service.counters();
    report.cancelled = report.cancelled || options.stop.stop_requested();
    report.completed = !report.cancelled && !report.fatal &&
                       !(options.errors == CifBatchErrorPolicy::stop_on_error && !report.failures.empty());
    return report;
}

} // namespace qualpf::application
