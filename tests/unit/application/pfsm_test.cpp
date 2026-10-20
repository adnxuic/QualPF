#include "qualpf/application/pfsm.h"
#include "qualpf/application/product_files.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/search_preparation.h"
#include "qualpf/application/configured_fwhm.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <omp.h>
#include <source_location>
#include <stdexcept>
#include <thread>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok)
        throw std::runtime_error("PFSM Application contract line " + std::to_string(at.line()));
}
template <class T> static T take(Result<T> r) {
    if (!r)
        throw std::runtime_error(r.error().field + ": " + r.error().message);
    return std::move(r).value();
}
static double il(double x, double q) {
    return std::pow(1 + 4 * (std::pow(2., 2. / 3) - 1) * std::pow((x - q) / .2, 2), -1.5);
}
struct Reader final : CandidateReader {
    int calls = 0;
    std::thread::id execution;
    std::vector<ProductCard> cards;
    Result<std::vector<ProductCard>> read(const CandidateQuery &query, const ExecutionContext &,
                                          SearchProgress) override {
        ++calls;
        execution = std::this_thread::get_id();
        check(query.route == CandidateQuery::Route::constrained_browse && query.strictPairedReflections &&
              query.wavelength == 0);
        return Result<std::vector<ProductCard>>{cards};
    }
};
static ProductCard card(std::string id, std::initializer_list<double> positions) {
    ProductCard result;
    result.id = std::move(id);
    for (double q : positions) {
        result.d.push_back(1. / (2 * std::sin(q * std::numbers::pi / 360)));
        result.intensity.push_back(1);
    }
    return result;
}
static ProductState state(AlgorithmHandle algorithms = {}) {
    ProductState result;
    result.algorithms =
        algorithms ? algorithms : take(resolveAlgorithms(AlgorithmSpec{{}, SearchMethod::pfsm}));
    auto &p = result.pattern;
    p.id = {17};
    p.source_id = "analytic-IL-mixture";
    p.radiation.lines = {{WavelengthAngstrom{1}, 1}};
    p.processing = {false, false, false};
    p.calculation_range = AngularRange{TwoThetaDegrees{28}, TwoThetaDegrees{35}};
    for (int i = 0; i <= 700; ++i) {
        double x = 28 + .01 * i;
        p.two_theta.emplace_back(x);
        p.observed.push_back(7 + 100 * il(x, 30.061) + 30 * il(x, 32.964));
    }
    return result;
}
static std::shared_ptr<const PfsmRequest> request(int threads = 1, bool cache = false) {
    auto r = std::make_shared<PfsmRequest>();
    r->threads = threads;
    r->template_cache = cache;
    r->lower_shift = -.12;
    r->upper_shift = .12;
    r->observations.emplace();
    auto &o = *r->observations;
    o.signal_source = "independent analytic IL mixture";
    o.intensity_units = "arbitrary intensity";
    o.noise_source = "no variance; explicit equal weights";
    o.unclipped_signal = true;
    o.equal_weights = true;
    o.background = std::vector<double>(701, 7);
    return r;
}
static Reader reader() {
    Reader r;
    r.cards = {card("wrong-a", {29}),        card("weak", {33}),    card("strong", {30}),
               card("wrong-b", {31.2}),      card("wrong-c", {34}), card("wrong-d", {29.4, 31.4}),
               card("wrong-e", {28.5, 34.5})};
    return r;
}
static SearchInput input(ProductHandle source, int threads = 1) {
    SearchInput in;
    in.source = std::move(source);
    in.method = SearchMethod::pfsm;
    in.databaseVersion = 19;
    in.pfsm = request(threads);
    in.parameters.maximum_entries = 1;
    return in;
}
static void deterministic(const ProductState &a, const ProductState &b) {
    check(a.candidates.size() == b.candidates.size());
    for (std::size_t i = 0; i < a.candidates.size(); ++i) {
        const auto &x = a.candidates[i], &y = b.candidates[i];
        check(x.id == y.id && x.fom == y.fom && x.scale == y.scale && x.pfsm->q == y.pfsm->q &&
              x.pfsm->parameters.shifts == y.pfsm->parameters.shifts &&
              x.pfsm->parameters.scales == y.pfsm->parameters.scales);
    }
}
static void workflow(bool cache = false) {
    ProductSearchSession session(state());
    SearchCoordinator coordinator(session);
    auto task = take(coordinator.begin({"fixture", 19}));
    auto original = session.snapshot();
    check(task.context.numerical_policy == NumericalPolicy::explicit_float64);
    auto prep = take(prepareSearch(original, SearchMethod::pfsm));
    check(!prep.peaks && prep.state == SearchPreparation::State::ready);
    auto read = reader();
    auto in = input(original);
    in.pfsm = request(1, cache);
    in.taskId = task.id;
    auto first = take(SearchEngine{}.run(in, read, task.context));
    check(session.snapshot() == original && read.execution == std::this_thread::get_id() && read.calls == 1);
    check(first.next.candidates.size() == 7 && first.next.candidates.front().id == "strong");
    check(first.next.pfsm->universe->size() == 7 && first.next.pfsm->selected.empty() &&
          first.next.pfsm->baseline.rwp_net == 1);
    check(first.next.peaks.tth.empty() && first.next.pfsm->problem->width.default_fwhm);
    check(std::abs(first.next.pfsm->problem->width.default_fwhm->value - .2) < .001);
    for (int threads : {2, 4, omp_get_max_threads()}) {
        in.pfsm = request(threads, cache);
        std::size_t completed = 0;
        const auto owner = std::this_thread::get_id();
        auto parallel =
            take(SearchEngine{}.run(in, read, task.context, [&](std::size_t done, std::size_t total) {
                check(std::this_thread::get_id() == owner && done >= completed && done <= total);
                completed = done;
            }));
        check(completed == read.cards.size());
        deterministic(first.next, parallel.next);
    }
    auto reordered = reader();
    std::reverse(reordered.cards.begin(), reordered.cards.end());
    auto order = take(SearchEngine{}.run(in, reordered, task.context));
    deterministic(first.next, order.next);
    auto published = coordinator.complete(task.id, Result<PreparedSearchResult>{first});
    check(bool(published));
    const auto before = session.snapshot();
    check(before != original && bool(validateProjectSaveSource(before)));
    SearchParameters acceptanceParameters;
    auto intent = take(phaseAcceptanceInput(before, "strong", acceptanceParameters, true));
    check(bool(intent));
    auto acceptedTask = take(coordinator.begin({"fixture", 19}));
    intent->taskId = acceptedTask.id;
    intent->databaseVersion = 19;
    auto second = take(SearchEngine{}.run(*intent, read, acceptedTask.context));
    check(second.next.phases.size() == 1 && second.next.candidates.size() == 6 &&
          second.next.candidates.front().id == "weak");
    check(second.next.pfsm->universe == before->pfsm->universe && session.snapshot() == before &&
          before->phases.empty());
    check(second.next.pfsm->baseline.q <= before->candidates.front().pfsm->q + 1e-9);
    check(second.next.pfsm->baseline.parameters.shifts[0] > .06 &&
          second.next.pfsm->baseline.parameters.shifts[0] < .062);
    check(bool(coordinator.complete(acceptedTask.id, Result<PreparedSearchResult>{second})));
    auto weak = take(phaseAcceptanceInput(session.snapshot(), "weak", acceptanceParameters, true));
    auto weakTask = take(coordinator.begin({"fixture", 19}));
    weak->taskId = weakTask.id;
    weak->databaseVersion = 19;
    auto third = take(SearchEngine{}.run(*weak, read, weakTask.context));
    check(third.next.phases.size() == 2 && third.next.pfsm->baseline.rwp_net < .001);
    check(bool(coordinator.complete(weakTask.id, Result<PreparedSearchResult>{third})));
    check(third.next.pfsm->baseline.diagnostics.exact_final_verification);
    check(third.next.pfsm->baseline.diagnostics.interpolated_iterations == cache);
    const auto full = session.snapshot();
    auto remove = take(phaseRemovalInput(full, "weak"));
    remove.databaseVersion = 19;
    remove.pfsm = std::make_shared<const PfsmRequest>();
    auto removed = take(SearchEngine{}.run(remove, read, weakTask.context));
    check(removed.next.phases.size() == 1 && removed.next.phases[0].id == "strong" &&
          removed.next.candidates.size() == 6 && removed.next.candidates.front().id == "weak");
    check(removed.next.pfsm->universe == full->pfsm->universe && session.snapshot() == full);
    check(bool(validatePfsmState(removed.next)));
    auto one = std::make_shared<const ProductState>(removed.next);
    auto removeLast = take(phaseRemovalInput(one, "strong"));
    removeLast.databaseVersion = 19;
    auto empty = take(SearchEngine{}.run(removeLast, read, weakTask.context));
    check(empty.next.phases.empty() && empty.next.candidates.size() == 7 && empty.next.pfsm->baseline.rwp_net == 1);
    auto invalidExact = third.next;
    auto invalidAnalysis = std::make_shared<PfsmAnalysis>(*invalidExact.pfsm);
    invalidAnalysis->baseline.diagnostics.exact_final_verification = false;
    invalidExact.pfsm = invalidAnalysis;
    check(!validatePfsmState(invalidExact));
    auto stale = *weak;
    stale.source = session.snapshot();
    auto duplicate = take(SearchEngine{}.run(stale, read, weakTask.context));
    check(!duplicate.changed);
    stale.phase = *std::find_if(first.next.candidates.begin(), first.next.candidates.end(),
                                [](const auto &c) { return c.id == "wrong-a"; });
    check(!SearchEngine{}.run(stale, read, weakTask.context));
    auto changed = *session.snapshot();
    changed.pattern.observed[0] += 1;
    check(bool(session.commit(session.snapshot(), std::move(changed))) && !session.snapshot()->pfsm);
}
static void presentationAndWidths() {
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    auto s = state();
    s.pattern.original_observed = s.pattern.observed;
    s.pattern.original_kalpha2_subtracted = false;
    s.original_signal_provenance = OriginalSignalProvenance{s.pattern.source_id, "analytic units", true, false};
    ProductSearchSession session(s);
    PfsmSettings settings;
    check(settings.candidate_recall_enabled && PfsmRequest{}.candidate_recall.mode == PfsmCandidateRecallMode::off);
    check(pfsmSettings({}).candidate_recall_enabled);
    settings.threads = 1;
    settings.lower_shift = settings.upper_shift = 0;
    MetadataObject values{{"SearchOptions/minFom", .3}, {"vendor/unknown", "retained"}};
    storePfsmSettings(values, settings);
    check(values["SearchOptions/minFom"].toDouble() == .3 && values["vendor/unknown"].toString() == "retained");
    check(pfsmSettings(values).lower_shift == 0 && bool(validatePfsmSettings(settings)));
    {
        auto saved = values;
        saved["PFSMOptions/candidateRecall/enabled"] = false;
        saved["PFSMOptions/candidateRecall/localShortlist"] = 7;
        saved["PFSMOptions/candidateRecall/future"] = MetadataArray{1,"unknown"};
        const auto before = saved;
        std::optional<Error> warning;
        const auto parsed = pfsmSettings(saved, &warning);
        check(!warning && !parsed.candidate_recall_enabled && parsed.candidate_recall_limits.local_shortlist == 7);
        check(saved == before);
        for (const auto &bad : {MetadataValue{0}, MetadataValue{-1}, MetadataValue{1.5},
                               MetadataValue{std::int64_t{2147483648LL}}, MetadataValue{"future"}}) {
            saved["PFSMOptions/candidateRecall/recallCandidates"] = bad;
            const auto original = saved;
            const auto recovered = pfsmSettings(saved, &warning);
            check(warning && recovered.candidate_recall_limits == profile::PfsmCandidateRecallLimits{});
            check(!recovered.candidate_recall_enabled && recovered.lower_shift == 0 && recovered.threads == 1);
            check(saved == original);
        }
        storePfsmSettings(saved, parsed);
        check(saved["PFSMOptions/candidateRecall/future"] == before.at("PFSMOptions/candidateRecall/future"));
    }
    settings.minimum_relative_fom = 1.01;
    check(!validatePfsmSettings(settings));
    settings.minimum_relative_fom = 0;
    auto request = take(makePfsmRequest(session.snapshot(), settings));
    check(request.candidate_recall.mode == PfsmCandidateRecallMode::enabled &&
          request.candidate_recall.limits == settings.candidate_recall_limits);
    check(!request.supplied_width && request.observations->equal_weights &&
          request.observations->signal == PfsmObservationRequest::Signal::observed);
    auto statistical = s;
    statistical.original_signal_provenance->standard_deviations = std::vector<double>(701, 2);
    statistical.original_signal_provenance->noise_source = "independently supplied sigma";
    auto statisticalSource = std::make_shared<const ProductState>(statistical);
    check(!take(makePfsmRequest(statisticalSource, settings)).observations->equal_weights);
    // Range keeps the original measurements on the cropped grid. Importer
    // standard deviations still belong to the full grid and cannot be reused.
    auto cropped = statistical;
    cropped.pattern.two_theta.erase(cropped.pattern.two_theta.begin());
    cropped.pattern.observed.erase(cropped.pattern.observed.begin());
    cropped.pattern.original_observed->erase(cropped.pattern.original_observed->begin());
    cropped.pattern.calculation_range->minimum = cropped.pattern.two_theta.front();
    auto croppedRequest = take(makePfsmRequest(std::make_shared<const ProductState>(cropped), settings));
    check(croppedRequest.observations->equal_weights &&
          std::holds_alternative<std::monostate>(croppedRequest.observations->noise));
    check(croppedRequest.observations->noise_source == "Equal weights: working signal statistics unavailable");
    auto transformed = statistical;
    transformed.pattern.observed.front() += 1;
    transformed.preprocessing.smoothing_applied = true;
    transformed.pattern.background = SampledSignal{std::vector<double>(701,9),SampleWindow{0}};
    auto transformedRequest = take(makePfsmRequest(std::make_shared<const ProductState>(transformed), settings));
    check(transformedRequest.observations->signal == PfsmObservationRequest::Signal::observed &&
        transformedRequest.observations->equal_weights && transformedRequest.observations->background->front() == 9);
    settings.weighting = PfsmSettings::Weighting::equal;
    check(take(makePfsmRequest(statisticalSource, settings)).observations->equal_weights);
    settings.weighting = PfsmSettings::Weighting::automatic;
    auto next = *session.snapshot();
    FwhmUsage manual;
    manual.width = AngularWidthDegrees{.2};
    manual.origin = FwhmOrigin::gaussian;
    manual.algorithm = {"qualpf.peaks.fwhm.cpp", 3};
    manual.dataset = s.pattern.id;
    manual.source_id = s.pattern.source_id;
    manual.width_count = 1;
    manual.current = manual.manual = true;
    manual.source_pattern = manual.input_pattern = std::make_shared<const Pattern>(s.pattern);
    manual.source_original_signal = s.original_signal_provenance;
    next.preprocessing.fwhm_usage = manual;
    next.preprocessing.fwhm = FwhmConfiguration{};
    next.preprocessing.fwhm->default_fwhm = manual.width;
    check(bool(session.commit(session.snapshot(), next)));
    request = take(makePfsmRequest(session.snapshot(), settings));
    check(request.supplied_width && request.supplied_width->origin == FwhmOrigin::gaussian);
    auto changedEvidence = *session.snapshot();
    changedEvidence.original_signal_provenance->intensity_units = "different units";
    check(!take(makePfsmRequest(std::make_shared<const ProductState>(changedEvidence), settings)).supplied_width);
    request.observations->background = std::vector<double>(701, 7);
    auto problem = take(preparePfsmProblem(session.snapshot(), request, context));
    check(problem->physical_width->width.value == .2 && !problem->width.default_fwhm);
    next = *session.snapshot();
    next.preprocessing.fwhm->use_fixed = true;
    next.preprocessing.fwhm->default_fwhm = AngularWidthDegrees{.25};
    next.preprocessing.fwhm_usage.reset();
    check(bool(session.commit(session.snapshot(), next)));
    check(take(makePfsmRequest(session.snapshot(), settings)).supplied_width->width.value == .25);
    next.preprocessing.fwhm->use_fixed = false;
    next.preprocessing.fwhm->default_fwhm = manual.width;
    next.preprocessing.fwhm_usage = manual;
    next.preprocessing.fwhm_usage->current = false;
    check(bool(session.commit(session.snapshot(), next)));
    check(!take(makePfsmRequest(session.snapshot(), settings)).supplied_width);
    next.preprocessing.fwhm_usage = manual;
    next.pattern.observed[0] += 1;
    check(bool(session.commit(session.snapshot(), next)));
    check(!take(makePfsmRequest(session.snapshot(), settings)).supplied_width);
    auto noProvenance = next;
    noProvenance.original_signal_provenance.reset();
    check(!makePfsmRequest(std::make_shared<const ProductState>(noProvenance), settings));

    // Exact, independent projection example: 12% baseline, improvements 2/1 pp.
    ProductState projection;
    auto analysis = std::make_shared<PfsmAnalysis>();
    analysis->generation = 4;
    analysis->normalization_maximum = .02;
    projection.pfsm = analysis;
    ProductCard candidate;
    auto evidence = std::make_shared<PfsmCandidateEvidence>();
    evidence->generation = 4;
    evidence->rwp_net = .11;
    evidence->delta_rwp = .01;
    evidence->parameters = {{.98447}, {-.012}};
    candidate.pfsm = evidence;
    auto metrics = *pfsmCandidateMetrics(projection, candidate);
    check(metrics.relative_fom == .5 && metrics.rwp_percent == 11 && metrics.delta_rwp_pp == 1 &&
          metrics.shift_degrees == -.012 && metrics.scale == .98447);
    analysis->normalization_maximum = 0;
    evidence->delta_rwp = 0;
    check(pfsmCandidateMetrics(projection, candidate)->relative_fom == 0);
    evidence->generation = 3;
    check(!pfsmCandidateMetrics(projection, candidate));

    ProductSearchSession fitted(state());
    auto read = reader();
    auto in = input(fitted.snapshot());
    auto result = take(SearchEngine{}.run(in, read, context));
    check(bool(fitted.commit(fitted.snapshot(), result.next)));
    const auto source = fitted.snapshot();
    const auto &top = source->candidates.front();
    const double denominator = source->pfsm->normalization_maximum;
    check(denominator == top.fom && pfsmCandidateMetrics(*source, top)->relative_fom == 1);
    auto plot = take(preparePfsmPlot(source, top.id, context));
    double q = 0;
    for (std::size_t i = 0; i < plot.residual.size(); ++i) {
        check(std::abs(plot.pattern.observed[i] - plot.pattern.calculated->values[i] - plot.residual[i]) < 1e-12);
        q += plot.residual[i] * plot.residual[i];
    }
    check(std::abs(q - top.pfsm->q) < 1e-8);
    check(bool(fitted.removeCandidates(source, std::vector<std::string>{top.id})));
    check(fitted.snapshot()->pfsm->normalization_maximum == denominator &&
          pfsmCandidateMetrics(*fitted.snapshot(), fitted.snapshot()->candidates.front())->relative_fom < 1);
    check(!fitted.mergeCandidates(fitted.snapshot(), read.cards));
    check(!prepareSearchMethod(fitted.snapshot(), SearchMethod::classic_sm, false));
    auto switched = take(prepareSearchMethod(fitted.snapshot(), SearchMethod::classic_sm, true));
    check(switched.candidates.empty() && !switched.pfsm && switched.algorithms->spec.method() == SearchMethod::classic_sm);
}
class ThrowingExpander final : public profile::IProfileExpander {
  public:
    AlgorithmIdentity identity() const override { return {"test.pfsm.throw", 1}; }
    Result<profile::ProfileTemplate> prepare(const profile::ProfileExpansionInput &,
                                             const ExecutionContext &) const override {
        throw std::runtime_error("injected worker failure");
    }
};
class WaitingExpander final : public profile::IProfileExpander {
  public:
    mutable std::atomic<bool> entered{false};
    AlgorithmIdentity identity() const override { return {"test.pfsm.wait", 1}; }
    Result<profile::ProfileTemplate> prepare(const profile::ProfileExpansionInput &in,
                                             const ExecutionContext &context) const override {
        entered = true;
        while (!context.cancellation.stop_requested())
            std::this_thread::yield();
        return profile::IntermediateLorentzianExpander{}.prepare(in, context);
    }
};
static void failures() {
    ProductSearchSession session(state());
    auto source = session.snapshot();
    auto read = reader();
    auto in = input(source, 4);
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    std::stop_source progressStop;
    auto progressContext = context;
    progressContext.cancellation = progressStop.get_token();
    const auto owner = std::this_thread::get_id();
    auto interrupted = SearchEngine{}.run(in, read, progressContext, [&](std::size_t done, std::size_t) {
        check(std::this_thread::get_id() == owner);
        if (done)
            progressStop.request_stop();
    });
    check(!interrupted && interrupted.error().code == ErrorCode::cancelled && session.snapshot() == source);
    auto failedProgress = SearchEngine{}.run(in, read, context, [](std::size_t, std::size_t) {
        throw std::runtime_error("injected progress failure");
    });
    check(!failedProgress && session.snapshot() == source);
    read.calls = 0;
    auto undeclared = std::make_shared<PfsmRequest>(*request());
    undeclared->observations->unclipped_signal = false;
    in.pfsm = undeclared;
    auto bad = SearchEngine{}.run(in, read, context);
    check(!bad && read.calls == 0 && session.snapshot() == source);
    in = input(source);
    std::stop_source stop;
    stop.request_stop();
    auto canceled = context;
    canceled.cancellation = stop.get_token();
    bad = SearchEngine{}.run(in, read, canceled);
    check(!bad && bad.error().code == ErrorCode::cancelled && read.calls == 0);
    // Cancellation from another thread during numerical work drains workers
    // before return.
    auto waiting = std::make_shared<WaitingExpander>();
    AlgorithmSelections waitSelections;
    AlgorithmModules waitModules;
    waitSelections.profile_expander = waiting->identity();
    waitModules.profile_expander = waiting;
    ProductSearchSession waitSession(
        state(take(resolveAlgorithms(AlgorithmSpec{waitSelections, SearchMethod::pfsm}, waitModules))));
    std::stop_source during;
    auto running = context;
    running.cancellation = during.get_token();
    std::jthread cancel([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!waiting->entered && std::chrono::steady_clock::now() < deadline)
            std::this_thread::yield();
        during.request_stop();
    });
    bad = SearchEngine{}.run(input(waitSession.snapshot(), 4), read, running);
    check(waiting->entered && !bad && bad.error().code == ErrorCode::cancelled &&
          !waitSession.snapshot()->pfsm);
    AlgorithmSelections selections;
    AlgorithmModules modules;
    modules.profile_expander = std::make_shared<ThrowingExpander>();
    selections.profile_expander = modules.profile_expander->identity();
    ProductSearchSession injected(
        state(take(resolveAlgorithms(AlgorithmSpec{selections, SearchMethod::pfsm}, modules))));
    bad = SearchEngine{}.run(input(injected.snapshot(), 4), read, context);
    check(!bad && bad.error().field == "pfsm.worker" && !injected.snapshot()->pfsm);
    SearchCoordinator coordinator(session);
    auto task = take(coordinator.begin({"fixture", 19}));
    in = input(task.source);
    in.taskId = task.id;
    auto result = SearchEngine{}.run(in, read, task.context);
    check(bool(result));
    auto next = *source;
    next.projectMetadata["edit"] = 1;
    check(bool(session.commit(source, std::move(next))));
    check(!coordinator.complete(task.id, std::move(result)) && !session.snapshot()->pfsm);
}
static void observationRules() {
    ProductSearchSession session(state());
    const auto source = session.snapshot();
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    auto r = *request();
    r.observations->equal_weights = false;
    check(!preparePfsmProblem(source, r, context));
    r.observations->noise = preprocessing::FwhmPoissonCounts{};
    auto counts = take(preparePfsmProblem(source, r, context));
    check(counts->observations->weights[0] == 1 / source->pattern.observed[0]);
    check(!counts->width.default_fwhm &&
          !counts->width.candidates.empty()); // Explicit no-width diagnostics, no fallback.
    auto ratesState = state();
    for (auto &y : ratesState.pattern.observed)
        y /= 2;
    ProductSearchSession ratesSession(ratesState);
    r.observations->background = std::vector<double>(701, 3.5);
    r.observations->noise =
        preprocessing::FwhmPoissonRates{source->pattern.observed, std::vector<double>(701, 2)};
    auto rates = take(preparePfsmProblem(ratesSession.snapshot(), r, context));
    check(rates->observations->weights[0] == 4 / source->pattern.observed[0]);
    check(!preparePfsmProblem(source, r,
                              context)); // Counts/exposure must match the supplied rate signal.
    r = *request();
    r.observations->equal_weights = false;
    r.observations->noise = preprocessing::FwhmStandardDeviations{std::vector<double>(701, 2)};
    r.observations->valid = std::vector<std::uint8_t>(701, 1);
    r.observations->valid[0] = 0;
    auto sigma = take(preparePfsmProblem(source, r, context));
    check(sigma->observations->weights[0] == 0 && sigma->observations->weights[1] == .25);
    auto clipped = state();
    clipped.pattern.processing.background_subtracted = true;
    ProductSearchSession clippedSession(clipped);
    check(!preparePfsmProblem(clippedSession.snapshot(), r, context));
    auto scaled = state();
    scaled.pattern.original_observed = scaled.pattern.observed;
    scaled.pattern.original_kalpha2_subtracted = false;
    for (auto &y : scaled.pattern.observed)
        y /= 2;
    scaled.pattern.processing.intensity_scaled = true;
    scaled.pattern.intensity_scale = IntensityScale::relative;
    ProductSearchSession scaledSession(scaled);
    r = *request();
    r.observations->equal_weights = false;
    r.observations->noise = preprocessing::FwhmPoissonCounts{};
    check(!preparePfsmProblem(scaledSession.snapshot(), r, context));
    r.observations->signal = PfsmObservationRequest::Signal::original_observed;
    check(!preparePfsmProblem(scaledSession.snapshot(), r, context));
    r.observations->original_intensity_scaled = false;
    auto restored = take(preparePfsmProblem(scaledSession.snapshot(), r, context));
    check(restored->observations->weights[0] == 1 / source->pattern.observed[0]);
    r.observations->original_intensity_scaled = true;
    check(!preparePfsmProblem(scaledSession.snapshot(), r, context));
    r.observations->signal = PfsmObservationRequest::Signal::observed;
    r.observations->background = std::vector<double>(701, 3.5);
    r.observations->noise = preprocessing::FwhmStandardDeviations{std::vector<double>(701, 2)};
    auto propagated = take(preparePfsmProblem(scaledSession.snapshot(), r, context));
    check(propagated->observations->weights[0] == .25);
}
// The extra overlapping component is prescribed model mismatch, not a product
// golden. A zero template cannot improve a converged one-phase baseline.
static ProductState overlappingState(AlgorithmHandle algorithms = {}) {
    auto result = state(std::move(algorithms));
    for (std::size_t i = 0; i < result.pattern.observed.size(); ++i) {
        const double x = 28 + .01 * i;
        result.pattern.observed[i] = 7 + 100 * il(x, 30.01) + 70 * il(x, 30.21) + 30 * il(x, 32.964);
    }
    return result;
}
static void baselineStability() {
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    ProductSearchSession session(overlappingState());
    Reader read;
    read.cards = {card("strong", {30}), card("zero", {})};
    auto first = take(SearchEngine{}.run(input(session.snapshot()), read, context));
    check(bool(session.commit(session.snapshot(), std::move(first.next))));
    SearchParameters acceptanceParameters;
    auto accept = take(phaseAcceptanceInput(session.snapshot(), "strong", acceptanceParameters, true));
    accept->databaseVersion = 19;
    auto result = take(SearchEngine{}.run(*accept, read, context));
    const auto &analysis = *result.next.pfsm;
    check(analysis.performance.baseline_refinement_fits > 2 && analysis.performance.baseline_rebuilds == 0);
    const auto &base = analysis.baseline;
    check(base.q <= session.snapshot()->candidates.front().pfsm->q);
    check(result.next.candidates.size() == 1 && result.next.candidates[0].id == "zero" &&
          result.next.candidates[0].scale == 0);
    const double h = analysis.problem->width.default_fwhm->value;
    const auto &observations = *analysis.problem->observations;
    const auto objective = [&](double shift) {
        std::vector<double> p(observations.coordinates.size());
        double dot = 0, norm = 0;
        for (std::size_t i = 0; i < p.size(); ++i) {
            p[i] = std::pow(1 + 4 * (std::pow(2., 2. / 3) - 1) *
                                    std::pow((observations.coordinates[i] - 30 - shift) / h, 2),
                            -1.5);
            dot += p[i] * (observations.observed[i] - 7);
            norm += p[i] * p[i];
        }
        const double scale = std::max(0., dot / norm);
        double q = 0;
        for (std::size_t i = 0; i < p.size(); ++i) {
            const double residual = observations.observed[i] - 7 - scale * p[i];
            q += residual * residual;
        }
        return q;
    };
    // Independent dense scan followed by golden-section refinement, with no
    // product parameters seeding its bracket or expected minimum.
    constexpr int intervals = 4096;
    const double step = .24 / intervals;
    int best = 0;
    double q = objective(-.12);
    for (int i = 1; i <= intervals; ++i) {
        const double value = objective(-.12 + i * step);
        if (value < q) {
            q = value;
            best = i;
        }
    }
    double left = -.12 + std::max(0, best - 1) * step;
    double right = -.12 + std::min(intervals, best + 1) * step;
    const double ratio = (std::sqrt(5.) - 1) / 2;
    for (int iteration = 0; iteration < 80; ++iteration) {
        const double a = right - ratio * (right - left), b = left + ratio * (right - left);
        if (objective(a) < objective(b))
            right = b;
        else
            left = a;
    }
    const double reference = std::min(q, objective((left + right) / 2));
    check(std::abs(base.q - reference) <= 2e-8 * std::max(1., reference));
    for (int threads : {2, 4}) {
        auto options = std::make_shared<PfsmRequest>();
        options->threads = threads;
        accept->pfsm = options;
        auto parallel = take(SearchEngine{}.run(*accept, read, context));
        deterministic(result.next, parallel.next);
        check(parallel.next.pfsm->baseline.q == base.q);
    }
}
class LimitedBaselineFitter final : public profile::IProfileFitter {
  public:
    bool armed = false;
    std::stop_source *stop = nullptr;
    mutable std::size_t calls = 0;
    AlgorithmIdentity identity() const override { return {"test.pfsm.limited-baseline", 1}; }
    Result<profile::ProfileFitResult> evaluateAt(const profile::ProfileFitInput &input,
                                                 std::span<const double> shifts,
                                                 const ExecutionContext &context) const override {
        return profile::NnlsTrfProfileFitter{}.evaluateAt(input, shifts, context);
    }
    Result<profile::ProfileFitResult> fit(const profile::ProfileFitInput &input,
                                          const ExecutionContext &context) const override {
        if (!armed || input.phases.empty())
            return profile::NnlsTrfProfileFitter{}.fit(input, context);
        check(input.phases.size() == 1);
        ++calls;
        if (stop && calls == 3)
            stop->request_stop();
        // Valid NNLS solutions at intentionally slowly improving locations,
        // simulating a bounded fitter that has not stabilized its baseline.
        const std::vector<double> shift{calls == 1 ? 0 : input.phases[0].initial_shift + .0001};
        return evaluateAt(input, shift, context);
    }
};
class InvalidScanFitter final : public profile::IProfileFitter {
  public:
    bool nonfinite = false;
    AlgorithmIdentity identity() const override { return {"test.pfsm.invalid-scan", 1}; }
    Result<profile::ProfileFitResult> fit(const profile::ProfileFitInput &in,
                                         const ExecutionContext &context) const override {
        return profile::NnlsTrfProfileFitter{}.fit(in, context);
    }
    Result<profile::ProfileFitResult> evaluateAt(const profile::ProfileFitInput &in,
        std::span<const double> shifts, const ExecutionContext &context) const override {
        return profile::NnlsTrfProfileFitter{}.evaluateAt(in, shifts, context);
    }
    Result<profile::ProfileScanResult> scanAt(const profile::ProfileFitInput &,
        std::span<const double>, std::size_t, std::span<const double> values,
        const ExecutionContext &) const override {
        profile::ProfileScanResult result;
        if (nonfinite) result.q.assign(values.size(), std::numeric_limits<double>::infinity());
        return Result<profile::ProfileScanResult>{std::move(result)};
    }
};
static void invalidScan() {
    for (bool nonfinite : {false, true}) {
        auto fitter = std::make_shared<InvalidScanFitter>(); fitter->nonfinite = nonfinite;
        AlgorithmModules modules; modules.profile_fitter = fitter;
        AlgorithmSelections selections; selections.profile_fitter = fitter->identity();
        ProductSearchSession session(state(take(resolveAlgorithms(AlgorithmSpec{selections, SearchMethod::pfsm}, modules))));
        const auto before = session.snapshot(); auto read = reader();
        const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
        auto result = SearchEngine{}.run(input(before, 4), read, context);
        check(!result && result.error().reason == "invalid_algorithm_output" && session.snapshot() == before);
    }
}
static void baselineFailures() {
    for (bool cancel : {false, true}) {
        auto fitter = std::make_shared<LimitedBaselineFitter>();
        AlgorithmModules modules;
        modules.profile_fitter = fitter;
        AlgorithmSelections selections;
        selections.profile_fitter = fitter->identity();
        ProductSearchSession session(overlappingState(
            take(resolveAlgorithms(AlgorithmSpec{selections, SearchMethod::pfsm}, modules))));
        Reader read;
        read.cards = {card("strong", {30}), card("zero", {})};
        std::stop_source stop;
        ExecutionContext context{stop.get_token(), NumericalPolicy::explicit_float64, {}};
        auto first = take(SearchEngine{}.run(input(session.snapshot()), read, context));
        check(bool(session.commit(session.snapshot(), std::move(first.next))));
        const auto before = session.snapshot();
        SearchParameters acceptanceParameters;
        auto accept = take(phaseAcceptanceInput(before, "strong", acceptanceParameters, true));
        accept->databaseVersion = 19;
        fitter->armed = true;
        fitter->stop = cancel ? &stop : nullptr;
        auto failure = SearchEngine{}.run(*accept, read, context);
        check(!failure && session.snapshot() == before && before->phases.empty());
        if (cancel)
            check(failure.error().code == ErrorCode::cancelled && fitter->calls == 3);
        else
            check(failure.error().code == ErrorCode::numerical_failure &&
                  failure.error().reason == "unstable_pfsm_baseline" && fitter->calls == 33);
    }
}

