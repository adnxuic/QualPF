#pragma once
#include "qualpf/application/pfsm.h"

namespace qualpf::application::pfsm_detail {
// Owned immutable inputs survive every joined worker. No analysis owner is exposed.
struct PfsmCandidateRecallInput {
    std::shared_ptr<const PfsmProblem> problem;
    std::shared_ptr<const std::vector<ProductCard>> universe;
    std::vector<profile::ProfileFitPhase> selected;
    profile::ProfileFitResult baseline;
    std::uint64_t generation = 0;
    std::shared_ptr<const profile::PfsmCandidateRecallData> data;
    std::vector<std::size_t> all_indices;
    int threads = 1;
    std::size_t baseline_version = 0;
    std::string full_reason;
};
struct PfsmCandidateRecallTiming {
    double shared = 0, correlation = 0, initial = 0, review = 0;
};
struct PfsmCandidateRecallResult {
    std::vector<std::size_t> formal_indices;
    std::shared_ptr<const profile::PfsmCandidateRecallData> data;
    std::optional<PfsmCandidateRecallReport> report;
    PfsmCandidateRecallTiming timing;
};
PfsmCandidateRecallResult recallPfsmCandidates(const PfsmCandidateRecallInput &, const ExecutionContext &);
PfsmCandidateRecallResult refinePfsmRecallCandidates(const PfsmCandidateRecallInput &,
    PfsmCandidateRecallResult, const ExecutionContext &);
void recordCandidateRecallTop5(PfsmCandidateRecallReport &, const std::vector<ProductCard> &);
} // namespace qualpf::application::pfsm_detail
