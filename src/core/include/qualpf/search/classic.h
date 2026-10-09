#pragma once

#include "qualpf/migration/backends.h"

#include <optional>
#include <span>
#include <vector>

namespace qualpf::search {

// Source-derived compatibility contract, not a claim of validated Fortran
// equivalence. Only legacy_mixed_precision is supported: default REAL is
// modelled as binary32, ABI doubles remain binary64. All workspace is local to
// each synchronous call.
enum class ScoreDisposition { scored, no_reflections_in_range, no_associations };
struct ScoreTrace {
    ScoreValues score{0.0, 0.0, 0.0, 1.0};
    ScoreDisposition disposition = ScoreDisposition::no_reflections_in_range;
    std::vector<std::size_t> reflection_indices;                  // Into the original card, in source order.
    std::vector<std::optional<std::size_t>> experimental_indices; // Per in-range reflection.
    std::vector<double> merged_intensities;                       // Per experimental peak, before scaling.
    std::vector<double> scaled_intensities;
    std::size_t associated_reflections = 0;
    std::size_t associated_peaks = 0;
    std::size_t unassociated_below_observed = 0;
    std::size_t expected_peaks_per_phase = 0;
    double position_difference_sum = 0.0;
    double associated_sample_sum = 0.0;
    double associated_card_sum = 0.0;
    double total_sample_sum = 0.0;
    double total_card_sum = 0.0;
    double database_fraction = 0.0;
    double sample_fraction = 0.0;
    double sqrt_database = 0.0;
    double sqrt_sample = 0.0;
};
Result<ScoreTrace> score_classic(const ScoreInput &, const ExecutionContext &);
class ClassicScorer final : public IScoringBackend {
  public:
    Result<ScoreValues> score(const ScoreInput &, const ExecutionContext &) override;
};

// Input is the decoded, stored top-index sequence (including its decimal
// quantization), NOT freshly selected strongest reflections. Uses its first
// min(3, size) entries, as makeQueryStrongest does. Empty top index passes.
Result<bool> passes_strongest(const PeakSet &, std::span<const DSpacingAngstrom> stored_top);

struct ResidualAssociation {
    std::optional<std::size_t> reflection_index;
    double quality = 0.0;
};
// Separate legacy residual/GUI association: inclusive window, first-index ties,
// conflict rule, fixed 1000 intensity denominator. Does not merge reflections.
Result<std::vector<ResidualAssociation>> associate_residual(const PeakSet &, const ReferenceCard &,
                                                            double scale, AngularWidthDegrees delta);
struct ResidualResult {
    PeakSet peaks;
    std::vector<ResidualAssociation> associations; // Indexed by INPUT peaks.
    std::vector<PeakId> removed;
};
Result<ResidualResult> subtract_phase(const PeakSet &, const ReferenceCard &, double scale,
                                      const SearchParameters &);

enum class SearchPass { initial, rescore };
struct CandidateInput {
    ReferenceCard card;
    std::optional<std::vector<DSpacingAngstrom>> stored_top;
    // Required for rescore: MainWindow intentionally preserves the previous
    // display/accepted-card scale even though computeFOM fits a new scale.
    std::optional<double> previous_scale;
};
struct BatchEvaluation {
    SearchResults results;
    std::vector<bool> prefilter_passed;    // Input order; includes deleted-card filter.
    std::vector<CandidateScore> evaluated; // Before threshold/order/limit; includes failures.
};
// Decoded candidate batch only: repository constraints/SQL/order are supplied
// by the caller. Initial: score > minimum, sort only when truncating. Rescore:
// score > 0 && score >= minimum, always sort. Ties retain input order (legacy
// std::sort ties were unspecified). Zero maximum_entries retains zero rows.
Result<BatchEvaluation> evaluate_candidates(const Pattern &, const PeakSet &, std::span<const CandidateInput>,
                                            const SearchParameters &, SearchPass, const ExecutionContext &);

} // namespace qualpf::search
