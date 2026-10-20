#include "pfsm_workflow.h"
#include "pfsm_candidate_recall.h"
#include <cfenv>
#include <omp.h>

namespace qualpf::application::pfsm_detail {
PfsmCandidateRecallResult refinePfsmRecallCandidates(const PfsmCandidateRecallInput &a,
    PfsmCandidateRecallResult out, const ExecutionContext &context) {
    const auto &all = a.all_indices;
    const int threads = a.threads;
    auto &report = *out.report;
    using namespace profile;
    const auto &p=*a.problem;
    auto begin=Clock::now();
    std::vector<PfsmCandidateRecallPhysicalPhase> selected;
    for(std::size_t j=0;j<a.selected.size();++j) {
        const auto card=std::find_if(a.universe->begin(),a.universe->end(),
            [&](const auto &c){return c.id==a.selected[j].identity;});
        if(card==a.universe->end())fail("pfsm.fast.v2","Selected phase missing from universe");
        selected.push_back({expansionInput(*card,p),a.baseline.parameters.scales[j],a.baseline.parameters.shifts[j]});
    }
    auto prepared=preparePfsmCandidateRecallPhysicalRound(out.data,a.baseline.residual,selected,context,false);
    out.timing.shared+=elapsed(begin);
    if(!prepared) {
        if(prepared.error().code!=ErrorCode::numerical_failure)throw Failure{prepared.error()};
        report.full_fallback=true;report.fallback_reason=prepared.error().message;
        report.recalled_indices=report.retained_indices=all;
        return out;
    }
    const auto round=std::move(prepared).value();report.local_baseline_q=round.baseline_q;
    report.candidates.resize(all.size());
    std::fenv_t environment;std::fegetenv(&environment);
    const auto workers=[&](std::size_t count,auto work) {
        std::vector<std::optional<Error>> errors(count);
        const int workerThreads=threads;
#pragma omp parallel for schedule(dynamic, 1) num_threads(workerThreads)
        for(std::ptrdiff_t slot=0;slot<static_cast<std::ptrdiff_t>(count);++slot) {
            const auto i=static_cast<std::size_t>(slot);
            std::fenv_t previous;std::fegetenv(&previous);std::fesetenv(&environment);
            try {checkpoint(context);work(i);}
            catch(const Failure &f){errors[i]=f.error;}
            catch(const std::exception &e){errors[i]=Error{ErrorCode::numerical_failure,"pfsm.fast.v2.worker",e.what()};}
            catch(...){errors[i]=Error{ErrorCode::numerical_failure,"pfsm.fast.v2.worker","Unknown worker failure"};}
            std::fesetenv(&previous);
        }
        checkpoint(context);for(const auto &e:errors)if(e)throw Failure{*e};
    };
    begin=Clock::now();
    workers(all.size(),[&](std::size_t i) {
        auto &e=report.candidates[i];e.original_index=all[i];
        auto scored=scorePfsmCandidateRecallCandidate(round.recall,expansionInput((*a.universe)[all[i]],p),all[i],context,true);
        if(scored)e=std::move(scored).value();
        else if(scored.error().code==ErrorCode::numerical_failure)e.protection_reason=scored.error().message;
        else throw Failure{scored.error()};
    });
    report.recalled_indices=selectPfsmRecallCandidates(report.candidates, p.candidate_recall.limits);
    out.timing.correlation+=elapsed(begin);
    std::vector<std::size_t> recalledSlots;
    for(std::size_t i=0;i<all.size();++i)if(report.candidates[i].recalled)recalledSlots.push_back(i);
    report.refinement_candidates.resize(recalledSlots.size());
    begin=Clock::now();
    workers(recalledSlots.size(),[&](std::size_t i) {
        const auto &e=report.candidates[recalledSlots[i]];
        report.refinement_candidates[i]=take(evaluatePfsmCandidateRecallInitial(round,
            expansionInput((*a.universe)[e.original_index],p),e,context));
    });
    auto selection=preparePfsmCandidateRecallReview(report.refinement_candidates, p.candidate_recall.limits);
    out.timing.initial+=elapsed(begin);
    begin=Clock::now();
    if(!selection.review_skipped)workers(recalledSlots.size(),[&](std::size_t i) {
        auto &e=report.refinement_candidates[i];
        if(!std::binary_search(selection.review.begin(),selection.review.end(),e.recall.original_index))return;
        const auto input=expansionInput((*a.universe)[e.recall.original_index],p);
        e=take(reviewPfsmCandidateRecall(round,input,std::move(e),context));
    });
    finalizePfsmCandidateRecall(report.refinement_candidates,selection,all);
    out.timing.review+=elapsed(begin);
    report.full_fallback=selection.full_fallback;report.fallback_reason=selection.fallback_reason;
    report.retained_indices=selection.retained;
    for(std::size_t i=0;i<recalledSlots.size();++i) {
        const auto &v=report.refinement_candidates[i];auto &e=report.candidates[recalledSlots[i]];
        if(v.initialized){e.gain=v.best.gain;e.local_q=v.best.q;}
        e.local_rank=v.local_rank;e.nnls_iterations=v.nnls_iterations;e.protection_reason=v.numerical_protection;
        if(!v.retention_reasons.empty())e.retention_reason=v.retention_reasons.front();
    }
    for(auto &e:report.candidates) {
        e.retained=std::binary_search(report.retained_indices.begin(),report.retained_indices.end(),e.original_index);
        if(e.retained && e.retention_reason.empty())e.retention_reason="full_fallback";
    }
    report.selection=std::move(selection);
    out.formal_indices = p.candidate_recall.mode == PfsmCandidateRecallMode::shadow ? all : report.retained_indices;
    return out;
}
} // namespace qualpf::application::pfsm_detail
