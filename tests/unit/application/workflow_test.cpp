#include "qualpf/application/workflow.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/search_preparation.h"
#include "qualpf/application/configured_fwhm.h"
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v,std::source_location at=std::source_location::current()) {
    if(!v) throw std::runtime_error("Workflow contract line "+std::to_string(at.line()));
}
template<class T> static T take(Result<T> r) {
    if(!r) throw std::runtime_error(r.error().field+": "+r.error().message);
    return std::move(r).value();
}
struct Defaults final:DefaultSettingsStore {
    ProjectSettings settings;
    Result<ProjectSettings> load() const override {return Result<ProjectSettings>{settings};}
    Status save(const ProjectSettings &p) override {settings=p;return Status{std::monostate{}};}
};
struct Background final:preprocessing::IBackgroundEstimator {
    mutable std::vector<int> received;
    AlgorithmIdentity identity() const override {return {"qualpf.background.cpp",1};}
    Result<preprocessing::BackgroundResult> estimate(const Pattern &p,const preprocessing::BackgroundOptions &o,
        WavelengthAngstrom,std::optional<std::span<const preprocessing::BackgroundPoint>>,const ExecutionContext &) const override {
        received.push_back(o.coefficients);
        preprocessing::BackgroundResult r; r.values.assign(p.observed.size(),20+.001*o.coefficients);
        return Result<preprocessing::BackgroundResult>{std::move(r)};
    }
};
struct Initial final:preprocessing::IInitialPeakFinder {
    mutable std::vector<float> received;
    AlgorithmIdentity identity() const override {return {"qualpf.peaks.initial.cpp",5};}
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &s,WavelengthAngstrom,
        const preprocessing::InitialPeakOptions &o,const ExecutionContext &) const override {
        received.push_back(o.slope); preprocessing::InitialPeakResult r; r.dataset=s.pattern().id; r.candidates={{10}};
        r.applied.slope=o.slope; r.applied.automatic_window=o.automatic_window;
        r.applied.half_window=o.half_window; r.applied.polynomial_degree=o.polynomial_degree;
        return Result<preprocessing::InitialPeakResult>{r};
    }
};
struct Width final:preprocessing::IFwhmEstimator {
    mutable int calls=0; bool fail=false;
    AlgorithmIdentity identity() const override {return {"qualpf.peaks.fwhm.cpp",3};}
    Result<preprocessing::FwhmEstimationResult> estimate(const AnalysisSpectrum &,const preprocessing::InitialPeakResult &,
        const ExecutionContext &) const override {
        ++calls;
        if(fail) return Result<preprocessing::FwhmEstimationResult>{Error{ErrorCode::numerical_failure,"test","injected failure"}};
        preprocessing::FwhmEstimationResult r; r.estimate={preprocessing::FwhmEstimate::State::measured,AngularWidthDegrees{.2},1};
        return Result<preprocessing::FwhmEstimationResult>{r};
    }
};
struct Formal final:preprocessing::IExperimentalPeakExtractor {
    mutable std::vector<float> received;
    AlgorithmIdentity identity() const override {return {"qualpf.peaks.experimental.cpp",3};}
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &s,const preprocessing::PeakCalibration &c,
        WavelengthAngstrom w,AngularWidthDegrees,const ExecutionContext &) const override {
        received.push_back(c.applied.slope); preprocessing::PeakSearchResult r; r.applied=c.applied;
        r.peaks.dataset=r.candidates.dataset=s.pattern().id; r.peaks.effective_wavelength=r.candidates.effective_wavelength=w;
        r.automatic_tolerance=.5; return Result<preprocessing::PeakSearchResult>{r};
    }
};
static Pattern triangle() {
    Pattern p; p.id={81};p.source_id="workflow triangle";p.radiation.lines={{WavelengthAngstrom{1.54},1}};
    p.processing={false,false,false};
    for(int i=0;i<=20;++i) {p.two_theta.emplace_back(20+.01*i);p.observed.push_back(28-std::abs(i-10));}
    p.original_observed=p.observed;p.original_kalpha2_subtracted=false;return p;
}
int main() try {
    Defaults defaults;
    int coefficient=3;float slope=.11f;
    for(auto flow:workflows) {
        auto &p=defaults.settings.profile(flow);p.background.coefficients=coefficient;coefficient+=2;
        p.initial_peaks.options.slope=slope;p.peaks.slope=slope;slope+=.1f;
    }
    auto background=std::make_shared<Background>();auto initial=std::make_shared<Initial>();
    auto width=std::make_shared<Width>();auto formal=std::make_shared<Formal>();
    AlgorithmModules modules;modules.background=background;modules.initial_peaks=initial;modules.fwhm=width;modules.experimental_peaks=formal;
    auto algorithms=take(resolveAlgorithms(AlgorithmSpec{},modules));
    Workspace workspace(SearchBackend::cpp,algorithms,&defaults);auto &session=workspace.session();
    auto open=[&] {take(session.openPattern(session.snapshot(),triangle()));};
    const ExecutionContext execution{{},NumericalPolicy::legacy_mixed_precision,"workflow-test"};
    open();
    auto automatic=[&](PreprocessingAction action) {
        auto source=session.snapshot();auto request=take(automaticPreprocessingRequest(source,action));
        PreprocessingEvaluation evaluation(source);take(evaluation.preview(request));
        take(session.commit(source,*evaluation.preview()));
    };
    automatic(PreprocessingAction::prepare_search);
    check(background->received.back()==5);
    check(initial->received.back()==defaults.settings.profile(WorkflowId::csm).initial_peaks.options.slope);
    check(formal->received.back()==defaults.settings.profile(WorkflowId::csm).peaks.slope);
    if(session.snapshot()->workflow.records.size()!=4) {
        for(const auto &record:session.snapshot()->workflow.records) std::cerr<<workflowStepName(record.step)<<'\n';
        std::cerr<<"initial evidence="<<bool(session.snapshot()->preprocessing.initial_peak_usage)<<" width="<<bool(session.snapshot()->preprocessing.fwhm_usage)<<'\n';
    }
    check(session.snapshot()->workflow.records.size()==4);
    check(!reusableWorkflowStep(*session.snapshot(),WorkflowId::peak_search,WorkflowStep::background));
    automatic(PreprocessingAction::automatic_peaks);
    check(background->received.back()==9&&initial->received.back()==defaults.settings.profile(WorkflowId::peak_search).initial_peaks.options.slope);
    check(!currentWorkflowRecord(session.snapshot()->workflow,WorkflowStep::background)->manual);
    open();automatic(PreprocessingAction::generate_background);
    const auto manual=session.snapshot();const auto bgId=manual->workflow.records.back().id;
    const auto calls=background->received.size();
    automatic(PreprocessingAction::prepare_search);
    check(background->received.size()==calls&&session.snapshot()->pattern.background->values.front()==20.003);
    auto source=session.snapshot();auto search=*source;search.workflow.completed_target=WorkflowStep::search;
    take(session.commit(source,std::move(search)));
    source=session.snapshot();auto phases=*source;phases.workflow.completed_target=WorkflowStep::phases;
    take(session.commit(source,std::move(phases)));
    auto beforeUndo=session.snapshot();auto rollback=take(workspace.prepareRollback(bgId));
    check(rollback.affected.size()==6&&session.snapshot()==beforeUndo); // Preview/cancel has no side effects.
    auto changed=*beforeUndo->projectSettings;changed.profile(WorkflowId::pfsm).pfsm.threads=2;
    take(workspace.applySettings(take(prepareSettingsChange(beforeUndo,changed))));
    check(!workspace.commitRollback(rollback)); // A stale confirmation cannot restore anything.
    rollback=take(workspace.prepareRollback(bgId));const auto revision=session.snapshot()->revision;
    take(workspace.commitRollback(rollback));
    check(!session.snapshot()->pattern.background&&!session.snapshot()->preprocessing.fwhm_usage&&!session.snapshot()->peaks.valid);
    check(session.snapshot()->workflow.records.empty()&&session.snapshot()->revision==revision+1);
    check(session.snapshot()->projectSettings->profile(WorkflowId::pfsm).pfsm.threads==2);
    // Replacing a prerequisite and undoing the replacement restores its prior dependents.
    open();automatic(PreprocessingAction::generate_background);automatic(PreprocessingAction::prepare_search);
    const auto beforeReplacement=session.snapshot();automatic(PreprocessingAction::generate_background);
    check(!session.snapshot()->peaks.valid);
    auto replacement=currentWorkflowRecord(session.snapshot()->workflow,WorkflowStep::background)->id;
    take(workspace.commitRollback(take(workspace.prepareRollback(replacement))));
    check(session.snapshot()->peaks==beforeReplacement->peaks && session.snapshot()->preprocessing.initial_peak_usage==beforeReplacement->preprocessing.initial_peak_usage);
    source=session.snapshot();PreprocessingRequest erase;erase.manual=ManualPeakChange{};erase.manual->operation=ManualPeakChange::Operation::clear;
    PreprocessingEvaluation clear(source);take(clear.preview(erase));take(session.commit(source,*clear.preview()));
    const auto clearId=currentWorkflowRecord(session.snapshot()->workflow,WorkflowStep::peaks)->id;
    check(!session.snapshot()->peaks.valid && !reusableWorkflowStep(*session.snapshot(),WorkflowId::csm,WorkflowStep::peaks));
    take(workspace.commitRollback(take(workspace.prepareRollback(clearId))));check(session.snapshot()->peaks==source->peaks);
    open();
    // Direct FWHM uses Preprocessing, publishes prerequisite evidence, and can fail atomically.
    source=session.snapshot();width->fail=true;
    auto failed=evaluateConfiguredFwhm(source,{source->pattern,defaults.settings.profile(WorkflowId::preprocessing).fwhm,{},true},execution);
    check(!failed&&session.snapshot()==source);width->fail=false;
    auto estimated=take(evaluateConfiguredFwhm(source,{source->pattern,defaults.settings.profile(WorkflowId::preprocessing).fwhm,{},true},execution));
    auto next=*source;take(applyConfiguredFwhm(next,estimated));take(session.commit(source,std::move(next)));
    check(background->received.back()==3&&initial->received.back()==.11f&&session.snapshot()->workflow.records.size()==3);
    const auto initialCalls=initial->received.size();source=session.snapshot();
    estimated=take(evaluateConfiguredFwhm(source,{source->pattern,defaults.settings.profile(WorkflowId::preprocessing).fwhm,{},true},execution));
    check(initial->received.size()==initialCalls); // Exact manual evidence reuse.
    auto task=take(workspace.searches().begin());check(!workspace.prepareRollback(bgId));
    check(!workspace.applySettings(take(prepareSettingsChange(source,*source->projectSettings))));
    workspace.searches().cancel();
    auto cancelled=workspace.searches().complete(task.id,Result<PreparedSearchResult>{Error{ErrorCode::cancelled,"test","cancelled"}});
    check(!cancelled);
    // Fixed direct width has no background or initial dependency.
    open();source=session.snapshot();auto fixed=defaults.settings.profile(WorkflowId::preprocessing).fwhm;fixed.use_fixed=true;
    auto settings=*source->projectSettings;settings.profile(WorkflowId::preprocessing).fwhm=fixed;
    take(workspace.applySettings(take(prepareSettingsChange(source,settings))));source=session.snapshot();
    auto planned=take(prepareWorkflow(source,WorkflowId::preprocessing,WorkflowStep::fwhm));
    check(planned.required==std::vector{WorkflowStep::fwhm});
    estimated=take(evaluateConfiguredFwhm(source,{source->pattern,fixed,{},true},execution));
    next=*source;take(applyConfiguredFwhm(next,estimated));take(session.commit(source,std::move(next)));
    check(session.snapshot()->workflow.records.size()==1&&!session.snapshot()->pattern.background);
    check(session.snapshot()->workflow.records.front().status==WorkflowStepStatus::skipped);
    ProductSearchSession reopened(*session.snapshot());check(!reopened.canUndo(session.snapshot()->workflow.records.front().id));
    const auto fixedInitial=initial->received.size();
    automatic(PreprocessingAction::prepare_search);
    check(initial->received.size()==fixedInitial&&session.snapshot()->preprocessing.fwhm_usage->origin==FwhmOrigin::fixed);
    // PFSM prepares its own background and geometric width without invoking initial/formal peaks.
    OriginalSignalProvenance provenance{"workflow triangle","counts",true,false};
    take(session.openPattern(session.snapshot(),triangle(),{},false,provenance));
    source=session.snapshot();take(session.commit(source,take(prepareSearchMethod(source,SearchMethod::pfsm,false))));
    source=session.snapshot();check(take(prepareSearch(source,SearchMethod::pfsm)).state==SearchPreparation::State::needs_pfsm_preparation);
    const auto beforeInitial=initial->received.size(),beforeFormal=formal->received.size();
    auto precise=execution;precise.numerical_policy=NumericalPolicy::explicit_float64;
    next=take(evaluatePfsmPreparation(source,precise));
    check(background->received.back()==7&&initial->received.size()==beforeInitial&&formal->received.size()==beforeFormal);
    check(next.prepared_pfsm&&next.prepared_pfsm->physical_width->origin==FwhmOrigin::geometric);
    take(session.commit(source,std::move(next)));
    check(session.snapshot()->workflow.records.size()==2&&session.snapshot()->prepared_pfsm);
    check(take(prepareSearch(session.snapshot(),SearchMethod::pfsm)).state==SearchPreparation::State::ready);
    check(!currentWorkflowRecord(session.snapshot()->workflow,WorkflowStep::initial_peaks));
    rollback=take(workspace.prepareRollback(session.snapshot()->workflow.records.front().id));
    take(workspace.commitRollback(rollback));check(!session.snapshot()->prepared_pfsm);
    source=session.snapshot();const auto recordCount=source->workflow.records.size();
    task=take(workspace.searches().begin({},WorkflowTarget{WorkflowId::pfsm,WorkflowStep::search}));
    check(workspace.searches().activity()->status==WorkflowStepStatus::running);
    auto failure=workspace.searches().complete(task.id,Result<PreparedSearchResult>{Error{ErrorCode::numerical_failure,"test","injected search failure"}});
    check(!failure&&session.snapshot()->workflow.last_attempt->status==WorkflowStepStatus::failed);
    check(session.snapshot()->workflow.records.size()==recordCount&&session.snapshot()->pattern.observed==source->pattern.observed);
    check(session.snapshot()->projectSettings->profile(WorkflowId::pfsm).pfsm.lower_shift==source->projectSettings->profile(WorkflowId::pfsm).pfsm.lower_shift);
    task=take(workspace.searches().begin({},WorkflowTarget{WorkflowId::pfsm,WorkflowStep::search}));workspace.searches().cancel();
    failure=workspace.searches().complete(task.id,Result<PreparedSearchResult>{Error{ErrorCode::cancelled,"test","cancelled"}});
    check(!failure&&session.snapshot()->workflow.last_attempt->status==WorkflowStepStatus::cancelled);
    // Native preview dialogs commit only their requested parameters, with their output.
    open();source=session.snapshot();
    const auto savedCsm=source->projectSettings->profile(WorkflowId::csm).background.coefficients;
    {
        ProductPreprocessingEdit edit(session,true);
        PreprocessingRequest request;request.background=source->projectSettings->profile(WorkflowId::preprocessing).background;
        request.background->coefficients=11;request.wavelength=WavelengthAngstrom{1.54};request.tolerance=AngularWidthDegrees{.1};
        take(edit.preview(request));
        check(session.snapshot()->projectSettings->profile(WorkflowId::preprocessing).background.coefficients!=11);
        take(edit.commit());
    }
    check(session.snapshot()->projectSettings->profile(WorkflowId::preprocessing).background.coefficients==11);
    check(session.snapshot()->projectSettings->profile(WorkflowId::csm).background.coefficients==savedCsm);
    check(defaults.settings.profile(WorkflowId::preprocessing).background.coefficients==3);
    check(session.snapshot()->workflow.records.back().parameters.background.coefficients==11);
    {
        ProductPreprocessingEdit edit(session,true);PreprocessingRequest request;
        request.background=session.snapshot()->projectSettings->profile(WorkflowId::preprocessing).background;
        request.background->coefficients=13;request.wavelength=WavelengthAngstrom{1.54};request.tolerance=AngularWidthDegrees{.1};
        take(edit.preview(request));edit.cancel();
    }
    check(session.snapshot()->projectSettings->profile(WorkflowId::preprocessing).background.coefficients==11);
    // An outlying aperture remains accepted by the native Range editor. Its
    // saved bounds describe the physical domain, not the sample-aligned result.
    open();
    {
        ProductPreprocessingEdit edit(session,true);PreprocessingRequest request;
        request.crop_range=AngularRange{TwoThetaDegrees{-5},TwoThetaDegrees{200}};
        take(edit.previewRange(request));take(edit.commit());
    }
    const auto range=session.snapshot()->projectSettings->profile(WorkflowId::preprocessing).range;
    check(range && range->minimum.value==0 && range->maximum.value==180);
    check(session.snapshot()->pattern.two_theta==triangle().two_theta);
    std::cout<<"Workflow isolation, prerequisite evidence, atomic failure and cascading undo passed\n";
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
