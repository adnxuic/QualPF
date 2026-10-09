#pragma once

#include "qualpf/domain/validation.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace qualpf::application {

struct DatasetState {
    Pattern pattern;
    std::optional<PeakSet> peaks;
};
struct SessionInputs {
    std::vector<DatasetState> datasets;
    std::optional<DatasetId> active_dataset;
    std::optional<SearchParameters> search_parameters;
};
struct SessionSnapshot {
    std::uint64_t revision = 0;
    SessionInputs inputs;
};
using SnapshotHandle = std::shared_ptr<const SessionSnapshot>;

class Session;
class Edit {
public:
    Edit(const Edit&) = delete;
    Edit& operator=(const Edit&) = delete;
    Edit(Edit&&) noexcept = default;
    Edit& operator=(Edit&&) noexcept = default;

    [[nodiscard]] const SessionInputs& preview() const & noexcept { return draft_; }
    const SessionInputs& preview() const && = delete;
    [[nodiscard]] Status replace_pattern(Pattern value); // Always invalidates that dataset's peaks.
    [[nodiscard]] Status replace_peaks(PeakSet value);     // Caller supplies a fresh, same-dataset set.
    [[nodiscard]] Status remove_dataset(DatasetId id);
    [[nodiscard]] Status select_dataset(DatasetId id);
    [[nodiscard]] Status set_search_parameters(SearchParameters value);
    void cancel() noexcept { closed_ = true; } // Does not touch the Session or Fortran.

private:
    friend class Session;
    explicit Edit(SnapshotHandle base) : base_(std::move(base)), draft_(base_->inputs) {}
    SnapshotHandle base_;
    SessionInputs draft_;
    bool closed_ = false;
};

// Single-owner-thread object; distinct sessions have distinct snapshot identities.
// It does not replace AppState yet, own SQL connections, or synchronize Fortran.
class Session {
public:
    Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session&&) = delete;

    [[nodiscard]] SnapshotHandle snapshot() const noexcept { return current_; }
    [[nodiscard]] Edit begin_edit() const { return Edit(current_); }
    [[nodiscard]] Result<SnapshotHandle> commit(Edit& edit);
    // Algorithm outputs must use these checked entry points, not reattach an
    // old result through an unrelated new Edit. Successful publication advances inputs.
    [[nodiscard]] Result<SnapshotHandle> publish_pattern(SnapshotHandle input, Pattern result);
    [[nodiscard]] Result<SnapshotHandle> publish_peaks(SnapshotHandle input, PeakSet result);
    // Only a result computed from this exact current snapshot may be published.
    [[nodiscard]] Status publish(SnapshotHandle input, SearchResults result);
    // Publish a prepared residual transition as one operation. Validation and
    // allocations finish before either peaks or results become visible.
    [[nodiscard]] Status publish_search_transition(SnapshotHandle input, PeakSet peaks, SearchResults result);
    [[nodiscard]] std::shared_ptr<const SearchResults> results() const noexcept { return results_; }

private:
    SnapshotHandle current_;
    std::shared_ptr<const SearchResults> results_;
};

} // namespace qualpf::application
