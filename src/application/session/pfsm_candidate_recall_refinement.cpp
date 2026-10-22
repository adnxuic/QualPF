#include "pfsm_workflow.h"
#include "pfsm_candidate_recall.h"
#include "pfsm_templates.h"
#include "pfsm_recall_store.h"
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
    const auto workers=[&](std::size_t count,ProfileWorkStatistics &total,auto work) {
        std::vector<ProfileWorkStatistics> measured(a.measure_work ? count : 0);
        std::vector<std::optional<Error>> errors(count);
        const int workerThreads=threads;
#pragma omp parallel for schedule(dynamic, 1) num_threads(workerThreads)
        for(std::ptrdiff_t slot=0;slot<static_cast<std::ptrdiff_t>(count);++slot) {
            const auto i=static_cast<std::size_t>(slot);
            std::fenv_t previous;std::fegetenv(&previous);std::fesetenv(&environment);
            try {
                checkpoint(context);
                if (a.measure_work) { ProfileWorkMeasurement measurement(measured[i],true); work(i); }
                else work(i);
            }
            catch(const Failure &f){errors[i]=f.error;}
            catch(const std::exception &e){errors[i]=Error{ErrorCode::numerical_failure,"pfsm.fast.v2.worker",e.what()};}
            catch(...){errors[i]=Error{ErrorCode::numerical_failure,"pfsm.fast.v2.worker","Unknown worker failure"};}
            std::fesetenv(&previous);
        }
        checkpoint(context);for(const auto &e:errors)if(e)throw Failure{*e};
        for(const auto &v:measured)total+=v;
    };
    begin=Clock::now();
    std::shared_ptr<PfsmRecallStore> building;
    if (!out.recall_store || !out.recall_store->matches(a.problem, a.universe, context)) {
        building = std::make_shared<PfsmRecallStore>(a.problem, a.universe, context,
            PfsmRecallStore::retentionBudget(*a.problem, *a.universe));
        out.recall_store = building;
    }
    for (auto index : all) {
        if (out.recall_store->get(index)) ++out.cache_hits;
        else {
            ++out.cache_misses;
            if (!out.recall_store->reserved(index)) ++out.cache_budget_fallbacks;
        }
    }
    workers(all.size(),out.timing.correlation_work,[&](std::size_t i) {
        auto &e=report.candidates[i];e.original_index=all[i];
        const auto score = [&]() -> Result<PfsmCandidateRecallEvidence> {
            if (const auto cached = out.recall_store->get(all[i]))
                return scorePreparedPfsmRecallReference(round.recall, *cached, p.physical_width->width.value, all[i], context, true);
            const auto input = expansionInput((*a.universe)[all[i]], p);
            if (!building || !building->reserved(all[i]))
                return scorePfsmCandidateRecallCandidate(round.recall, input, all[i], context, true);
            auto prepared = preparePfsmRecallReference(input, context);
            if (!prepared) return Result<PfsmCandidateRecallEvidence>{prepared.error()};
            building->store(all[i], prepared.value());
            return scorePreparedPfsmRecallReference(round.recall, prepared.value(), input.fwhm_degrees, all[i], context, true);
        };
        auto scored=score();
        if(scored)e=std::move(scored).value();
        else if(scored.error().code==ErrorCode::numerical_failure)e.protection_reason=scored.error().message;
        else throw Failure{scored.error()};
    });
    building.reset(); // No mutable cache handle survives the joined scan.
    const auto selectionStart=Clock::now();
    report.recalled_indices=selectPfsmRecallCandidates(report.candidates, p.candidate_recall.limits);
    out.timing.selection+=elapsed(selectionStart);
    out.timing.correlation+=elapsed(begin);
    std::vector<std::size_t> recalledSlots;
    for(std::size_t i=0;i<all.size();++i)if(report.candidates[i].recalled)recalledSlots.push_back(i);
    report.refinement_candidates.resize(recalledSlots.size());
    if(a.templates)a.templates->reserve(report.recalled_indices);
    begin=Clock::now();
    workers(recalledSlots.size(),out.timing.initial_work,[&](std::size_t i) {
        const auto &e=report.candidates[recalledSlots[i]];
        const auto input=a.templates ? a.templates->input(e.original_index) :
            std::make_shared<const ProfileExpansionInput>(expansionInput((*a.universe)[e.original_index],p));
        const auto prepared=a.templates && e.protection_reason.empty()
            ? a.templates->prepare(e.original_index,*input,context) : PreparedProfileTemplateHandle{};
        report.refinement_candidates[i]=take(evaluatePfsmCandidateRecallInitial(round,*input,e,context,prepared));
    });
    auto selection=preparePfsmCandidateRecallReview(report.refinement_candidates, p.candidate_recall.limits);
    out.timing.initial+=elapsed(begin);
    begin=Clock::now();
    if(!selection.review_skipped)workers(recalledSlots.size(),out.timing.review_work,[&](std::size_t i) {
        auto &e=report.refinement_candidates[i];
        if(!std::binary_search(selection.review.begin(),selection.review.end(),e.recall.original_index))return;
        const auto input=a.templates ? a.templates->input(e.recall.original_index) :
            std::make_shared<const ProfileExpansionInput>(expansionInput((*a.universe)[e.recall.original_index],p));
        const auto prepared=a.templates && e.numerical_protection.empty()
            ? a.templates->prepare(e.recall.original_index,*input,context) : PreparedProfileTemplateHandle{};
        e=take(reviewPfsmCandidateRecall(round,*input,std::move(e),context,prepared));
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
