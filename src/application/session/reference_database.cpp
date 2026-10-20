#include "qualpf/application/reference_database.h"
#include <limits>

namespace qualpf::application {
DatabaseCatalog::DatabaseCatalog(SearchCoordinator &searches, DatabaseCatalogStore &store,
                                 ReferenceRepository &repository, std::uint64_t version)
    : searches_(searches), store_(store), repository_(repository), version_(version) {
    searches_.selectDatabase(version);
}
Result<DatabaseRemovalResult> DatabaseCatalog::removeDraft(std::vector<ReferenceDatabaseEntry> entries,
                                                           int row, int activeIndex, bool deleteFiles,
                                                           std::function<void(const DatabaseRemovalResult &)> beforePersist) {
    if (row < 0 || static_cast<std::size_t>(row) >= entries.size())
        return Result<DatabaseRemovalResult>{Error{ErrorCode::invalid_input, "database.removal", "Invalid database selection", "invalid_selection"}};
    auto removal = deleteFiles ? store_.prepareRemoval(entries[row].path) : nullptr;
    if (deleteFiles && !removal)
        return Result<DatabaseRemovalResult>{Error{ErrorCode::invalid_input, "database.removal", "The database is in use by a search.", "database_busy"}};
    entries.erase(entries.begin() + row);
    if (activeIndex == row) {
        activeIndex = entries.empty() ? -1 : 0;
        if (activeIndex == 0) entries[0].inUse = true;
    } else if (activeIndex > row) --activeIndex;
    auto failed = removal ? removal->removeFiles() : std::vector<std::string>{};
    DatabaseRemovalResult result{std::move(entries), activeIndex, std::move(failed)};
    if (beforePersist) beforePersist(result);
    store_.save(result.entries); // The write lease spans presentation and persistence.
    return Result<DatabaseRemovalResult>{std::move(result)};
}
void DatabaseCatalog::load() {
    entries_ = store_.load();
    const auto directory = defaultDirectory();
    if (directoryExists(directory)) discover(directory);
}
const ReferenceDatabaseEntry *DatabaseCatalog::active() const {
    for (const auto &entry : entries_) if (entry.inUse) return &entry;
    return nullptr;
}
Status DatabaseCatalog::select(std::vector<ReferenceDatabaseEntry> entries) {
    if (searches_.running()) {
        searches_.cancel();
        return Status{Error{ErrorCode::invalid_input, "database.selection", "Finish the running Search first",
                            "search_in_progress"}};
    }
    if (searches_.closing())
        return Status{Error{ErrorCode::cancelled, "database.selection", "Workspace is closing", "closing"}};
    std::string path;
    for (const auto &entry : entries) if (entry.inUse) { path = entry.path; break; }
    if (path != openPath_ && version_ == std::numeric_limits<std::uint64_t>::max())
        return Status{Error{ErrorCode::invalid_input, "database.selection", "Database selection revision exhausted",
                            "revision_exhausted"}};
    entries_ = std::move(entries);
    store_.save(entries_);
    return openActive();
}
Status DatabaseCatalog::openActive() {
    const auto *entry = active();
    const auto path = entry ? entry->path : std::string{};
    if (path == openPath_) return Status{std::monostate{}};
    if (version_ == std::numeric_limits<std::uint64_t>::max())
        return Status{Error{ErrorCode::invalid_input, "database.selection", "Database selection revision exhausted",
                            "revision_exhausted"}};
    ++version_;
    searches_.selectDatabase(version_);
    repository_.close();
    openPath_.clear();
    if (!path.empty() && repository_.open(path)) openPath_ = path;
    return Status{std::monostate{}};
}
Result<std::vector<ProductCard>> synchronousQuery(SearchCoordinator &execution, CandidateReader &reader,
    ReferenceDatabase database, CandidateQuery query, SearchParameters parameters, double threshold, bool scored,
    SearchProgress progress, SynchronousSearchPurpose purpose) {
    if (purpose == SynchronousSearchPurpose::cli_match && !scored)
        return Result<std::vector<ProductCard>>{Error{ErrorCode::invalid_input, "search.operation",
            "CLI matching requires a scored query"}};
    auto admission = execution.begin(std::move(database));
    if (!admission) return Result<std::vector<ProductCard>>{admission.error()};
    SearchInput input;
    input.source = admission.value().source;
    input.method = admission.value().algorithms->spec.method();
    input.taskId = admission.value().id;
    input.databaseVersion = admission.value().database.selectionVersion;
    input.databaseIdentity = admission.value().database.identity;
    input.query = std::move(query);
    input.operation = purpose == SynchronousSearchPurpose::cli_match ? SearchOperation::cli_match :
        scored ? SearchOperation::scored_browse : SearchOperation::metadata_browse;
    input.threshold = threshold;
    input.parameters = parameters;
    if (purpose == SynchronousSearchPurpose::browse)
        input.parameters.maximum_entries = std::numeric_limits<std::size_t>::max();
    auto result = execution.complete(admission.value().id,
        SearchEngine{admission.value().algorithms}.run(input, reader, admission.value().context, std::move(progress)),
        SearchPublication::prepare_only);
    if (!result) return Result<std::vector<ProductCard>>{result.error()};
    return Result<std::vector<ProductCard>>{std::move(result).value().next.candidates};
}
} // namespace qualpf::application
