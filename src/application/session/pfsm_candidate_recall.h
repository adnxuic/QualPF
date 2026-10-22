#pragma once
#include "qualpf/application/pfsm.h"

namespace qualpf::application::pfsm_detail {
class PfsmTemplateStore;
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
    std::shared_ptr<PfsmTemplateStore> templates;
    std::shared_ptr<const profile::PfsmIntegralData> integral_data;
    bool measure_work = false;
    std::shared_ptr<const PfsmRecallStore> recall_store;
};
struct PfsmCandidateRecallTiming {
    double shared = 0, correlation = 0, initial = 0, review = 0;
    double selection = 0;
    profile::ProfileWorkStatistics correlation_work, initial_work, review_work;
};
struct PfsmCandidateRecallResult {
    std::vector<std::size_t> formal_indices;
    std::shared_ptr<const profile::PfsmCandidateRecallData> data;
    std::shared_ptr<const profile::PfsmIntegralData> integral_data;
    std::optional<PfsmCandidateRecallReport> report;
    PfsmCandidateRecallTiming timing;
    std::shared_ptr<const PfsmRecallStore> recall_store;
    std::size_t cache_hits = 0, cache_misses = 0, cache_budget_fallbacks = 0;
};
PfsmCandidateRecallResult recallPfsmCandidates(const PfsmCandidateRecallInput &, const ExecutionContext &);
PfsmCandidateRecallResult refinePfsmRecallCandidates(const PfsmCandidateRecallInput &,
    PfsmCandidateRecallResult, const ExecutionContext &);
PfsmCandidateRecallResult integralPfsmRecallCandidates(const PfsmCandidateRecallInput &,
    PfsmCandidateRecallResult, const ExecutionContext &);
void recordCandidateRecallTop5(PfsmCandidateRecallReport &, const std::vector<ProductCard> &);
} // namespace qualpf::application::pfsm_detail
