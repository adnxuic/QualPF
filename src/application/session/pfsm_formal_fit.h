#pragma once
#include "qualpf/application/pfsm_evidence.h"

namespace qualpf::application::pfsm_detail {
// Owned candidate input. The caller retains the immutable baseline and modules
// until all workers join. No recall configuration, seeds or evidence can enter.
struct PfsmFormalCandidateInput {
    profile::ProfileFitInput fit;
    profile::ProfileExpansionInput expansion;
    std::string identity;
    std::size_t original_index = 0;
    std::uint64_t generation = 0;
    double lower_shift = -.5, upper_shift = .5;
    profile::PreparedProfileTemplateHandle prepared;
};
struct PfsmFormalCandidateResult {
    std::shared_ptr<const PfsmCandidateEvidence> evidence;
    std::optional<Error> error;
    double expansion_seconds = 0, solve_seconds = 0;
};
PfsmFormalCandidateResult fitPfsmCandidate(PfsmFormalCandidateInput,
    const profile::IProfileExpander &, const profile::IProfileFitter &,
    const profile::ProfileFitResult &baseline, const ExecutionContext &);
// Fixed PFSM operation applied to checked formal evidence, not an algorithm override.
double calculatePfsmFom(double deltaQ, double denominatorNet, double baselineRwp, double trialRwp);
} // namespace qualpf::application::pfsm_detail
