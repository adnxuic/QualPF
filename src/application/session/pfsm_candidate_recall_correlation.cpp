#include "pfsm_workflow.h"
#include "pfsm_candidate_recall.h"
#include <cfenv>
#include <omp.h>

namespace qualpf::application::pfsm_detail {
PfsmCandidateRecallResult recallPfsmCandidates(const PfsmCandidateRecallInput &a,
    const ExecutionContext &context) {
    const auto &all = a.all_indices;
    PfsmCandidateRecallResult out;
    out.formal_indices = all;
    out.data = a.data;
    using namespace profile;
    if (a.problem->candidate_recall.mode == PfsmCandidateRecallMode::off) return out;
    auto &report = out.report.emplace();
    report.algorithm_version = a.problem->candidate_recall.version;
    report.limits = a.problem->candidate_recall.limits;
    report.generation = a.generation; report.baseline_version = a.baseline_version; report.pool_size = all.size();
    report.formal_baseline_q = a.baseline.q;
    const auto full = [&](std::string reason) {
        report.full_fallback = true; report.fallback_reason = std::move(reason);
        report.recalled_indices = report.retained_indices = all;

        return out;
    };
    if (!a.full_reason.empty()) return full(a.full_reason);
    if (all.size() <= static_cast<std::size_t>(report.limits.local_shortlist)) return full("small_pool");
    const auto &p = *a.problem;
    if (!dynamic_cast<const IntermediateLorentzianExpander *>(p.algorithms->modules.profile_expander.get()) ||
        !dynamic_cast<const NnlsTrfProfileFitter *>(p.algorithms->modules.profile_fitter.get()))
        return full("replacement_module");
    auto begin = Clock::now();
    if (!out.data) {
        auto prepared = preparePfsmCandidateRecallData(p.observations, p.physical_width->width.value,
                                            p.lower_shift, p.upper_shift, context);
        if (!prepared) {
            out.timing.shared += elapsed(begin);
            if (prepared.error().code != ErrorCode::numerical_failure) throw Failure{prepared.error()};
            return full(prepared.error().message);
        }
        out.data = std::move(prepared).value();
    }
    report.grid_nodes = out.data->weights.size(); report.grid_step = out.data->step;
    out.timing.shared += elapsed(begin);
    return refinePfsmRecallCandidates(a, std::move(out), context);
}

void recordCandidateRecallTop5(PfsmCandidateRecallReport &r, const std::vector<ProductCard> &ranked) {
    r.recall_top5_retained = r.final_top5_retained = true;
    for (std::size_t i = 0; i < std::min<std::size_t>(5, ranked.size()); ++i) {
        const auto index = ranked[i].pfsm->original_index;
        r.top5_indices.push_back(index);
        *r.recall_top5_retained = *r.recall_top5_retained &&
            std::binary_search(r.recalled_indices.begin(), r.recalled_indices.end(), index);
        *r.final_top5_retained = *r.final_top5_retained &&
            std::binary_search(r.retained_indices.begin(), r.retained_indices.end(), index);
    }
}
} // namespace qualpf::application::pfsm_detail
