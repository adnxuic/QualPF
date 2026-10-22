#pragma once
#include "qualpf/application/geometric_fwhm.h"
#include "qualpf/application/pfsm_evidence.h"
#include "qualpf/application/pfsm_settings.h"
#include "qualpf/profile/pfsm_candidate_recall.h"
#include "qualpf/profile/pfsm_relaxed_screen.h"
#include "qualpf/profile/work_statistics.h"

namespace qualpf::application {
namespace pfsm_detail { class PfsmRecallStore; class PfsmTemplateStore; }
enum class PfsmCandidateRecallMode { off, shadow, enabled };
enum class PfsmCandidateRecallVersion { v2 };
enum class PfsmCandidateRecallBackend { v2, bounded_integral_relaxed };
struct PfsmCandidateRecallConfiguration {
    PfsmCandidateRecallMode mode = PfsmCandidateRecallMode::off;
    PfsmCandidateRecallVersion version = PfsmCandidateRecallVersion::v2;
    profile::PfsmCandidateRecallLimits limits;
    PfsmResourceFallback resource_fallback = PfsmResourceFallback::bounded_integral;
    friend bool operator==(const PfsmCandidateRecallConfiguration &, const PfsmCandidateRecallConfiguration &) = default;
};
struct PfsmObservationRequest {
    enum class Signal { observed, original_observed };
    Signal signal = Signal::observed;
    // Explicit declaration supplied by the importer/caller, not inferred from a
    // two-column file.
    std::string signal_source, intensity_units, noise_source;
    bool unclipped_signal = false, equal_weights = false;
    // Required for original_observed: working-spectrum scaling metadata does
    // not establish whether the separately retained original signal was scaled.
    std::optional<bool> original_intensity_scaled;
    preprocessing::FwhmNoiseData noise;
    std::optional<Radiation> radiation; // Explicit working-signal radiation after stripping.
    // An absent background requests the assembled common background estimator.
    std::optional<std::vector<double>> background;
    std::vector<std::uint8_t> valid, saturated, break_before;
    preprocessing::GeometricFwhmOptions width_options;
};
struct PfsmRequest {
    // Required for a new analysis; subsequent rounds reuse the frozen problem.
    std::optional<PfsmObservationRequest> observations;
    // A physical width from explicit fixed configuration or compatible manual
    // estimation. Absent means automatic geometric estimation, never Gaussian.
    std::optional<FwhmUsage> supplied_width;
    // Bounds and fit options apply only when creating the frozen problem.
    // Continuing rounds reuse that problem; only threads is a per-run setting.
    double lower_shift = -.5, upper_shift = .5;
    profile::ProfileFitOptions fit;
    int threads = 0;             // 0: available OpenMP threads. One candidate remains sequential.
    bool template_cache = false; // Explicit opt-in; final fitting always verifies direct IL.
    PfsmCandidateRecallConfiguration candidate_recall;
    bool full_round = false; // Per-run override, without changing the frozen mode.
    bool measure_search_work = false; // Runtime-only diagnostic request.
};
struct PfsmProblem {
    std::vector<preprocessing::BackgroundPoint> background_points;
    std::shared_ptr<const profile::ProfileObservations> observations;
    Radiation radiation;
    PfsmObservationRequest source;
    std::optional<OriginalSignalProvenance> original_signal_provenance;
    preprocessing::GeometricFwhmResult width;
    std::optional<FwhmUsage> physical_width;
    AlgorithmHandle algorithms;
    double lower_shift = -.5, upper_shift = .5;
    profile::ProfileFitOptions fit;
    bool template_cache = false;
    PfsmCandidateRecallConfiguration candidate_recall;
};
struct PfsmTemplateStatistics {
    std::size_t prepare_calls = 0, hits = 0, misses = 0, expanded_hits = 0;
    std::size_t bytes = 0, peak_bytes = 0, evictions = 0;
    std::size_t budget_fallbacks = 0;
    double prepare_seconds = 0;
};
struct PfsmPerformance {
    // Expansion/solve are aggregate worker times; wall time measures joined
    // candidate work.
    double database_seconds = 0, expansion_seconds = 0, solve_seconds = 0, sorting_seconds = 0;
    double candidate_wall_seconds = 0;
    double candidate_recall_shared_seconds = 0, candidate_recall_correlation_seconds = 0, candidate_recall_local_seconds = 0;
    double candidate_recall_initial_seconds = 0, candidate_recall_review_seconds = 0;
    std::size_t formal_reused = 0;
    std::size_t evaluated = 0, residual_evaluations = 0, baseline_rebuilds = 0;
    std::size_t baseline_refinement_fits = 0;
    int threads = 1;
    PfsmTemplateStatistics templates;
    std::optional<CandidateReadStatistics> database_read;
    bool detailed_work = false;
    profile::ProfileWorkStatistics correlation_work, initial_work, review_work;
    double recall_selection_seconds = 0;
    std::size_t recall_cache_hits = 0, recall_cache_misses = 0, recall_cache_budget_fallbacks = 0;
    std::size_t recall_cache_bytes = 0;
};
struct PfsmIntegralRecallReport {
    profile::PfsmIntegralOptions options;
    profile::PfsmRelaxedOptions relaxed_options;
    std::size_t fine_bins = 0, coarse_bins = 0, bank_columns = 0, peak_buffer_bytes = 0;
    std::size_t coarse_evaluations = 0, fine_evaluations = 0, relaxed_evaluations = 0;
    double coarse_baseline_q = 0, fine_baseline_q = 0, fine_denominator = 0, tau = 0, relaxation_radius = 0;
    double shared_seconds = 0, integral_seconds = 0, relaxed_seconds = 0;
    std::vector<double> baseline_shifts;
    std::vector<profile::PfsmScreenEvidence> candidates;
};
struct PfsmCandidateRecallReport {
    PfsmCandidateRecallBackend backend = PfsmCandidateRecallBackend::v2;
    std::string switch_reason;
    std::optional<PfsmIntegralRecallReport> integral;
    PfsmCandidateRecallVersion algorithm_version = PfsmCandidateRecallVersion::v2;
    profile::PfsmCandidateRecallLimits limits;
    std::optional<profile::PfsmCandidateRecallSelection> selection;
    std::vector<profile::PfsmCandidateRecallCandidate> refinement_candidates;
    std::uint64_t generation = 0;
    std::size_t baseline_version = 0, pool_size = 0, grid_nodes = 0;
    double formal_baseline_q = 0, local_baseline_q = 0, grid_step = 0;
    bool discarded = false, full_fallback = false;
    std::string fallback_reason;
    std::vector<profile::PfsmCandidateRecallEvidence> candidates;
    std::vector<std::size_t> recalled_indices, retained_indices, top5_indices;
    std::optional<bool> recall_top5_retained, final_top5_retained;
};
struct PfsmQuantificationReference {
    double maximum_intensity = 0;
    std::optional<double> rir;
    std::string original_rir;
};
struct PfsmQuantificationLibrary {
    std::shared_ptr<const std::vector<ProductCard>> universe;
    std::map<std::string, PfsmQuantificationReference, std::less<>> references;
};
struct PfsmRirRevision {
    enum class Radiation { cu_kalpha };
    double value = 0;
    std::string source;
    Radiation radiation = Radiation::cu_kalpha;
    friend bool operator==(const PfsmRirRevision &, const PfsmRirRevision &) = default;
};
struct PfsmAnalysis {
    CandidateQuery query;
    std::shared_ptr<const PfsmProblem> problem;
    std::shared_ptr<const std::vector<ProductCard>> universe;
    std::shared_ptr<const PfsmQuantificationLibrary> quantification_library;
    std::map<std::string, PfsmRirRevision, std::less<>> rir_revisions;
    std::vector<profile::ProfileFitPhase> selected;
    profile::ProfileFitResult baseline;
    std::uint64_t generation = 0, database_version = 0;
    AlgorithmRunSource origin;
    PfsmPerformance performance;
    std::shared_ptr<const profile::PfsmCandidateRecallData> candidate_recall_data;
    std::shared_ptr<const profile::PfsmIntegralData> integral_recall_data;
    // Runtime-only derived inputs; omitted from QPF and frozen at publication.
    std::shared_ptr<const pfsm_detail::PfsmRecallStore> recall_store;
    std::shared_ptr<const pfsm_detail::PfsmTemplateStore> template_store;
    std::vector<PfsmCandidateRecallReport> candidate_recall_reports;
    bool normalization_over_full_pool = true;
    // Frozen after final baseline stabilization over the complete unselected
    // universe. Display filtering/removal/sorting must not recompute it.
    double normalization_maximum = 0;
};
struct PfsmCandidateMetrics {
    double rwp_percent = 0, shift_degrees = 0, scale = 0;
    double relative_fom = 0, delta_rwp_pp = 0;
};
std::optional<PfsmCandidateMetrics> pfsmCandidateMetrics(const ProductState &, const ProductCard &);
Result<PfsmRequest> makePfsmRequest(ProductHandle, const PfsmSettings &);
Result<ProductState> evaluatePfsmPreparation(ProductHandle, const ExecutionContext &);
bool compatibleManualPfsmWidth(const ProductState &, const FwhmUsage &);
// Prepare an atomic method/analysis transition. Caller confirms ending an
// existing analysis before passing endAnalysis=true, then commits once.
Result<ProductState> prepareSearchMethod(ProductHandle, SearchMethod, bool endAnalysis);
struct PfsmPlot {
    std::vector<preprocessing::BackgroundPoint> background_points;
    Pattern pattern;
    std::vector<double> residual;
    std::string intensity_units, candidate_id;
};
Result<PfsmPlot> preparePfsmPlot(ProductHandle, const std::string &candidateId, const ExecutionContext &);
// Read-only preparation also exposes complete no-eligible-width diagnostics.
Result<std::shared_ptr<const PfsmProblem>> preparePfsmProblem(ProductHandle, const PfsmRequest &,
                                                              const ExecutionContext &);
// Called by the canonical owner before publication of an unrelated
// pattern/configuration edit.
void invalidatePfsmAfterEdit(const ProductState &previous, ProductState &next);
Status validatePfsmState(const ProductState &);
// Rebuild executable templates from saved immutable inputs; never refit.
Status restorePfsmEvidence(ProductState &);
Result<ProductState> preparePfsmRirRevision(ProductHandle, const std::string &phaseId,
                                           std::optional<PfsmRirRevision>);
} // namespace qualpf::application