// Reuse the independently analytic local-alignment counterexample, not prescreen code.
static void candidateRecallBaselineReplay(bool cache, bool smallPool = false, PfsmCandidateRecallVersion version = PfsmCandidateRecallVersion::v2) {
    using namespace profile;
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    // A shifted four-line pattern has an inferior local alignment near zero.
    // Observations come from the analytic IL, not from fitted product output.
    const std::vector<double> strengths{.82216192819178113, .68587306439876561,
        .39003297276794913, .88754720985889435};
    auto source = state();
    for (std::size_t i = 0; i < source.pattern.observed.size(); ++i) {
        double y = 7;
        for (std::size_t g = 0; g < strengths.size(); ++g)
            y += 100 * strengths[g] * il(28 + .01 * i, 30 + .4 * g + .4);
        source.pattern.observed[i] = y;
    }
    Reader read;
    auto old = card("old", {30, 30.4, 30.8, 31.2}); old.intensity = strengths;
    read.cards.push_back(old);
    auto escape = card("escape", {30.065425477735698, 30.565315905492753,
        30.884968008194118, 31.333210938004775});
    escape.intensity = {.68587467633187771, .8352170791476965, .510434384085238, .47227775044739251};
    read.cards.push_back(escape);
    for (int c = 0; c < (smallPool ? 10 : 210); ++c) {
        auto p = card("decoy-" + std::to_string(c),
            {29.9 + .001 * c, 30.43 + .0003 * c, 30.92 + .0007 * c, 31.37 + .0002 * c});
        p.intensity = {.3 + .003 * c, .8 - .002 * c, .2 + .001 * c, .4};
        read.cards.push_back(std::move(p));
    }
    auto options = std::make_shared<PfsmRequest>(*request(4, cache));
    options->lower_shift = -1; options->upper_shift = 1;
    FwhmUsage width;
    width.width = AngularWidthDegrees{.2}; width.origin = FwhmOrigin::fixed;
    width.algorithm = {"qualpf.fwhm.fixed", 1}; width.dataset = source.pattern.id;
    width.source_id = source.pattern.source_id; width.current = width.manual = true;
    width.minimum_sampling_density = 6; options->supplied_width = width;
    auto in = input(std::make_shared<const ProductState>(source)); in.pfsm = options;
    auto first = take(SearchEngine{}.run(in, read, context)).next;
    SearchParameters parameters;
    auto accept = *take(phaseAcceptanceInput(std::make_shared<const ProductState>(first), "old", parameters, true));
    accept.databaseVersion = 19;
    auto continuing = std::make_shared<PfsmRequest>(); continuing->threads = 4; accept.pfsm = continuing;
    auto selected = take(SearchEngine{}.run(accept, read, context)).next;
    auto local = std::make_shared<PfsmAnalysis>(*selected.pfsm);
    ProfileFitInput fit;
    fit.observations = local->problem->observations; fit.options = local->problem->fit;
    fit.phases = local->selected; fit.phases[0].initial_shift = 0;
    local->baseline = take(NnlsTrfProfileFitter{}.fit(fit, context));
    check(local->baseline.q > 100000 && std::abs(local->baseline.parameters.shifts[0]) < .03);
    // Inject a valid local solution as the prior baseline, with no stale scores.
    selected.phases[0].scale = local->baseline.parameters.scales[0]; selected.candidates.clear();
    local->normalization_maximum = 0; selected.pfsm = local;
    std::vector<ProductState> outputs;
    for (auto mode : {PfsmCandidateRecallMode::off, PfsmCandidateRecallMode::shadow, PfsmCandidateRecallMode::enabled}) {
        auto start = selected;
        auto analysis = std::make_shared<PfsmAnalysis>(*local);
        auto problem = std::make_shared<PfsmProblem>(*local->problem); problem->candidate_recall.mode = mode; problem->candidate_recall.version = version;
        analysis->problem = problem; analysis->candidate_recall_data.reset(); analysis->candidate_recall_reports.clear();
        analysis->normalization_over_full_pool = true;
        if (mode != PfsmCandidateRecallMode::off) {
            PfsmCandidateRecallReport report; report.algorithm_version = version;
            report.generation = analysis->generation;
            report.formal_baseline_q = analysis->baseline.q; report.pool_size = read.cards.size() - 1;
            report.full_fallback = true; report.fallback_reason = "synthetic_local_baseline";
            for (std::size_t i = 1; i < read.cards.size(); ++i) {
                report.retained_indices.push_back(i);
                report.recalled_indices.push_back(i);
            }
            analysis->candidate_recall_reports.push_back(std::move(report));
        }
        start.pfsm = analysis; check(bool(validatePfsmState(start)));
        SearchInput rescore;
        rescore.source = std::make_shared<const ProductState>(start); rescore.method = SearchMethod::pfsm;
        rescore.operation = SearchOperation::rescore; rescore.databaseVersion = 19; rescore.pfsm = continuing;
        outputs.push_back(take(SearchEngine{}.run(rescore, read, context)).next);
        const auto &result = outputs.back();
        check(bool(validatePfsmState(result)) && result.pfsm->performance.baseline_rebuilds == 1 &&
            result.pfsm->baseline.q < 1e-10 && std::abs(result.pfsm->baseline.parameters.shifts[0] - .4) < 1e-8);
        deterministic(outputs.front(), result);
        const auto &a = outputs.front().pfsm->baseline, &b = result.pfsm->baseline;
        check(a.q == b.q && a.parameters.scales == b.parameters.scales && a.parameters.shifts == b.parameters.shifts &&
            a.calculated == b.calculated && a.residual == b.residual && a.termination == b.termination);
        for (std::size_t i = 0; i < result.candidates.size(); ++i) {
            const auto &x = *outputs.front().candidates[i].pfsm, &y = *result.candidates[i].pfsm;
            check(x.termination == y.termination && x.diagnostics.residual_evaluations == y.diagnostics.residual_evaluations &&
                x.diagnostics.nnls_iterations == y.diagnostics.nnls_iterations);
        }
    }
    const auto &shadow = outputs[1].pfsm->candidate_recall_reports, &enabled = outputs[2].pfsm->candidate_recall_reports;
    check(shadow.size() == 2 && shadow[0].discarded && !shadow[1].discarded);
    check(enabled.front().discarded && !enabled.back().discarded && enabled.back().full_fallback);
    check(enabled.back().fallback_reason == "baseline_rebuild_full_replay");
    check(outputs[2].pfsm->performance.formal_reused == 0);
    if (smallPool) {
        check(enabled.size() == 2 && enabled.front().fallback_reason == "small_pool");
        check(outputs[2].pfsm->performance.evaluated == outputs[0].pfsm->performance.evaluated);
    }

}
static void candidateRecallWorkflow(bool cache, PfsmCandidateRecallVersion version = PfsmCandidateRecallVersion::v2) {
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    auto source = std::make_shared<const ProductState>(state());
    auto library = reader();
    for (int j = 0; j < 40; ++j) library.cards.push_back(card("extra-" + std::to_string(j), {28.1 + .165 * j}));
    auto in = input(source, 2);
    in.pfsm = request(2, cache);
    const auto full = take(SearchEngine{}.run(in, library, context));
    check(!full.next.pfsm->candidate_recall_data && full.next.pfsm->candidate_recall_reports.empty());
    const auto subset = [&](const ProductState &expected, const ProductState &actual) {
        check(actual.pfsm->baseline.q == expected.pfsm->baseline.q);
        check(actual.pfsm->baseline.calculated == expected.pfsm->baseline.calculated);
        for (std::size_t i = 0; i < 5; ++i) check(actual.candidates[i].id == expected.candidates[i].id);
        std::size_t position = 0;
        for (const auto &c : actual.candidates) {
            while (position < expected.candidates.size() && expected.candidates[position].id != c.id) ++position;
            check(position < expected.candidates.size());
            const auto &e = expected.candidates[position++];
            check(e.pfsm->q == c.pfsm->q && e.pfsm->rwp_net == c.pfsm->rwp_net && e.fom == c.fom &&
                  e.pfsm->parameters.shifts == c.pfsm->parameters.shifts &&
                  e.pfsm->parameters.scales == c.pfsm->parameters.scales &&
                  e.pfsm->diagnostics.residual_evaluations == c.pfsm->diagnostics.residual_evaluations);
        }
        check(bool(validatePfsmState(actual)));
    };
    for (auto mode : {PfsmCandidateRecallMode::shadow, PfsmCandidateRecallMode::enabled}) {
        auto r = std::make_shared<PfsmRequest>(*request(2, cache)); r->candidate_recall.mode = mode; r->candidate_recall.version = version; in.pfsm = r;
        auto candidate_recall = take(SearchEngine{}.run(in, library, context));
        check(candidate_recall.next.pfsm->candidate_recall_data && candidate_recall.next.pfsm->candidate_recall_reports.size() == 1);
        const auto &report = candidate_recall.next.pfsm->candidate_recall_reports.back();
        {
            check(report.algorithm_version == PfsmCandidateRecallVersion::v2 && report.selection);
            check(report.selection->review.size() <= 80);

            for (const auto &v : report.refinement_candidates)
                check(bool(v.formal_gain) == std::binary_search(report.selection->proposed.begin(),
                    report.selection->proposed.end(), v.recall.original_index));
            if (report.fallback_reason == "formal_underestimation" && mode == PfsmCandidateRecallMode::enabled)
                check(candidate_recall.next.pfsm->performance.formal_reused == report.selection->proposed.size() &&
                      candidate_recall.next.pfsm->performance.evaluated == library.cards.size());
        }
        if (mode == PfsmCandidateRecallMode::shadow) {
            deterministic(full.next, candidate_recall.next);
            check(candidate_recall.next.pfsm->candidate_recall_reports.back().final_top5_retained == true);
        } else {
            check(candidate_recall.next.candidates.size() <= full.next.candidates.size());
            subset(full.next, candidate_recall.next);
        }
        auto frozen = std::make_shared<const ProductState>(candidate_recall.next);
        auto rescore = input(frozen, 2); rescore.operation = SearchOperation::rescore;
        auto override = std::make_shared<PfsmRequest>(); override->threads = 2; override->full_round = true;
        rescore.pfsm = override;
        auto replay = take(SearchEngine{}.run(rescore, library, context));
        deterministic(full.next, replay.next);
        check(replay.next.pfsm->problem->candidate_recall.mode == mode && replay.next.pfsm->problem->candidate_recall.version == version);
        check(replay.next.pfsm->candidate_recall_reports.back().fallback_reason == "requested_full_round");
        check(replay.next.pfsm->candidate_recall_data == frozen->pfsm->candidate_recall_data);
        auto corrupted = candidate_recall.next;
        auto changed = std::make_shared<PfsmAnalysis>(*corrupted.pfsm);
        changed->candidate_recall_reports.back().candidates.pop_back();corrupted.pfsm = changed;
        check(!validatePfsmState(corrupted));
        std::stop_source stop;stop.request_stop();auto cancelled=context;cancelled.cancellation=stop.get_token();
        check(!SearchEngine{}.run(in, library, cancelled));
        check(frozen->pfsm->generation == candidate_recall.next.pfsm->generation);
        // Every continuation uses the complete universe, including prior exclusions.
        SearchParameters acceptanceParameters;
        auto accept = take(phaseAcceptanceInput(frozen, "strong", acceptanceParameters, true));check(bool(accept));
        accept->databaseVersion = 19;
        auto accepted = take(SearchEngine{}.run(*accept, library, context));

        check(accepted.next.pfsm->universe == frozen->pfsm->universe);
        check(accepted.next.pfsm->candidate_recall_reports.back().pool_size == library.cards.size()-1);
        check(accepted.next.pfsm->candidate_recall_reports.back().generation == frozen->pfsm->generation+1);
        check(accepted.next.pfsm->candidate_recall_data == frozen->pfsm->candidate_recall_data);
        check(bool(validatePfsmState(accepted.next)));
        auto remove = take(phaseRemovalInput(std::make_shared<const ProductState>(accepted.next), "strong"));
        remove.databaseVersion=19;
        auto removed=take(SearchEngine{}.run(remove,library,context));
        check(removed.next.pfsm->candidate_recall_reports.back().pool_size==library.cards.size());
        check(bool(validatePfsmState(removed.next)));
    }
}
static void configurableRecallWorkflow() {
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    auto source = std::make_shared<const ProductState>(state());
    auto library = reader();
    for (int j=0;j<30;++j) library.cards.push_back(card("configured-"+std::to_string(j), {28.13+.19*j}));
    auto configured = std::make_shared<PfsmRequest>(*request(2));
    configured->lower_shift=configured->upper_shift=0;
    configured->candidate_recall.mode=PfsmCandidateRecallMode::enabled;
    configured->candidate_recall.limits={12,3,1,8,8};
    auto in=input(source);in.pfsm=configured;
    auto first=take(SearchEngine{}.run(in,library,context)).next;
    const auto &report=first.pfsm->candidate_recall_reports.front();
    check(bool(validatePfsmState(first)) && report.limits==configured->candidate_recall.limits);
    check(report.selection && report.selection->limits==report.limits && report.recalled_indices.size()<library.cards.size());
    auto frozen=std::make_shared<const ProductState>(first);
    PfsmSettings changed;changed.threads=1;changed.candidate_recall_enabled=false;
    changed.candidate_recall_limits={25,7,2,10,10};
    auto continued=std::make_shared<PfsmRequest>(take(makePfsmRequest(frozen,changed)));
    auto rescore=input(frozen);rescore.operation=SearchOperation::rescore;rescore.pfsm=continued;
    const auto next=take(SearchEngine{}.run(rescore,library,context)).next;
    check(next.pfsm->problem==first.pfsm->problem && next.pfsm->problem->candidate_recall==configured->candidate_recall);
    check(next.pfsm->performance.threads==1);
    auto corrupted=first;auto badAnalysis=std::make_shared<PfsmAnalysis>(*first.pfsm);
    ++badAnalysis->candidate_recall_reports.front().limits.review_limit;corrupted.pfsm=badAnalysis;
    check(!validatePfsmState(corrupted));
    auto badRequest=std::make_shared<PfsmRequest>(*configured);badRequest->candidate_recall.limits.local_shortlist=13;
    in.pfsm=badRequest;
    const auto rejected=SearchEngine{}.run(in,library,context);
    check(!rejected && rejected.error().field=="pfsm.candidate_recall.localShortlist");
    // A configured local shortlist, rather than the old fixed 20, controls small-pool fallback.
    configured->candidate_recall.limits={100,50,1,50,50};in.pfsm=configured;
    const auto small=take(SearchEngine{}.run(in,library,context));
    check(small.next.pfsm->candidate_recall_reports.back().fallback_reason=="small_pool");
    auto original=state();original.pattern.original_observed=original.pattern.observed;
    original.pattern.original_kalpha2_subtracted=false;
    original.original_signal_provenance=OriginalSignalProvenance{original.pattern.source_id,"analytic",true,false};
    auto fresh=take(makePfsmRequest(std::make_shared<const ProductState>(original),changed));
    check(fresh.candidate_recall.mode==PfsmCandidateRecallMode::off && fresh.candidate_recall.limits==changed.candidate_recall_limits);
}
static void v2JointConflict() {
    const ExecutionContext context{{},NumericalPolicy::explicit_float64,{}};
    auto source=state();
    for(std::size_t i=0;i<source.pattern.observed.size();++i) {
        const double x=28+.01*i;
        source.pattern.observed[i]=7+100*il(x,30.12)+100*il(x,30.27)+200*il(x,29.9);
    }
    Reader library;library.cards={card("old",{30,30.15}),card("new",{30})};
    for(int i=0;i<40;++i)library.cards.push_back(card("supplement-decoy-"+std::to_string(i),{32+.04*i}));
    auto requestOld=std::make_shared<PfsmRequest>(*request(4,true));
    requestOld->lower_shift=-.3;requestOld->upper_shift=.3;
    FwhmUsage width;width.width=AngularWidthDegrees{.2};width.origin=FwhmOrigin::fixed;
    width.algorithm={"qualpf.fwhm.fixed",1};width.dataset=source.pattern.id;
    width.source_id=source.pattern.source_id;width.current=width.manual=true;width.minimum_sampling_density=6;
    requestOld->supplied_width=width;
    auto in=input(std::make_shared<const ProductState>(source));in.pfsm=requestOld;
    auto initial=take(SearchEngine{}.run(in,library,context)).next;
    SearchParameters parameters;
    auto accept=*take(phaseAcceptanceInput(std::make_shared<const ProductState>(initial),"old",parameters,true));
    accept.databaseVersion=19;
    auto previous=take(SearchEngine{}.run(accept,library,context)).next;
    std::optional<ProductState> shadow;
    for(auto mode:{PfsmCandidateRecallMode::shadow,PfsmCandidateRecallMode::enabled}) {
        auto analysis=std::make_shared<PfsmAnalysis>(*previous.pfsm);
        auto problem=std::make_shared<PfsmProblem>(*analysis->problem);
        problem->candidate_recall={mode,PfsmCandidateRecallVersion::v2};analysis->problem=problem;
        analysis->candidate_recall_reports.clear();analysis->candidate_recall_data.reset();
        PfsmCandidateRecallReport report;report.algorithm_version=PfsmCandidateRecallVersion::v2;
        report.generation=analysis->generation;report.formal_baseline_q=analysis->baseline.q;
        report.pool_size=library.cards.size()-1;report.full_fallback=true;report.fallback_reason="analytic_transition";
        for(std::size_t i=1;i<library.cards.size();++i)report.recalled_indices.push_back(i);
        report.retained_indices=report.recalled_indices;analysis->candidate_recall_reports.push_back(report);
        auto start=previous;start.pfsm=analysis;check(bool(validatePfsmState(start)));
        auto rescore=input(std::make_shared<const ProductState>(start),4);rescore.operation=SearchOperation::rescore;
        auto continued=std::make_shared<PfsmRequest>();continued->threads=4;rescore.pfsm=continued;
        auto result=take(SearchEngine{}.run(rescore,library,context)).next;
        const auto &r=result.pfsm->candidate_recall_reports.back();

        check(bool(validatePfsmState(result)));
        if(mode==PfsmCandidateRecallMode::shadow)shadow=std::move(result);
        else {
            check(shadow->pfsm->baseline.residual==result.pfsm->baseline.residual);
            for(std::size_t i=0;i<5;++i)check(shadow->candidates[i].id==result.candidates[i].id);
            std::size_t position=0;
            for(const auto &candidate:result.candidates) {
                while(position<shadow->candidates.size() && shadow->candidates[position].id!=candidate.id)++position;
                check(position<shadow->candidates.size());
                const auto &expected=*shadow->candidates[position++].pfsm;
                check(expected.q==candidate.pfsm->q && expected.parameters.scales==candidate.pfsm->parameters.scales &&
                    expected.parameters.shifts==candidate.pfsm->parameters.shifts);
            }
            if(r.full_fallback)deterministic(*shadow,result);
            if(r.fallback_reason=="formal_underestimation")check(result.pfsm->performance.evaluated==library.cards.size()-1 &&
                result.pfsm->performance.formal_reused==r.selection->proposed.size());
        }
    }
}
static void v2ThreadSelection() {
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    auto source=std::make_shared<const ProductState>(state());auto library=reader();
    for(int j=0;j<24;++j)library.cards.push_back(card("thread-extra-"+std::to_string(j),{28.1+.25*j}));
    std::optional<ProductState> first;
    for(int threads:{1,4,12,20}) {
        auto r=std::make_shared<PfsmRequest>(*request(threads,true));
        r->candidate_recall.mode=PfsmCandidateRecallMode::enabled;r->candidate_recall.version=PfsmCandidateRecallVersion::v2;
        auto in=input(source);in.pfsm=r;
        auto output=take(SearchEngine{}.run(in,library,context));
        check(bool(validatePfsmState(output.next)));
        if(!first){first=std::move(output.next);continue;}
        deterministic(*first,output.next);
        const auto &a=first->pfsm->candidate_recall_reports.back(),&b=output.next.pfsm->candidate_recall_reports.back();
        check(a.retained_indices==b.retained_indices && a.recalled_indices==b.recalled_indices &&
            a.selection->initial_top==b.selection->initial_top &&
            a.selection->local_top==b.selection->local_top && a.selection->review==b.selection->review);
        for(std::size_t i=0;i<a.refinement_candidates.size();++i)
            check(a.refinement_candidates[i].best.gain==b.refinement_candidates[i].best.gain &&
                a.refinement_candidates[i].nnls_calls==b.refinement_candidates[i].nnls_calls);
    }
}
int main() try {
    candidateRecallWorkflow(false, PfsmCandidateRecallVersion::v2);
    candidateRecallWorkflow(true, PfsmCandidateRecallVersion::v2);
    candidateRecallBaselineReplay(false, false, PfsmCandidateRecallVersion::v2);
    candidateRecallBaselineReplay(true, false, PfsmCandidateRecallVersion::v2);
    v2ThreadSelection();
    v2JointConflict();
    configurableRecallWorkflow();
    candidateRecallBaselineReplay(false, true);
    candidateRecallBaselineReplay(true, true);
    workflow();
    workflow(true);
    failures();
    observationRules();
    presentationAndWidths();
    baselineStability();
    baselineFailures();
    invalidScan();
    std::cout << "PFSM Application contracts passed\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
