#pragma once
#include "qualpf/preprocessing/legacy.h"
#include <utility>

namespace qualpf::preprocessing::detail {
struct Measured {
    float x, y, width, area = 0;
    bool removed = false;
};
using RefinedPeakCandidates = std::pair<std::vector<Measured>, std::vector<Measured>>;
// Ordered derivative triggers, before any Gaussian fitting or width-dependent
// decisions. The applied half-window is retained for the refinement stage.
Result<std::vector<std::size_t>> detect_peak_candidates(const std::vector<float> &x,
    const std::vector<float> &y, PeakOptions &, std::optional<float> normalization_maximum = {});
Result<RefinedPeakCandidates> refine_peak_candidates(const std::vector<float> &x,
    const std::vector<float> &y, PeakOptions &, std::span<const std::size_t> triggers,
    PeakDetectionStage, std::vector<RejectedPeakCandidate> &, std::size_t sample_offset = 0,
    std::optional<float> normalization_maximum = {});
// Fixed strongest-three binary32 reduction, independently testable without
// fabricating fitted widths in an initial-finder result.
Result<FwhmEstimate> summarize_fwhm(std::span<const Peak>);
// Internal numerical seam: callers supply the validated binary32 grid/signal and
// ordered fitted candidates. Only coincident endpoints are recoverable rejections.
Result<std::vector<RejectedPeakCandidate>> measure_peak_candidates(std::vector<Measured> &,
    std::span<const float> x, std::span<const float> y, PeakDetectionStage, std::size_t sample_offset = 0);
} // namespace qualpf::preprocessing::detail
