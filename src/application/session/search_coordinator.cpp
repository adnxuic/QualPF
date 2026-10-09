#include "qualpf/application/search_coordinator.h"
#include <exception>
#include <limits>

namespace qualpf::application {
Result<SearchTask> SearchCoordinator::begin(ReferenceDatabase database) {
    if (std::this_thread::get_id() != owner_)
        return Result<SearchTask>{
            Error{ErrorCode::invalid_input, "search.thread", "Admission requires the owner thread"}};
    if (closing_)
        return Result<SearchTask>{Error{ErrorCode::closed_edit, "search", "Workspace is closing", "closing"}};
    if (active_)
        return Result<SearchTask>{
            Error{ErrorCode::invalid_input, "search", "A search is already running", "busy"}};
    if (issuedTasks_ == std::numeric_limits<std::uint64_t>::max())
        return Result<SearchTask>{Error{ErrorCode::exhausted_revision, "search.task_id",
                                        "Task identity exhausted", "task_id_exhausted"}};
    SearchTask task;
    task.source = session_.snapshot();
    task.database = std::move(database);
    // Construct owned values before occupying the slot or consuming an ID.
    stop_ = std::stop_source{};
    task.id = issuedTasks_ + 1;
    task.context = {stop_.get_token(), NumericalPolicy::legacy_mixed_precision, "product-search"};
    active_ = task;
    issuedTasks_ = task.id;
    databaseVersion_ = task.database.selectionVersion;
    error_.reset();
    cancelled_ = false;
    return Result<SearchTask>{std::move(task)};
}
void SearchCoordinator::cancel() noexcept {
    cancelled_ = true;
    stop_.request_stop();
}
void SearchCoordinator::close() noexcept {
    closing_ = true;
    cancel();
}
Result<PreparedSearchResult> SearchCoordinator::complete(std::uint64_t task,
                                                         Result<PreparedSearchResult> result,
                                                         SearchPublication publication) {
    if (std::this_thread::get_id() != owner_)
        return Result<PreparedSearchResult>{
            Error{ErrorCode::invalid_input, "search.thread", "Completion requires the owner thread"}};
    // An old delivery cannot retire, cancel or overwrite the active newer task.
    if (!active_ || active_->id != task)
        return Result<PreparedSearchResult>{
            Error{ErrorCode::stale_snapshot, "search.task_id", "Obsolete search task", "old_task"}};
    try {
        const auto input = *active_;
        if (closing_)
            error_ = Error{ErrorCode::closed_edit, "search", "Workspace is closing", "closing"};
        else if (stop_.stop_requested()) {
            cancelled_ = true;
            error_ = Error{ErrorCode::cancelled, "search", "Search canceled"};
        } else if (!result) {
            error_ = result.error();
            cancelled_ = error_->code == ErrorCode::cancelled;
        } else if (result.value().taskId != task || result.value().source != input.source ||
                   session_.snapshot() != input.source ||
                   session_.snapshot()->revision != input.source->revision)
            error_ =
                Error{ErrorCode::stale_snapshot, "search.source", "Search input changed during calculation"};
        else if (result.value().databaseVersion != input.database.selectionVersion ||
                 databaseVersion_ != input.database.selectionVersion)
            error_ = Error{ErrorCode::stale_snapshot, "search.database",
                           "Database selection changed during calculation"};
        else if (publication == SearchPublication::commit && result.value().changed) {
            auto status = session_.commit(input.source, result.value().next);
            if (!status)
                error_ = status.error();
        }
    } catch (const std::exception &exception) {
        error_ =
            Error{ErrorCode::numerical_failure, "search.commit", exception.what(), "unexpected_exception"};
    } catch (...) {
        error_ = Error{ErrorCode::numerical_failure, "search.commit", "Unexpected publication failure",
                       "unexpected_exception"};
    }
    active_.reset();
    if (error_)
        return Result<PreparedSearchResult>{*error_};
    return result;
}
} // namespace qualpf::application
