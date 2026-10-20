#include "qualpf/application/workflow.h"
#include "qualpf/application/pfsm.h"
#include <algorithm>
#include <limits>
#include <set>
namespace qualpf::application {
namespace {
bool sameSignal(const std::optional<SampledSignal> &a,const std::optional<SampledSignal> &b) {
    return bool(a)==bool(b)&&(!a||(a->values==b->values&&bool(a->window)==bool(b->window)&&
        (!a->window||a->window->first==b->window->first)));
}
bool sameMeasurement(const Pattern &a,const Pattern &b) {
    if(a.id!=b.id||a.source_id!=b.source_id||a.two_theta!=b.two_theta||a.observed!=b.observed||
       a.corrected_two_theta!=b.corrected_two_theta||a.weights!=b.weights||a.radiation.kind!=b.radiation.kind||
       a.radiation.lines.size()!=b.radiation.lines.size()||a.intensity_scale!=b.intensity_scale||
       a.processing.background_subtracted!=b.processing.background_subtracted||
       a.processing.kalpha2_subtracted!=b.processing.kalpha2_subtracted||
       a.processing.intensity_scaled!=b.processing.intensity_scaled||bool(a.calculation_range)!=bool(b.calculation_range)||
       (a.calculation_range&&(a.calculation_range->minimum!=b.calculation_range->minimum||a.calculation_range->maximum!=b.calculation_range->maximum))) return false;
    for(std::size_t i=0;i<a.radiation.lines.size();++i)
        if(a.radiation.lines[i].wavelength!=b.radiation.lines[i].wavelength||
           a.radiation.lines[i].relative_weight!=b.radiation.lines[i].relative_weight) return false;
    return true;
}
bool finished(const WorkflowRecord &r) {return r.status==WorkflowStepStatus::completed||r.status==WorkflowStepStatus::skipped;}
bool sameParameters(const WorkflowProfile &a,const WorkflowProfile &b,WorkflowStep step) {
    switch(step) {
    case WorkflowStep::background: return sameBackgroundOptions(a.background,b.background);
    case WorkflowStep::initial_peaks: return a.initial_peaks==b.initial_peaks;
    case WorkflowStep::fwhm: return a.fwhm==b.fwhm;
    case WorkflowStep::peaks: return samePeakOptions(a.peaks,b.peaks)&&a.selection.minimum==b.selection.minimum&&
        a.selection.maximum==b.selection.maximum&&a.selection.threshold_percent==b.selection.threshold_percent&&
        a.selection.maximum_peaks==b.selection.maximum_peaks&&a.selection.append==b.selection.append;
    default: return true;
    }
}
void invalidate(WorkflowState &state,std::uint64_t id) {
    std::set<std::uint64_t> affected{id};
    for(auto &r:state.records) {
        if(affected.contains(r.id)||std::any_of(r.dependencies.begin(),r.dependencies.end(),[&](auto d){return affected.contains(d);})) {
            affected.insert(r.id); r.status=WorkflowStepStatus::invalidated;
        }
    }
}
void clearResult(ProductState &n, WorkflowStep step) {
    auto &c=n.preprocessing;
    switch(step) {
    case WorkflowStep::background:
        n.pattern.background.reset(); c.background_points.reset(); c.background_filters.clear();
        n.provenance.backgroundSource.reset(); n.projectMetadata["has_back"]=false; n.prepared_pfsm.reset(); break;
    case WorkflowStep::initial_peaks: c.initial_peak_usage.reset(); break;
    case WorkflowStep::fwhm: c.fwhm_usage.reset(); c.fwhm_estimate.reset(); n.prepared_pfsm.reset(); break;
    case WorkflowStep::peaks:
        n.peaks.clear(); c.peak_candidates.reset(); c.peak_calibration.reset(); c.peak_fwhm_usage.reset();
        c.rejected_candidates.clear(); c.candidate_failure.reset(); n.peakTolerance.reset(); n.automaticDelta=.5;
        n.provenance.peakSource.reset(); n.provenance.retainedPeakSources.clear(); break;
    case WorkflowStep::search: case WorkflowStep::phases: case WorkflowStep::rir:
        if(!n.provenance.residualHistory.empty()) n.peaks.intensity=n.peaks.intensityOrig;
        n.candidates.clear(); n.phases.clear(); n.pfsm.reset(); n.provenance.residualHistory.clear(); n.pattern.calculated.reset(); break;
    default: break;
    }
}
void restoreStep(ProductState &to,const ProductState &from,WorkflowStep step) {
    auto &a=to.preprocessing; const auto &b=from.preprocessing;
    switch(step) {
    case WorkflowStep::range: case WorkflowStep::smoothing: case WorkflowStep::kalpha2:
        to.pattern=from.pattern; to.original_signal_provenance=from.original_signal_provenance;
        a=b; to.peaks=from.peaks; to.peakTolerance=from.peakTolerance; to.automaticDelta=from.automaticDelta;
        to.provenance=from.provenance; to.candidates=from.candidates; to.phases=from.phases; to.pfsm=from.pfsm;
        to.prepared_pfsm=from.prepared_pfsm; to.analysis_database=from.analysis_database; to.restored_project=from.restored_project;
        to.projectMetadata["has_back"]=bool(from.pattern.background);
        break;
    case WorkflowStep::background:
        to.pattern.background=from.pattern.background;
        a.background=b.background; a.background_points=b.background_points; a.background_filters=b.background_filters;
        to.provenance.backgroundSource=from.provenance.backgroundSource;
        to.projectMetadata["has_back"]=bool(from.pattern.background);
        restoreStep(to,from,WorkflowStep::initial_peaks);
        break;
    case WorkflowStep::initial_peaks:
        a.initial_peak_usage=b.initial_peak_usage; a.initial_peaks=b.initial_peaks;
        restoreStep(to,from,WorkflowStep::fwhm); break;
    case WorkflowStep::fwhm:
        a.fwhm=b.fwhm; a.fwhm_usage=b.fwhm_usage; a.fwhm_estimate=b.fwhm_estimate; to.prepared_pfsm=from.prepared_pfsm;
        restoreStep(to,from,WorkflowStep::peaks); break;
    case WorkflowStep::peaks:
        to.peaks=from.peaks; to.peakTolerance=from.peakTolerance; to.automaticDelta=from.automaticDelta;
        a.peaks=b.peaks; a.selection=b.selection; a.peak_candidates=b.peak_candidates;
        a.peak_calibration=b.peak_calibration; a.peak_fwhm_usage=b.peak_fwhm_usage;
        a.rejected_candidates=b.rejected_candidates; a.candidate_failure=b.candidate_failure;
        to.provenance.peakSource=from.provenance.peakSource;
        to.provenance.retainedPeakSources=from.provenance.retainedPeakSources;
        restoreStep(to,from,WorkflowStep::search); break;
    case WorkflowStep::search: case WorkflowStep::phases:
        to.algorithms=from.algorithms; to.candidates=from.candidates; to.phases=from.phases; to.pfsm=from.pfsm;
        to.analysis_database=from.analysis_database; to.restored_project=from.restored_project;
        to.peaks=from.peaks; to.provenance.residualHistory=from.provenance.residualHistory;
        to.pattern.calculated=from.pattern.calculated;
        to.prepared_pfsm=from.prepared_pfsm;
        break;
    case WorkflowStep::rir: to.pfsm=from.pfsm; break;
    }
}
}
const char *workflowStepName(WorkflowStep s) {
    switch(s) {
    case WorkflowStep::range:return "Range"; case WorkflowStep::smoothing:return "Smoothing";
    case WorkflowStep::kalpha2:return "Kalpha2"; case WorkflowStep::background:return "Background";
    case WorkflowStep::initial_peaks:return "Initial Peaks"; case WorkflowStep::fwhm:return "FWHM";
    case WorkflowStep::peaks:return "Peak Extraction"; case WorkflowStep::search:return "Search";
    case WorkflowStep::phases:return "Phase Selection"; case WorkflowStep::rir:return "RIR Revision";
    } return "Unknown";
}
const WorkflowRecord *currentWorkflowRecord(const WorkflowState &s,WorkflowStep step) {
    for(auto i=s.records.rbegin();i!=s.records.rend();++i) if(i->step==step&&finished(*i)) return &*i;
    return nullptr;
}
bool reusableWorkflowStep(const ProductState &s,WorkflowId flow,WorkflowStep step) {
    if(!s.projectSettings) return true; // Existing explicit API contracts.
    if((step==WorkflowStep::background&&!s.pattern.background) ||
       (step==WorkflowStep::initial_peaks&&!s.preprocessing.initial_peak_usage) ||
       (step==WorkflowStep::fwhm&&!s.preprocessing.fwhm_usage) || (step==WorkflowStep::peaks&&!s.peaks.valid)) return false;
    const auto *r=currentWorkflowRecord(s.workflow,step);
    if(!r||!r->input||!sameMeasurement(*r->input,s.pattern)||s.provenance.legacyUnverified) return false;
    const bool fixed=step==WorkflowStep::fwhm&&s.preprocessing.fwhm_usage&&s.preprocessing.fwhm_usage->origin==FwhmOrigin::fixed;
    if(step!=WorkflowStep::background&&!fixed&&!sameSignal(r->input->background,s.pattern.background)) return false;
    const auto &a=s.algorithms->spec.selections();
    if(r->algorithms.background!=a.background) return false;
    if((step==WorkflowStep::initial_peaks||step==WorkflowStep::fwhm)&&r->algorithms.initial_peaks!=a.initial_peaks) return false;
    if(step==WorkflowStep::fwhm&&r->algorithms.fwhm!=a.fwhm) return false;
    if(step==WorkflowStep::peaks&&r->algorithms.experimental_peaks!=a.experimental_peaks) return false;
    if(!r->manual&&(r->workflow!=flow||!sameParameters(r->parameters,s.projectSettings->profile(flow),step))) return false;
    for(auto dependency:r->dependencies) {
        auto d=std::find_if(s.workflow.records.begin(),s.workflow.records.end(),[&](const auto &v){return v.id==dependency;});
        if(d==s.workflow.records.end()||!finished(*d)) return false;
    }
    return true;
}
Result<WorkflowPreparation> prepareWorkflow(ProductHandle source,WorkflowId flow,WorkflowStep target) {
    if(!source) return Result<WorkflowPreparation>{Error{ErrorCode::invalid_input,"workflow.source","Missing workflow source"}};
    WorkflowPreparation out{source,*source,target,{},{}};
    if(!source->projectSettings) return Result<WorkflowPreparation>{std::move(out)};
    auto valid=validateProjectSettings(*source->projectSettings); if(!valid) return Result<WorkflowPreparation>{valid.error()};
    auto &next=out.next; auto &c=next.preprocessing; const auto &p=source->projectSettings->profile(flow);
    next.workflow.active=flow;
    next.workflow.configuration_transition=true;
    const auto use=[&](WorkflowStep s,bool exists) {
        bool reusable=exists&&reusableWorkflowStep(*source,flow,s);
        (reusable?out.reused:out.required).push_back(s); return reusable;
    };
    const bool needsWidth=target==WorkflowStep::fwhm||target==WorkflowStep::peaks||target==WorkflowStep::search;
    const bool reusableWidth=source->preprocessing.fwhm_usage&&source->preprocessing.fwhm_usage->current&&
        reusableWorkflowStep(*source,flow,WorkflowStep::fwhm);
    const bool fixedWidth=reusableWidth&&source->preprocessing.fwhm_usage->origin==FwhmOrigin::fixed;
    const bool needsBackground=target!=WorkflowStep::fwhm||!p.fwhm.use_fixed;
    const bool needsInitial=target==WorkflowStep::initial_peaks||
        (needsWidth&&!reusableWidth&&!p.fwhm.use_fixed&&p.fwhm.method==FwhmMethod::gaussian&&flow!=WorkflowId::pfsm);
    const bool background=!needsBackground||use(WorkflowStep::background,bool(source->pattern.background));
    if(!background) {
        next.pattern.background.reset(); c.background_points.reset(); c.background_filters.clear();
        c.background=p.background; next.provenance.backgroundSource.reset();
        c.initial_peak_usage.reset();
        if(!fixedWidth) {c.fwhm_usage.reset(); c.fwhm_estimate.reset();}
        next.prepared_pfsm.reset();
    }
    const bool initial=!needsInitial||use(WorkflowStep::initial_peaks,background&&bool(c.initial_peak_usage));
    if(!initial) {c.initial_peak_usage.reset(); c.initial_peaks=p.initial_peaks;}
    const bool width=!needsWidth||use(WorkflowStep::fwhm,(background||fixedWidth)&&c.fwhm_usage&&c.fwhm_usage->current);
    if(!width) {c.fwhm=p.fwhm; c.fwhm_usage.reset(); c.fwhm_estimate.reset();}
    const bool needsPeaks=target==WorkflowStep::peaks||(target==WorkflowStep::search&&flow!=WorkflowId::pfsm);
    const bool peaks=!needsPeaks||use(WorkflowStep::peaks,background&&width&&next.peaks.valid);
    if(!peaks) {
        next.peaks.clear(); c.peak_candidates.reset(); c.peak_calibration.reset(); c.peak_fwhm_usage.reset();
        c.rejected_candidates.clear(); c.candidate_failure.reset(); c.peaks=p.peaks; c.selection=p.selection;
        next.peakTolerance.reset(); next.automaticDelta=.5; next.provenance.peakSource.reset();
    }
    c.smoothing=p.smoothing;
    for(auto &r:next.workflow.records) {
        const bool invalid=(r.step==WorkflowStep::background&&!background)||(r.step==WorkflowStep::initial_peaks&&!initial)||
            (r.step==WorkflowStep::fwhm&&!width)||(r.step==WorkflowStep::peaks&&!peaks);
        if(invalid&&finished(r)) invalidate(next.workflow,r.id);
    }
    if(flow==WorkflowId::pfsm) {
        out.required.erase(std::remove_if(out.required.begin(),out.required.end(),[](auto s){return s==WorkflowStep::initial_peaks||s==WorkflowStep::peaks;}),out.required.end());
        if(!width) c.fwhm->method=FwhmMethod::geometric;
    }
    if(flow==WorkflowId::preprocessing) {
        // Standalone commands always run their requested step.
        if(std::find(out.required.begin(),out.required.end(),target)==out.required.end()) out.required.push_back(target);
    }
    return Result<WorkflowPreparation>{std::move(out)};
}
Result<std::vector<WorkflowCheckpoint>> recordWorkflowTransition(ProductHandle source,ProductState &next) {
    std::vector<WorkflowCheckpoint> checkpoints;
    const auto target=next.workflow.completed_target; next.workflow.completed_target.reset();
    const bool configuring=next.workflow.configuration_transition; next.workflow.configuration_transition=false;
    if(configuring) return Result<std::vector<WorkflowCheckpoint>>{std::move(checkpoints)};
    if(!source||!next.projectSettings||next.workflow.imported_pattern!=source->workflow.imported_pattern||next.pattern.observed.empty())
        return Result<std::vector<WorkflowCheckpoint>>{std::move(checkpoints)};
    auto &state=next.workflow;
    auto add=[&](WorkflowStep step) -> Status {
        if(state.next_id==std::numeric_limits<std::uint64_t>::max()) return Status{Error{ErrorCode::exhausted_revision,"workflow.id","Workflow record IDs exhausted"}};
        WorkflowRecord r; r.id=state.next_id++; r.step=step; r.workflow=state.active;
        if(step==WorkflowStep::search||step==WorkflowStep::phases||step==WorkflowStep::rir)
            r.workflow=next.algorithms->spec.method()==SearchMethod::pfsm?WorkflowId::pfsm:WorkflowId::csm;
        r.manual=r.workflow==WorkflowId::preprocessing;
        if(step==WorkflowStep::fwhm&&next.preprocessing.fwhm_usage&&next.preprocessing.fwhm_usage->origin==FwhmOrigin::fixed)
            r.status=WorkflowStepStatus::skipped;
        r.parameters=next.projectSettings->profile(r.workflow); r.algorithms=next.algorithms->spec.selections();
        PreprocessingConfiguration actual;
        switch(step) {
        case WorkflowStep::background: actual.background=next.preprocessing.background; break;
        case WorkflowStep::initial_peaks: actual.initial_peaks=next.preprocessing.initial_peaks; break;
        case WorkflowStep::fwhm: actual.fwhm=next.preprocessing.fwhm; actual.fwhm_usage=next.preprocessing.fwhm_usage; break;
        case WorkflowStep::peaks: actual.peaks=next.preprocessing.peaks; actual.selection=next.preprocessing.selection; break;
        case WorkflowStep::smoothing: actual.smoothing=next.preprocessing.smoothing; break;
        default: break;
        }
        r.actual_parameters=preprocessingValues(actual);
        if(step==WorkflowStep::peaks&&!next.peaks.valid) r.actual_parameters["cleared"]=true;
        if(step==WorkflowStep::search || step==WorkflowStep::phases || step==WorkflowStep::rir) {
            if(next.pfsm) {
                const auto &p=*next.pfsm->problem;
                r.actual_parameters["PFSM"] = MetadataObject{
                    {"lower_shift",p.lower_shift},{"upper_shift",p.upper_shift},
                    {"equal_weights",p.source.equal_weights},{"template_cache",p.template_cache},
                    {"candidate_recall",int(p.candidate_recall.mode)},
                    {"ftol",p.fit.ftol},{"xtol",p.fit.xtol},{"gtol",p.fit.gtol},
                    {"maximum_evaluations",std::to_string(p.fit.maximum_evaluations)},
                    {"maximum_nnls_iterations",std::to_string(p.fit.maximum_nnls_iterations)},
                    {"threads_used",next.pfsm->performance.threads},
                    {"normalization_maximum",next.pfsm->normalization_maximum}};
                if(p.physical_width)r.actual_parameters["physical_width"]=fwhmUsageValues(*p.physical_width);
            } else {
                const AlgorithmRunSource *used=nullptr;
                if(!next.provenance.residualHistory.empty())used=&next.provenance.residualHistory.back();
                for(const auto &card:next.candidates)if(card.algorithmSource){used=&card.algorithmSource->score;break;}
                if(used) {
                    const auto &p=used->parameters;
                    r.actual_parameters["CSM"] = MetadataObject{
                        {"delta_two_theta",p.delta_two_theta.value},{"weight_position",p.weight_position},
                        {"weight_intensity",p.weight_intensity},{"weight_phases",p.weight_phases},
                        {"minimum_score",p.minimum_score},{"maximum_entries",std::to_string(p.maximum_entries)},
                        {"filter_strongest",p.filter_strongest},{"strongest_count",std::to_string(p.strongest_count)},
                        {"include_deleted_cards",p.include_deleted_cards},{"residual_search",p.residual_search}};
                }
            }
        }
        r.input=std::make_shared<const Pattern>(next.pattern);
        const auto depend=[&](WorkflowStep s) {if(const auto *d=currentWorkflowRecord(state,s)) r.dependencies.push_back(d->id);};
        for(auto s:{WorkflowStep::range,WorkflowStep::smoothing,WorkflowStep::kalpha2}) depend(s);
        if(step!=WorkflowStep::background&&step!=WorkflowStep::range&&step!=WorkflowStep::smoothing&&step!=WorkflowStep::kalpha2&&
            !(step==WorkflowStep::fwhm&&r.status==WorkflowStepStatus::skipped)) depend(WorkflowStep::background);
        if(step==WorkflowStep::fwhm&&next.preprocessing.fwhm_usage&&next.preprocessing.fwhm_usage->origin==FwhmOrigin::gaussian) depend(WorkflowStep::initial_peaks);
        if(step==WorkflowStep::peaks||step==WorkflowStep::search) depend(WorkflowStep::fwhm);
        if(step==WorkflowStep::search&&r.workflow==WorkflowId::csm) depend(WorkflowStep::peaks);
        if(step==WorkflowStep::search) depend(WorkflowStep::phases);
        if(step==WorkflowStep::phases||step==WorkflowStep::rir) {depend(WorkflowStep::search); depend(WorkflowStep::phases);}
        const bool transformStep=step==WorkflowStep::range||step==WorkflowStep::smoothing||step==WorkflowStep::kalpha2;
        if(!transformStep&&step!=WorkflowStep::phases&&step!=WorkflowStep::rir&&step!=WorkflowStep::search)
            if(const auto *old=currentWorkflowRecord(state,step)) invalidate(state,old->id);
        // A pattern transform invalidates every result bound to its old input.
        if(step==WorkflowStep::range||step==WorkflowStep::smoothing||step==WorkflowStep::kalpha2)
            for(auto &old:state.records) if(finished(old)&&old.step!=WorkflowStep::range&&old.step!=WorkflowStep::smoothing&&old.step!=WorkflowStep::kalpha2)
                old.status=WorkflowStepStatus::invalidated;
        checkpoints.push_back({r.id,step,source}); state.records.push_back(std::move(r));
        return Status{std::monostate{}};
    };
    std::vector<WorkflowStep> steps;
    const bool transform=!sameMeasurement(source->pattern,next.pattern);
    if(transform) steps.push_back(source->pattern.two_theta!=next.pattern.two_theta?WorkflowStep::range:
        source->pattern.processing.kalpha2_subtracted!=next.pattern.processing.kalpha2_subtracted?WorkflowStep::kalpha2:WorkflowStep::smoothing);
    else if(source->pattern.processing.kalpha2_subtracted!=next.pattern.processing.kalpha2_subtracted) steps.push_back(WorkflowStep::kalpha2);
    if(next.pattern.background&&(!sameSignal(source->pattern.background,next.pattern.background)||
        (next.provenance.backgroundSource&&(!source->provenance.backgroundSource||next.provenance.backgroundSource->sourceRevision!=source->provenance.backgroundSource->sourceRevision)))) steps.push_back(WorkflowStep::background);
    if(next.preprocessing.initial_peak_usage&&next.preprocessing.initial_peak_usage!=source->preprocessing.initial_peak_usage) steps.push_back(WorkflowStep::initial_peaks);
    if(next.preprocessing.fwhm_usage&&next.preprocessing.fwhm_usage->current&&
        (!source->preprocessing.fwhm_usage||fwhmUsageValues(*next.preprocessing.fwhm_usage)!=fwhmUsageValues(*source->preprocessing.fwhm_usage)||
         next.preprocessing.fwhm_usage->source_revision!=source->preprocessing.fwhm_usage->source_revision)) steps.push_back(WorkflowStep::fwhm);
    const bool phases=next.phases!=source->phases;
    const bool search=next.candidates!=source->candidates||next.pfsm!=source->pfsm;
    if(next.peaks!=source->peaks&&!phases&&!transform&&next.peaks.valid) steps.push_back(WorkflowStep::peaks);
    const bool preparing=!steps.empty() || (target && *target<=WorkflowStep::peaks);
    if(phases&&!preparing) steps.push_back(WorkflowStep::phases);
    else if(search&&!preparing&&(next.pfsm||!next.candidates.empty())&&target!=WorkflowStep::rir) steps.push_back(WorkflowStep::search);
    if(target&&std::find(steps.begin(),steps.end(),*target)==steps.end()) steps.push_back(*target);
    for(auto step:steps) {auto status=add(step); if(!status) return Result<std::vector<WorkflowCheckpoint>>{status.error()};}
    for(auto step:{WorkflowStep::background,WorkflowStep::initial_peaks,WorkflowStep::fwhm,WorkflowStep::peaks,WorkflowStep::search,WorkflowStep::phases})
        if(currentWorkflowRecord(source->workflow,step)&&!currentWorkflowRecord(state,step)) clearResult(next,step);
    return Result<std::vector<WorkflowCheckpoint>>{std::move(checkpoints)};
}
Result<WorkflowRollback> prepareRollback(ProductHandle source,std::span<const WorkflowCheckpoint> history,std::uint64_t id) {
    const auto fail=[] {return Result<WorkflowRollback>{Error{ErrorCode::invalid_input,"workflow.undo","This step has no undo checkpoint in the current session","undo_unavailable"}};};
    if(!source||source->editing) return fail();
    auto checkpoint=std::find_if(history.begin(),history.end(),[&](const auto &c){return c.record==id;});
    if(checkpoint==history.end()) return fail();
    auto record=std::find_if(source->workflow.records.begin(),source->workflow.records.end(),[&](const auto &r){return r.id==id;});
    if(record==source->workflow.records.end()||!finished(*record)) return fail();
    const auto closure=workflowDependents(source->workflow,id);
    std::set<std::uint64_t> affected(closure.begin(),closure.end());
    WorkflowRollback out{source,{affected.begin(),affected.end()},*source};
    for(auto i=history.rbegin();i!=history.rend();++i) if(affected.contains(i->record)) restoreStep(out.next,*i->before,i->step);
    auto &records=out.next.workflow.records;
    records.erase(std::remove_if(records.begin(),records.end(),[&](const auto &r){return affected.contains(r.id);}),records.end());
    for(auto &r:records) {
        const auto &old=checkpoint->before->workflow.records;
        auto previous=std::find_if(old.begin(),old.end(),[&](const auto &v){return v.id==r.id;});
        if(previous!=old.end()) r.status=previous->status;
    }
    if(out.next.projectSettings) {
        auto reconciled=prepareSettingsChange(std::make_shared<const ProductState>(out.next),*out.next.projectSettings);
        if(!reconciled) return Result<WorkflowRollback>{reconciled.error()};
        out.next=std::move(reconciled).value().next;
        out.next.workflow.configuration_transition=false;
    }
    auto valid=validateProductState(out.next);
    if(!valid) return Result<WorkflowRollback>{valid.error()};
    return Result<WorkflowRollback>{std::move(out)};
}
std::vector<std::uint64_t> workflowDependents(const WorkflowState &state,std::uint64_t id) {
    std::set<std::uint64_t> affected{id};
    for(const auto &r:state.records)
        if(std::any_of(r.dependencies.begin(),r.dependencies.end(),[&](auto d){return affected.contains(d);})) affected.insert(r.id);
    return {affected.begin(),affected.end()};
}
Status validateWorkflowState(const WorkflowState &state) {
    if(!state.next_id||static_cast<unsigned>(state.active)>3||
        (state.last_attempt&&(static_cast<unsigned>(state.last_attempt->target.workflow)>3||
         static_cast<unsigned>(state.last_attempt->target.step)>static_cast<unsigned>(WorkflowStep::rir)||
         static_cast<unsigned>(state.last_attempt->status)>static_cast<unsigned>(WorkflowStepStatus::invalidated))))
        return Status{Error{ErrorCode::invalid_input,"workflow.activity","Invalid workflow activity"}};
    std::set<std::uint64_t> seen;
    for(const auto &r:state.records) {
        if(!r.id||r.id>=state.next_id||!seen.insert(r.id).second||static_cast<unsigned>(r.workflow)>3||
            static_cast<unsigned>(r.step)>static_cast<unsigned>(WorkflowStep::rir)||
            static_cast<unsigned>(r.status)>static_cast<unsigned>(WorkflowStepStatus::invalidated))
            return Status{Error{ErrorCode::invalid_input,"workflow.records","Invalid workflow record"}};
        for(auto d:r.dependencies) if(d>=r.id||!seen.contains(d))
            return Status{Error{ErrorCode::invalid_input,"workflow.dependencies","Invalid workflow dependency"}};
    }
    return Status{std::monostate{}};
}
Result<WorkflowSettingsChange> prepareSettingsChange(ProductHandle source,const ProjectSettings &settings) {
    auto valid=validateProjectSettings(settings);
    if(!valid) return Result<WorkflowSettingsChange>{valid.error()};
    if(!source||source->editing) return Result<WorkflowSettingsChange>{Error{ErrorCode::closed_edit,"settings","Finish the active edit first"}};
    WorkflowSettingsChange out{source,*source,{}}; out.next.projectSettings=settings;
    out.next.workflow.configuration_transition=true;
    if (source->projectSettings) {
        const auto &a=source->projectSettings->profile(WorkflowId::pfsm), &b=settings.profile(WorkflowId::pfsm);
        auto pa=a.pfsm,pb=b.pfsm; pa.threads=pb.threads=0; pa.minimum_relative_fom=pb.minimum_relative_fom=0;
        MetadataObject before,after; storePfsmSettings(before,pa); storePfsmSettings(after,pb);
        if (before!=after || !sameBackgroundOptions(a.background,b.background) || a.fwhm!=b.fwhm)
            out.next.prepared_pfsm.reset();
    }
    std::set<std::uint64_t> affected;
    for(const auto &r:source->workflow.records) if(finished(r)) {
        const auto &p=settings.profile(r.workflow);
        bool changed=!sameParameters(r.parameters,p,r.step);
        if(r.step==WorkflowStep::search) {
            if(r.workflow==WorkflowId::csm) changed=csmSettingValues(r.parameters.csm)!=csmSettingValues(p.csm);
            else {
                MetadataObject before,after; auto a=r.parameters.pfsm,b=p.pfsm;
                a.minimum_relative_fom=b.minimum_relative_fom=0; a.threads=b.threads=0;
                storePfsmSettings(before,a); storePfsmSettings(after,b); changed=before!=after;
            }
        }
        if(changed||std::any_of(r.dependencies.begin(),r.dependencies.end(),[&](auto d){return affected.contains(d);})) affected.insert(r.id);
    }
    for(auto &r:out.next.workflow.records) if(affected.contains(r.id)) {
        r.status=WorkflowStepStatus::invalidated;
        clearResult(out.next,r.step);
    }
    auto options=source->searchOptions;
    for(const auto &[k,v]:csmSettingValues(settings.profile(WorkflowId::csm).csm)) options[k]=v;
    storePfsmSettings(options,settings.profile(WorkflowId::pfsm).pfsm); out.next.searchOptions=std::move(options);
    out.affected.assign(affected.begin(),affected.end());
    return Result<WorkflowSettingsChange>{std::move(out)};
}
} // namespace qualpf::application
