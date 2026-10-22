#pragma once
#include "qualpf/profile/pfsm_candidate_recall_correlation.h"

namespace qualpf::profile {
// Recall-only states. None of these parameters may initialize the formal fitter.
struct PfsmCandidateRecallPhysicalPhase {
    ProfileExpansionInput input;
    double scale = 0, shift = 0;
};
struct PfsmCandidateRecallPhysicalProblem;
struct PfsmCandidateRecallPhysicalRound {
    PfsmCandidateRecallRound recall;
    std::shared_ptr<const PfsmCandidateRecallPhysicalProblem> physical;
    double baseline_q = 0;
};
struct PfsmCandidateRecallPhysicalFit {
    ProfileFitParameters parameters;
    double q = 0, gain = 0, difference_gain = 0, kkt = 0;
    bool baseline_fallback = true, approximated = false;
};
struct PfsmCandidateRecallBackprojectionTrial {
    double linear_q = 0, candidate_initial_shift = 0;
    std::vector<double> coefficients;
    std::vector<std::vector<double>> column_shifts;
    PfsmCandidateRecallPhysicalFit physical;
};
struct PfsmCandidateRecallBackprojectionResult {
    PfsmCandidateRecallPhysicalFit best;
    std::vector<PfsmCandidateRecallBackprojectionTrial> trials;
    std::size_t nnls_calls = 0, nnls_iterations = 0;
};
Result<PfsmCandidateRecallPhysicalRound> preparePfsmCandidateRecallPhysicalRound(
    std::shared_ptr<const PfsmCandidateRecallData>, std::span<const double> residual,
    std::span<const PfsmCandidateRecallPhysicalPhase>, const ExecutionContext &, bool backprojectionDictionary = true);
Result<PfsmCandidateRecallBackprojectionResult> fitPfsmCandidateRecallBackprojection(
    const PfsmCandidateRecallPhysicalRound &, const ProfileExpansionInput &, const PfsmCandidateRecallEvidence &,
    const ExecutionContext &);
// Fixed v2 numerical rules; candidate counts live in PfsmCandidateRecallLimits.
struct PfsmCandidateRecallPolicy {
    static constexpr std::size_t seeds = 3, steps = 2, initial_calls = 3, total_calls = 15;
    static constexpr double radius = .125, acceptance = .1, gain_floor = 1e-12;
    static constexpr double distortion = .1, boundary = .05, correlation = 1e-3, underestimation = .25;
};
struct PfsmCandidateRecallStep {
    ProfileFitParameters anchor;
    std::vector<double> linear_coefficients;
    PfsmCandidateRecallPhysicalFit physical;
    double predicted = 0, actual = 0, distortion = 0, radius = 0;
    std::optional<double> ratio;
    bool accepted = false, indistinguishable = false, local_boundary = false;
};
struct PfsmCandidateRecallSeed {
    double initial_shift = 0, radius = PfsmCandidateRecallPolicy::radius;
    PfsmCandidateRecallPhysicalFit initial_fit, current;
    std::vector<PfsmCandidateRecallStep> steps;
    bool initialized = false;
};
struct PfsmCandidateRecallCandidate {
    PfsmCandidateRecallEvidence recall;
    PfsmCandidateRecallPhysicalFit best;
    std::vector<double> seeds;
    std::vector<PfsmCandidateRecallSeed> starts;
    std::optional<double> initial_gain, initial_eta, eta;
    double maximum_distortion = 0;
    std::size_t nnls_calls = 0, nnls_iterations = 0;
    bool initialized = false, reviewed = false;
    std::string numerical_protection;
    std::optional<std::size_t> initial_rank, local_rank;
    std::vector<std::string> review_reasons, retention_reasons;
    std::optional<double> formal_gain;

};
Result<PfsmCandidateRecallCandidate> evaluatePfsmCandidateRecallInitial(const PfsmCandidateRecallPhysicalRound &,
    const ProfileExpansionInput &, const PfsmCandidateRecallEvidence &, const ExecutionContext &,
    PreparedProfileTemplateHandle prepared = {});
Result<PfsmCandidateRecallCandidate> reviewPfsmCandidateRecall(const PfsmCandidateRecallPhysicalRound &,
    const ProfileExpansionInput &, PfsmCandidateRecallCandidate, const ExecutionContext &,
    PreparedProfileTemplateHandle prepared = {});
struct PfsmCandidateRecallSelection {
    PfsmCandidateRecallLimits limits;
    double g_star = PfsmCandidateRecallPolicy::gain_floor;
    std::vector<std::size_t> initial_top, local_top, channel, review, protected_indices, proposed, retained;
    bool review_skipped = false, full_fallback = false;
    std::string fallback_reason;
};
// Rows contain recalled candidates only. All decisions occur after a joined stage.
PfsmCandidateRecallSelection preparePfsmCandidateRecallReview(std::vector<PfsmCandidateRecallCandidate> &,
                                                             const PfsmCandidateRecallLimits & = {});
void finalizePfsmCandidateRecall(std::vector<PfsmCandidateRecallCandidate> &, PfsmCandidateRecallSelection &,
                       std::span<const std::size_t> completePool);
bool pfsmCandidateRecallUnderestimated(const PfsmCandidateRecallCandidate &, double gStar, double formalGain);
} // namespace qualpf::profile
