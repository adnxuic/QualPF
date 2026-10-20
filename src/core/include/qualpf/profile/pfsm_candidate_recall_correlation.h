#pragma once
#include "qualpf/profile/pfsm.h"

namespace qualpf::profile {
// Recall-only counts. These values never configure the formal fitter.
struct PfsmCandidateRecallLimits {
    int recall_candidates = 500, local_shortlist = 20, protected_per_channel = 5;
    int review_limit = 80, formal_limit = 80;
    friend bool operator==(const PfsmCandidateRecallLimits &, const PfsmCandidateRecallLimits &) = default;
};
Status validatePfsmCandidateRecallLimits(const PfsmCandidateRecallLimits &);
// Compatibility defaults for diagnostic-only consumers.
inline constexpr std::size_t pfsm_candidate_recall_recall_budget = PfsmCandidateRecallLimits{}.recall_candidates;
inline constexpr std::size_t pfsm_candidate_recall_fit_budget = PfsmCandidateRecallLimits{}.local_shortlist;
struct PfsmCandidateRecallKernel;
struct PfsmCandidateRecallLocalProblem;
struct PfsmCandidateRecallData {
    double origin = 0, step = 0, width = 0, lower_shift = 0, upper_shift = 0;
    double denominator = 0;
    std::vector<double> weights, weighted_signal, original_response, energy_response, scan;
    std::shared_ptr<const ProfileObservations> observations;
    std::shared_ptr<const PfsmCandidateRecallKernel> kernel;
};
struct PfsmCandidateRecallSelected {
    ProfileExpansionInput input;
    double shift = 0;
};
struct PfsmCandidateRecallRound {
    std::shared_ptr<const PfsmCandidateRecallData> data;
    std::vector<double> residual_response;
    std::shared_ptr<const PfsmCandidateRecallLocalProblem> local;
    double baseline_q = 0;
};
struct PfsmCandidateRecallEvidence {
    std::size_t original_index = 0;
    std::optional<double> original_score, residual_score, local_q, gain;
    std::optional<std::size_t> original_rank, residual_rank, local_rank;
    double original_shift = 0, residual_shift = 0;
    std::vector<double> original_seeds, residual_seeds; // Only populated for v2.
    std::size_t nnls_iterations = 0;
    bool recalled = false, retained = false;
    std::string recall_reason, retention_reason;
    std::string protection_reason;
};
Result<std::shared_ptr<const PfsmCandidateRecallData>> preparePfsmCandidateRecallData(
    std::shared_ptr<const ProfileObservations>, double width, double lower, double upper,
    const ExecutionContext &);
Result<PfsmCandidateRecallRound> preparePfsmCandidateRecallRound(std::shared_ptr<const PfsmCandidateRecallData>,
    std::span<const double> residual, std::span<const PfsmCandidateRecallSelected>, const ExecutionContext &,
    bool prepareLocalDictionary = true);
Result<PfsmCandidateRecallEvidence> scorePfsmCandidateRecallCandidate(const PfsmCandidateRecallRound &, const ProfileExpansionInput &,
    std::size_t originalIndex, const ExecutionContext &, bool collectSeeds = false);
// Deterministic original-library indices, with all cutoff ties and protected rows.
// The caller validates limits before entering selection.
std::vector<std::size_t> selectPfsmRecallCandidates(std::vector<PfsmCandidateRecallEvidence> &,
                                                 const PfsmCandidateRecallLimits & = {});
// Validation-only backprojection ranking; never the production retained set.
std::vector<std::size_t> selectPfsmRecallAblationCandidates(std::vector<PfsmCandidateRecallEvidence> &);
} // namespace qualpf::profile
