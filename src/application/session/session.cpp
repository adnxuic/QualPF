#include "qualpf/application/session.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace qualpf::application {
namespace {
Status ok() { return Status{std::monostate{}}; }
Error closed() { return {ErrorCode::closed_edit, "edit", "Edit was cancelled, committed or moved from"}; }
auto find_dataset(SessionInputs& inputs, DatasetId id)
{
    return std::find_if(inputs.datasets.begin(), inputs.datasets.end(),
                        [id](const auto& dataset) { return dataset.pattern.id == id; });
}
} // namespace

Status Edit::replace_pattern(Pattern value)
{
    if (closed_ || !base_) return Status{closed()};
    const auto issues = validate(value);
    if (!issues.empty()) return Status{issues.front()};
    const auto id = value.id;
    auto found = find_dataset(draft_, id);
    if (found == draft_.datasets.end()) draft_.datasets.push_back({std::move(value), std::nullopt});
    else *found = {std::move(value), std::nullopt};
    if (!draft_.active_dataset) draft_.active_dataset = id;
    return ok();
}

Status Edit::replace_peaks(PeakSet value)
{
    if (closed_ || !base_) return Status{closed()};
    const auto issues = validate(value);
    if (!issues.empty()) return Status{issues.front()};
    auto found = find_dataset(draft_, value.dataset);
    if (found == draft_.datasets.end())
        return Status{Error{ErrorCode::missing_data, "peaks.dataset", "Load the matching pattern first"}};
    found->peaks = std::move(value);
    return ok();
}

Status Edit::remove_dataset(DatasetId id)
{
    if (closed_ || !base_) return Status{closed()};
    auto found = find_dataset(draft_, id);
    if (found == draft_.datasets.end())
        return Status{Error{ErrorCode::missing_data, "dataset", "Dataset does not exist"}};
    draft_.datasets.erase(found);
    if (draft_.active_dataset == id) draft_.active_dataset.reset();
    return ok();
}

Status Edit::select_dataset(DatasetId id)
{
    if (closed_ || !base_) return Status{closed()};
    if (find_dataset(draft_, id) == draft_.datasets.end())
        return Status{Error{ErrorCode::missing_data, "dataset", "Dataset does not exist"}};
    draft_.active_dataset = id;
    return ok();
}

Status Edit::set_search_parameters(SearchParameters value)
{
    if (closed_ || !base_) return Status{closed()};
    const auto issues = validate(value);
    if (!issues.empty()) return Status{issues.front()};
    draft_.search_parameters = std::move(value);
    return ok();
}

Session::Session() : current_(std::make_shared<const SessionSnapshot>()) {}

Result<SnapshotHandle> Session::commit(Edit& edit)
{
    if (edit.closed_ || !edit.base_) return Result<SnapshotHandle>{closed()};
    // Pointer identity prevents cross-session acceptance even when revisions match.
    if (edit.base_ != current_)
        return Result<SnapshotHandle>{Error{ErrorCode::stale_snapshot, "edit", "Snapshot is stale or belongs to another session"}};
    if (current_->revision == std::numeric_limits<std::uint64_t>::max())
        return Result<SnapshotHandle>{Error{ErrorCode::exhausted_revision, "revision", "Revision cannot wrap"}};
    // Copy into a const allocation. Draft references can never mutate committed data.
    auto next = std::make_shared<const SessionSnapshot>(SessionSnapshot{current_->revision + 1, edit.draft_});
    current_ = std::move(next);
    results_.reset(); // Conservative invalidation, including accepted-phase calculations.
    edit.closed_ = true;
    return Result<SnapshotHandle>{current_};
}

Result<SnapshotHandle> Session::publish_pattern(SnapshotHandle input, Pattern result)
{
    if (!input || input != current_)
        return Result<SnapshotHandle>{Error{ErrorCode::stale_snapshot, "pattern", "Calculation input is no longer current"}};
    const auto& datasets = input->inputs.datasets;
    if (std::none_of(datasets.begin(), datasets.end(), [&](const auto& item) { return item.pattern.id == result.id; }))
        return Result<SnapshotHandle>{Error{ErrorCode::invalid_input, "pattern.id", "Calculated output changed dataset identity"}};
    auto edit = begin_edit();
    auto applied = edit.replace_pattern(std::move(result));
    if (!applied) return Result<SnapshotHandle>{applied.error()};
    return commit(edit);
}

Result<SnapshotHandle> Session::publish_peaks(SnapshotHandle input, PeakSet result)
{
    if (!input || input != current_)
        return Result<SnapshotHandle>{Error{ErrorCode::stale_snapshot, "peaks", "Calculation input is no longer current"}};
    auto edit = begin_edit();
    auto applied = edit.replace_peaks(std::move(result));
    if (!applied) return Result<SnapshotHandle>{applied.error()};
    return commit(edit);
}

Status Session::publish(SnapshotHandle input, SearchResults result)
{
    if (!input || input != current_)
        return Status{Error{ErrorCode::stale_snapshot, "results", "Result input is no longer current"}};
    if (current_->inputs.active_dataset != result.dataset)
        return Status{Error{ErrorCode::invalid_input, "results.dataset", "Result is not for the active dataset"}};
    const auto issues = validate(result);
    if (!issues.empty()) return Status{issues.front()};
    results_ = std::make_shared<const SearchResults>(result);
    return ok();
}

Status Session::publish_search_transition(SnapshotHandle input, PeakSet peaks, SearchResults result) {
    if (!input || input != current_)
        return Status{Error{ErrorCode::stale_snapshot, "results", "Search input is no longer current"}};
    if (current_->inputs.active_dataset != peaks.dataset || result.dataset != peaks.dataset)
        return Status{
            Error{ErrorCode::invalid_input, "results.dataset", "Transition must use the active dataset"}};
    if (!result.complete)
        return Status{
            Error{ErrorCode::invalid_input, "results.complete", "Cannot publish a partial transition"}};
    auto issues = validate(peaks);
    if (!issues.empty())
        return Status{issues.front()};
    issues = validate(result);
    if (!issues.empty())
        return Status{issues.front()};
    if (current_->revision == std::numeric_limits<std::uint64_t>::max())
        return Status{Error{ErrorCode::exhausted_revision, "revision", "Revision cannot wrap"}};
    auto inputs = current_->inputs;
    auto found = find_dataset(inputs, peaks.dataset);
    if (found == inputs.datasets.end())
        return Status{Error{ErrorCode::missing_data, "peaks.dataset", "Dataset does not exist"}};
    found->peaks = std::move(peaks);
    auto next =
        std::make_shared<const SessionSnapshot>(SessionSnapshot{current_->revision + 1, std::move(inputs)});
    auto next_results = std::make_shared<const SearchResults>(std::move(result));
    current_ = std::move(next);
    results_ = std::move(next_results);
    return ok();
}

} // namespace qualpf::application
