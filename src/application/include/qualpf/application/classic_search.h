#pragma once

#include "qualpf/application/session.h"
#include "qualpf/search/classic.h"

namespace qualpf::application {

// Offline integration/shadow preparation only; no product backend selector or
// Legacy writes. Repository/presenter supplies decoded candidates in its order.
// The input handle keeps the complete source state alive until publication.
class PreparedClassicSearch {
  public:
    const search::BatchEvaluation &evaluation() const noexcept { return evaluation_; }
    const std::optional<PeakSet> &residual_peaks() const noexcept { return residual_; }
    Status publish(Session &, const ExecutionContext &) const;

  private:
    friend Result<PreparedClassicSearch> prepare_classic_search(SnapshotHandle,
                                                                std::span<const search::CandidateInput>,
                                                                search::SearchPass, const ExecutionContext &);
    friend Result<PreparedClassicSearch> prepare_classic_residual(SnapshotHandle, const ReferenceCard &,
                                                                  double,
                                                                  std::span<const search::CandidateInput>,
                                                                  const ExecutionContext &);
    PreparedClassicSearch(SnapshotHandle input, search::BatchEvaluation evaluation,
                          std::optional<PeakSet> residual)
        : input_(std::move(input)), evaluation_(std::move(evaluation)), residual_(std::move(residual)) {}
    SnapshotHandle input_;
    search::BatchEvaluation evaluation_;
    std::optional<PeakSet> residual_;
};

Result<PreparedClassicSearch> prepare_classic_search(SnapshotHandle, std::span<const search::CandidateInput>,
                                                     search::SearchPass, const ExecutionContext &);
// Reuses the original working Pattern; only peak intensities change. No live
// accepted-phase list, quantification or project serialization is implied.
Result<PreparedClassicSearch> prepare_classic_residual(SnapshotHandle, const ReferenceCard &, double scale,
                                                       std::span<const search::CandidateInput>,
                                                       const ExecutionContext &);
} // namespace qualpf::application
