#include "classic_fixture.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/search/rir.h"
#include "qualpf/application/preprocessing_workflows.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/search_preparation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v, std::source_location at = std::source_location::current()) {
    if (!v)
        throw std::runtime_error("Algorithm workflow at line " + std::to_string(at.line()));
}
static AlgorithmHandle assemble(AlgorithmSelections selections, AlgorithmModules overrides) {
    auto result = resolveAlgorithms(AlgorithmSpec{std::move(selections)}, std::move(overrides));
    check(bool(result));
    return std::move(result).value();
}
static ProductState state(AlgorithmHandle algorithms = defaultAlgorithms(), bool dense = false) {
    testing::ClassicFixture f;
    ProductState s;
    s.algorithms = std::move(algorithms);
    s.pattern = f.pattern;
    s.pattern.radiation = {RadiationKind::x_ray, {{WavelengthAngstrom{1.54}, 1}}};
    s.pattern.processing.background_subtracted = false;
    s.pattern.processing.intensity_scaled = false;
    s.pattern.intensity_scale = IntensityScale::counts;
    if (dense) {
        s.pattern.two_theta.clear();
        s.pattern.observed.clear();
        for (int i = 0; i <= 1500; ++i) {
            const double x = 10 + .02 * i;
            s.pattern.two_theta.push_back(TwoThetaDegrees{x});
            double y = 5;
            for (auto [center, height] : {std::pair{15., 100.}, {23., 200.}, {32., 150.}})
                y += height * std::exp(-.5 * std::pow((x - center) / .12, 2));
            s.pattern.observed.push_back(y);
        }
        s.pattern.calculation_range = AngularRange{s.pattern.two_theta.front(), s.pattern.two_theta.back()};
    } else {
        s.peaks.valid = true;
        s.peaks.wave = 1.54;
        s.peaks.integratedIntensity.emplace();
        for (const auto &p : f.peaks.peaks) {
            s.peaks.tth.push_back(p.two_theta.value);
            s.peaks.intensity.push_back(p.intensity);
            s.peaks.intensityOrig.push_back(*p.original_intensity);
            s.peaks.d.push_back(p.d_spacing->value);
            s.peaks.deltaD.push_back(p.delta_d->value);
            s.peaks.fwhm.push_back(p.fwhm->value);
            s.peaks.integratedIntensity->push_back(*p.integrated_intensity);
        }
    }
    s.pattern.original_observed = s.pattern.observed;
    s.pattern.background = SampledSignal{std::vector<double>(s.pattern.observed.size(), 0), SampleWindow{0}};
    return s;
}
static ProductCard card(std::string id = "A") {
    testing::ClassicFixture f;
    ProductCard c;
    c.id = std::move(id);
    c.quality = "A";
    c.rir = "1";
    for (const auto &r : f.card.reflections) {
        c.tth.push_back(r.two_theta->value);
        c.intensity.push_back(r.intensity);
    }
    return c;
}
struct Reader : CandidateReader {
    int reads = 0, tops = 0;
    CandidateQuery received;
    std::vector<ProductCard> cards{card("A"), card("B")};
    Result<std::vector<StoredCandidateIndex>> storedTop(const ExecutionContext &, SearchProgress) override {
        ++tops;
        return Result<std::vector<StoredCandidateIndex>>{
            std::vector<StoredCandidateIndex>{{"A", {DSpacingAngstrom{99}}}, {"B", {testing::ClassicFixture{}.peaks.peaks.front().d_spacing.value()}}}};
    }
    Result<std::vector<ProductCard>> read(const CandidateQuery &q, const ExecutionContext &,
                                          SearchProgress) override {
        ++reads;
        received = q;
        auto result = cards;
        if (q.preselectedIds)
            std::erase_if(result, [&](const auto &c) {
                return std::find(q.preselectedIds->begin(), q.preselectedIds->end(), c.id) ==
                       q.preselectedIds->end();
            });
        return Result<std::vector<ProductCard>>{std::move(result)};
    }
};
struct Background : preprocessing::IBackgroundEstimator {
    mutable int calls = 0;
    bool expect_points = true;
    std::stop_source *cancel = nullptr;
    AlgorithmIdentity identity() const override { return {"test.background", 1}; }
    Result<preprocessing::BackgroundResult>
    estimate(const Pattern &p, const preprocessing::BackgroundOptions &o, WavelengthAngstrom w,
             std::optional<std::span<const preprocessing::BackgroundPoint>> points,
             const ExecutionContext &) const override {
        ++calls;
        check(o.method == preprocessing::BackgroundMethod::none && w.value == 1.54);
        check(expect_points ? points && points->size() == 2 : !points);
        if (cancel)
            cancel->request_stop();
        preprocessing::BackgroundResult r;
        r.values.assign(p.observed.size(), 2);
        if (points)
            r.points.assign(points->begin(), points->end());
        return Result<preprocessing::BackgroundResult>{r};
    }
};
struct Initial : preprocessing::IInitialPeakFinder {
    mutable int calls = 0;
    int malformed = 0;
    AlgorithmIdentity identity() const override { return {"test.initial", 1}; }
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &spectrum, WavelengthAngstrom,
        const preprocessing::InitialPeakOptions &, const ExecutionContext &) const override {
        const auto &pattern = spectrum.pattern();
        ++calls;
        preprocessing::InitialPeakResult r;
        r.dataset = pattern.id;
        for (double center : {15., 23., 32.}) {
            auto at = std::min_element(pattern.two_theta.begin(), pattern.two_theta.end(), [=](auto a, auto b) {
                return std::abs(a.value - center) < std::abs(b.value - center);
            });
            r.candidates.push_back({std::size_t(at - pattern.two_theta.begin())});
        }
        switch (malformed) {
        case 1: r.dataset = DatasetId{42}; break;
        case 2: r.candidates.back().sample_index = pattern.two_theta.size(); break;
        case 3: r.candidates[1] = r.candidates[0]; break;
        case 4: std::reverse(r.candidates.begin(), r.candidates.end()); break;
        case 5: r.applied.search_range = std::numeric_limits<float>::quiet_NaN(); break;
        case 6: r.applied.half_window = 0; break;
        case 7: r.applied.polynomial_degree = 2; break;
        case 8: r.applied.detection_range = AngularRange{TwoThetaDegrees{10}, TwoThetaDegrees{40}}; break;
        }
        return Result<preprocessing::InitialPeakResult>{r};
    }
};
struct Fwhm : preprocessing::IFwhmEstimator {
    mutable int calls = 0;
    mutable std::size_t received = 0;
    mutable std::vector<double> received_signal;
    double width = .2f;
    int malformed = 0;
    std::stop_source *cancel = nullptr;
    AlgorithmIdentity identity() const override { return {"test.fwhm", 1}; }
    Result<preprocessing::FwhmEstimationResult> estimate(const AnalysisSpectrum &spectrum, const preprocessing::InitialPeakResult &r, const ExecutionContext &) const override {
        const auto &pattern = spectrum.pattern();
        ++calls;
        received = r.candidates.size();
        received_signal = pattern.observed;
        check(pattern.id == r.dataset);
        preprocessing::FwhmEstimationResult result{
            {preprocessing::FwhmEstimate::State::measured, AngularWidthDegrees{width}, 1}, {}};
        switch (malformed) {
        case 1: result.estimate.peak_count = received + 1; break;
        case 2: result.estimate.width.reset(); break;
        case 3: result.estimate.state = preprocessing::FwhmEstimate::State::no_initial_peaks; break;
        case 4: result.estimate.width = AngularWidthDegrees{-1}; break;
        case 5: result.estimate.width = AngularWidthDegrees{std::numeric_limits<double>::infinity()}; break;
        }
        if (malformed >= 6) {
            result.rejected_candidates.push_back({preprocessing::PeakDetectionStage::initial, 0, 0,
                TwoThetaDegrees{double(float(pattern.two_theta.front().value))}, "coincident_baseline_endpoints"});
            auto &diagnostic = result.rejected_candidates.front();
            if (malformed == 6) diagnostic.stage = preprocessing::PeakDetectionStage::formal;
            if (malformed == 7) diagnostic.sample_index = pattern.two_theta.size();
            if (malformed == 8) diagnostic.two_theta.value += 1;
            if (malformed == 9) diagnostic.reason = "unknown_rejection";
        }
        if (cancel) cancel->request_stop();
        return Result<preprocessing::FwhmEstimationResult>{result};
    }
};
struct Extractor : preprocessing::IExperimentalPeakExtractor {
    mutable int calls = 0;
    mutable float range = 0;
    AlgorithmIdentity identity() const override { return {"test.extractor", 1}; }
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &spectrum, const preprocessing::PeakCalibration &c,
                                                    WavelengthAngstrom w, AngularWidthDegrees,
                                                    const ExecutionContext &) const override {
        const auto &p = spectrum.pattern();
        ++calls;
        range = c.applied.search_range;
        testing::ClassicFixture f;
        auto peaks = f.peaks;
        peaks.dataset = p.id;
        peaks.effective_wavelength = w;
        peaks.source_precision = SourcePrecision::float32_abi;
        peaks.intensity_scale = p.intensity_scale;
        peaks.peaks[0].intensity = 91;
        peaks.peaks[0].original_intensity = 91;
        return Result<preprocessing::PeakSearchResult>{
            preprocessing::PeakSearchResult{peaks, peaks, c.applied, .425}};
    }
};
struct Matcher : search::IClassicMatcher {
    mutable int calls = 0;
    bool incompatible = false;
    AlgorithmIdentity identity() const override { return {"test.matcher", 1}; }
    AlgorithmIdentity produces() const override {
        return incompatible ? AlgorithmIdentity{"test.other-evidence", 1} : search::classic_match_contract();
    }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &i, const ExecutionContext &ctx) const override {
        ++calls;
        auto r = search::ClassicMatcher{}.match(i, ctx);
        if (r)
            r.value().position_difference_sum = .375f;
        return r;
    }
};
static void independentPreprocessing() {
    testing::ClassicFixture f;
    auto background = std::make_shared<Background>();
    background->expect_points = false;
    auto initial = std::make_shared<Initial>();
    auto fwhm = std::make_shared<Fwhm>();
    auto extractor = std::make_shared<Extractor>();
    AlgorithmSelections selected;
    selected.background = background->identity(); selected.initial_peaks = initial->identity();
    selected.fwhm = fwhm->identity(); selected.experimental_peaks = extractor->identity();
    AlgorithmModules modules;
    modules.background = background; modules.initial_peaks = initial;
    modules.fwhm = fwhm; modules.experimental_peaks = extractor;
    ProductSearchSession session(state(assemble(selected, modules), true));
    const auto source = session.snapshot();
    CommonPreprocessingRequest request;
    request.wavelength = WavelengthAngstrom{1.54};
    request.background = preprocessing::BackgroundOptions{};
    request.background->method = preprocessing::BackgroundMethod::none;
    request.peaks = preprocessing::PeakOptions{}; request.peaks->search_range = -1;
    request.purpose = AnalysisPurpose::peak_measurement;
    auto common = evaluateCommonPreprocessing(source, request, f.context);
    check(common && common.value().source == source && background->calls == 1 && initial->calls == 1 &&
          fwhm->calls == 1 && extractor->calls == 0 && session.snapshot() == source);
    check(common.value().working_pattern.observed == source->pattern.observed &&
          common.value().working_pattern.background->values.front() == 2 &&
          common.value().spectrum->pattern().observed.front() ==
              source->pattern.observed.front());
    check(fwhm->received_signal == common.value().spectrum->pattern().observed &&
          fwhm->received_signal == common.value().working_pattern.observed);
    auto parameters = preprocessing::prepare_classic_peak_parameters(*request.peaks, common.value().fwhm->estimate,
        request.wavelength, f.context);
    check(bool(parameters));
    auto formal = evaluateFormalPeakExtraction(source,
        {common.value().spectrum, parameters.value(), request.wavelength, AngularWidthDegrees{.25}}, f.context);
    check(formal && formal.value().peaks.peaks.front().intensity == 91 && extractor->range == .2f * .9f &&
          initial->calls == 1 && fwhm->calls == 1 && extractor->calls == 1 && session.snapshot() == source);
    request.background.reset(); request.peaks->search_range = .2f;
    common = evaluateCommonPreprocessing(source, request, f.context);
    check(common && !common.value().initial_peaks && !common.value().fwhm && initial->calls == 1 && fwhm->calls == 1);
    request.peaks->search_range = -1; fwhm->width = std::numeric_limits<double>::quiet_NaN();
    auto invalid = evaluateCommonPreprocessing(source, request, f.context);
    check(!invalid && invalid.error().reason == "invalid_algorithm_output" && session.snapshot() == source);
    auto unrelated = source->pattern; unrelated.id = DatasetId{42};
    check(!evaluateCommonPreprocessing(source, unrelated, request, f.context));
    std::stop_source stop; stop.request_stop(); auto cancelled = f.context; cancelled.cancellation = stop.get_token();
    const auto initialCalls = initial->calls;
    check(!evaluateCommonPreprocessing(source, request, cancelled) && initial->calls == initialCalls);

    fwhm->width = .2f;
    for (int kind = 1; kind <= 8; ++kind) {
        initial->malformed = kind;
        const auto widthCalls = fwhm->calls;
        invalid = evaluateCommonPreprocessing(source, request, f.context);
        check(!invalid && invalid.error().reason == "invalid_algorithm_output" &&
              fwhm->calls == widthCalls && session.snapshot() == source);
    }
    initial->malformed = 0;
    for (int kind = 1; kind <= 9; ++kind) {
        fwhm->malformed = kind;
        invalid = evaluateCommonPreprocessing(source, request, f.context);
        check(!invalid && invalid.error().reason == "invalid_algorithm_output" && session.snapshot() == source);
    }
    fwhm->malformed = 0;
    std::stop_source duringWidth;
    fwhm->cancel = &duringWidth;
    auto widthContext = f.context; widthContext.cancellation = duringWidth.get_token();
    invalid = evaluateCommonPreprocessing(source, request, widthContext);
    check(!invalid && invalid.error().code == ErrorCode::cancelled && session.snapshot() == source);
    fwhm->cancel = nullptr;

    // A failed FWHM preview must invalidate a previously usable detection cache.
    auto prepared = automaticPreprocessingRequest(source, PreprocessingAction::automatic_peaks).value();
    PreprocessingEvaluation preview(source);
    check(bool(preview.preview(prepared)));
    fwhm->malformed = 1;
    check(!preview.preview(prepared) && !preview.preview());
    prepared.peak_operation = PeakOperation::reselect; prepared.selection = PeakSelection{};
    check(!preview.preview(prepared) && !preview.preview() && session.snapshot() == source);
    fwhm->malformed = 0;

    fwhm->width = 0;
    common = evaluateCommonPreprocessing(source, request, f.context);
    check(common && common.value().fwhm->estimate.width->value == 0);
    parameters = preprocessing::prepare_classic_peak_parameters(*request.peaks, common.value().fwhm->estimate,
        request.wavelength, f.context);
    check(parameters && parameters.value().formal_search_range.value == 0 &&
          parameters.value().source == preprocessing::PeakCalibration::Source::strongest_initial_widths);
    fwhm->width = .2f;

    ProductSearchSession defaults(state(defaultAlgorithms(), true));
    const auto defaultSource = defaults.snapshot();
    preprocessing::PeakOptions retry;
    retry.search_range = .15f; retry.threshold_percent = 100; retry.minimum_peaks = 100;
    auto manual = preprocessing::prepare_classic_peak_parameters(retry, {}, request.wavelength, f.context).value();
    auto retried = evaluateFormalPeakExtraction(defaultSource,
        {std::make_shared<const AnalysisSpectrum>(AnalysisSpectrum::create(defaultSource->pattern).value()), manual, request.wavelength, AngularWidthDegrees{.25}}, f.context);
    check(retried && retried.value().applied.threshold_percent == 1.5f * .4f &&
          !retried.value().peaks.peaks.empty() && defaults.snapshot() == defaultSource);
}
static void preprocessingModules() {
    testing::ClassicFixture f;
    auto bg = std::make_shared<Background>();
    AlgorithmSelections s;
    AlgorithmModules m;
    s.background = bg->identity();
    m.background = bg;
    ProductSearchSession session(state(assemble(s, m), true));
    const auto source = session.snapshot();
    PreprocessingRequest request;
    request.wavelength = WavelengthAngstrom{1.54};
    request.background = preprocessing::BackgroundOptions{};
    request.background->method = preprocessing::BackgroundMethod::none;
    request.background_points = std::vector<preprocessing::BackgroundPoint>{{10, 2}, {40, 2}};
    ProductPreprocessingEdit edit(session);
    check(bool(edit.preview(request)) && bg->calls == 1 && session.snapshot() == source);
    check(edit.preview()->pattern.background->values[10] == 2 &&
          edit.preview()->pattern.observed == source->pattern.observed);
    check(bool(edit.commit()) && session.snapshot()->pattern.background->values[10] == 2);
    auto measurement = *session.snapshot();
    measurement.pattern.background.reset();
    bg->expect_points = false;
    auto measurementContext = preparePeakMeasurementContext(measurement, WavelengthAngstrom{1.54}, f.context);
    check(measurementContext && measurementContext.value().background->values[10] == 2 && bg->calls == 2);
    bg->expect_points = true;
    std::stop_source stop;
    bg->cancel = &stop;
    PreprocessingEvaluation cancelled(session.snapshot());
    check(!cancelled.preview(request, stop.get_token()) && !cancelled.preview());

    auto initial = std::make_shared<Initial>();
    s = {};
    m = {};
    s.initial_peaks = initial->identity();
    m.initial_peaks = initial;
    auto analyticState = state(assemble(s, m), true);
    for (auto &y : analyticState.pattern.observed) y -= 5;
    analyticState.pattern.original_observed = analyticState.pattern.observed;
    ProductSearchSession onlyInitial(std::move(analyticState));
    auto automatic =
        automaticPreprocessingRequest(onlyInitial.snapshot(), PreprocessingAction::automatic_peaks).value();
    PreprocessingEvaluation first(onlyInitial.snapshot());
    check(bool(first.preview(automatic)) && initial->calls == 1);
    const double analyticWidth = 2. * std::sqrt(2. * std::log(2.)) * .12;
    const auto &evidence = *first.preview()->preprocessing.peak_calibration;
    check(evidence.width_count == 3 && std::abs(evidence.estimated_fwhm->value - analyticWidth) <=
          1e-5 + 1e-5 * analyticWidth);
    check(first.preview()->preprocessing.peaks->search_range == float(evidence.estimated_fwhm->value) * .9f);

    auto calibration = std::make_shared<Fwhm>();
    s = {};
    m = {};
    s.fwhm = calibration->identity();
    m.fwhm = calibration;
    ProductSearchSession onlyCalibration(state(assemble(s, m), true));
    automatic =
        automaticPreprocessingRequest(onlyCalibration.snapshot(), PreprocessingAction::automatic_peaks)
            .value();
    PreprocessingEvaluation second(onlyCalibration.snapshot());
    check(bool(second.preview(automatic)) && calibration->calls == 1 && calibration->received > 0);
    check(second.preview()->preprocessing.peaks->search_range == .2f * .9f && second.preview()->peaks.valid);

    auto extractor = std::make_shared<Extractor>();
    s = {};
    m = {};
    s.experimental_peaks = extractor->identity();
    m.experimental_peaks = extractor;
    ProductSearchSession onlyExtractor(state(assemble(s, m), true));
    automatic =
        automaticPreprocessingRequest(onlyExtractor.snapshot(), PreprocessingAction::automatic_peaks).value();
    PreprocessingEvaluation third(onlyExtractor.snapshot());
    check(bool(third.preview(automatic)) && extractor->calls == 1);
    check(third.preview()->peaks.intensity.front() == 91 && third.preview()->automaticDelta == .425);

    initial = std::make_shared<Initial>();
    calibration = std::make_shared<Fwhm>();
    extractor = std::make_shared<Extractor>();
    s = {};
    m = {};
    s.initial_peaks = initial->identity();
    m.initial_peaks = initial;
    s.fwhm = calibration->identity();
    m.fwhm = calibration;
    s.experimental_peaks = extractor->identity();
    m.experimental_peaks = extractor;
    ProductSearchSession stages(state(assemble(s, m), true));
    automatic =
        automaticPreprocessingRequest(stages.snapshot(), PreprocessingAction::automatic_peaks).value();
    PreprocessingEvaluation evaluation(stages.snapshot());
    check(bool(evaluation.preview(automatic)) && initial->calls == 1 && calibration->calls == 1 &&
          extractor->calls == 1);
    check(calibration->received == 3 && extractor->range == .2f * .9f);
    automatic.peaks->search_range = .2f;
    check(bool(evaluation.preview(automatic)) && initial->calls == 1 && calibration->calls == 1 &&
          extractor->calls == 2);
    automatic.peak_operation = PeakOperation::reselect;
    automatic.selection = PeakSelection{};
    check(bool(evaluation.preview(automatic)) && extractor->calls == 2 && initial->calls == 1 &&
          calibration->calls == 1);
    check(evaluation.preview()->preprocessing.peak_calibration->source ==
              preprocessing::PeakCalibration::Source::manual_parameters &&
          evaluation.preview()->preprocessing.peak_calibration->formal_search_range.value == double(.2f));
    const auto opening = stages.snapshot();
    automatic.peak_operation = PeakOperation::redetect;
    automatic.selection.reset();
    automatic.peaks->search_range = -1;
    calibration->width = std::numeric_limits<double>::quiet_NaN();
    check(!evaluation.preview(automatic) && !evaluation.preview() && stages.snapshot() == opening);
    automatic.peak_operation = PeakOperation::reselect;
    automatic.selection = PeakSelection{};
    auto failedReselection = evaluation.preview(automatic);
    check(!failedReselection && failedReselection.error().reason == "candidate_pool_invalidated");
}
static void searchModules() {
    testing::ClassicFixture f;
    Reader reader;
    SearchInput input;
    input.parameters = f.parameters;
    AlgorithmSelections s;
    AlgorithmModules m;
    input.source = std::make_shared<const ProductState>(state());
    input.query.names = "owned-constraints";
    auto r = SearchEngine{}.run(input, reader, f.context);
    check(r && reader.received.names == "owned-constraints" && reader.received.decodeReflections);
    input.query.route = CandidateQuery::Route::stored_strongest;
    r = SearchEngine{}.run(input, reader, f.context);
    check(r && reader.tops == 1 && r.value().next.candidates.size() == 1);
    check(r.value().next.candidates.front().id == "B" &&
          reader.received.preselectedIds == std::vector<std::string>{"B"});
    input.query.route = CandidateQuery::Route::matching;

    auto matcher = std::make_shared<Matcher>();
    s = {};
    m = {};
    s.matcher = matcher->identity();
    m.matcher = matcher;
    input.source = std::make_shared<const ProductState>(state(assemble(s, m)));
    r = SearchEngine{}.run(input, reader, f.context);
    check(r && matcher->calls == 2 && r.value().next.candidates.front().fomPeakPos == .5);
    check(r.value().next.candidates.front().fom < 1);

    auto before = state();
    before.candidates = {card("B")};
    before.candidates[0].scale = 3;
    input.source = std::make_shared<const ProductState>(before);
    input.operation = SearchOperation::accept_phase;
    input.phase = card("A");
    input.phase->intensity = {20, 10, 5};
    input.phase->scale = 1;
    const auto reads = reader.reads, tops = reader.tops;
    r = SearchEngine{}.run(input, reader, f.context);
    check(r && r.value().next.peaks.intensity == std::vector<double>({80,190,95}) &&
          r.value().next.phases.size() == 1 && reader.reads == reads && reader.tops == tops);
    check(r.value().next.candidates.front().scale == 3 && r.value().next.candidates.front().fom < 1);
    input.source = std::make_shared<const ProductState>(r.value().next);
    const auto residualPeaks = input.source->peaks;
    auto duplicate = SearchEngine{}.run(input, reader, f.context);
    check(duplicate && !duplicate.value().changed && duplicate.value().next.peaks == residualPeaks &&
          reader.reads == reads && reader.tops == tops);
    input.operation = SearchOperation::rescore;
    check(bool(SearchEngine{}.run(input, reader, f.context)) && reader.reads == reads && reader.tops == tops);

    const std::vector<double> heights{10};
    const std::vector<search::RirInput> phases{{true, heights, 2, 2}, {true, heights, 3, 1}};
    check(search::quantify_rir(phases) == std::vector<double>({25, 75}));
}
static void preparationDecisions() {
    testing::ClassicFixture f;
    auto dense = state(defaultAlgorithms(), true);
    dense.pattern.background.reset();
    ProductSearchSession session(dense);
    auto source = session.snapshot();
    auto prepared = prepareSearch(source, SearchMethod::classic_sm);
    check(prepared && prepared.value().state == SearchPreparation::State::needs_peak_preparation &&
          prepared.value().source == source && prepared.value().peaks && session.snapshot() == source);
    auto denied = prepareSearch(source, SearchMethod::pfsm);
    check(!denied && denied.error().reason == "algorithm_provenance_mismatch" && session.snapshot() == source);
    denied = prepareSearch({}, SearchMethod::pfsm);
    check(!denied && denied.error().code == ErrorCode::missing_data);
    source = std::make_shared<const ProductState>(state());
    prepared = prepareSearch(source, SearchMethod::classic_sm);
    check(prepared && prepared.value().state == SearchPreparation::State::ready && !prepared.value().peaks);
    auto imported = *source; imported.projectMetadata["peak_snapshot_origin"] = "imported";
    prepared = prepareSearch(std::make_shared<const ProductState>(imported), SearchMethod::classic_sm);
    check(prepared && prepared.value().state == SearchPreparation::State::ready && !prepared.value().peaks);
    for (bool missingSpectrum : {true,false}) {
        auto incomplete = imported;
        if (missingSpectrum) incomplete.pattern = Pattern{};
        else incomplete.pattern.calculation_range.reset();
        auto snapshot = std::make_shared<const ProductState>(incomplete);
        denied = prepareSearch(snapshot, SearchMethod::classic_sm);
        check(!denied && denied.error().reason == (missingSpectrum ? "classical_spectrum_required" : "classical_range_required"));
        Reader reader; SearchInput input; input.source = snapshot; input.parameters = f.parameters;
        auto searched = SearchEngine{}.run(input, reader, f.context);
        check(!searched && reader.reads == 0 && reader.tops == 0);
        input.method = SearchMethod::pfsm;
        searched = SearchEngine{}.run(input, reader, f.context);
        check(!searched && searched.error().reason == "algorithm_provenance_mismatch" && reader.reads == 0);
    }
    const auto denseOpening = session.snapshot();
    ProductPreprocessingEdit edit(session); check(bool(edit.preview(*prepareSearch(denseOpening, SearchMethod::classic_sm).value().peaks)));
    check(bool(edit.commit()) && session.snapshot() != denseOpening && session.snapshot()->peaks.valid);
    const auto committed = session.snapshot();
    Reader reader; reader.cards.clear(); SearchInput input; input.source = committed; input.parameters = f.parameters;
    std::stop_source stop; stop.request_stop(); auto context = f.context; context.cancellation = stop.get_token();
    auto searched = SearchEngine{}.run(input, reader, context);
    check(!searched && searched.error().code == ErrorCode::cancelled && session.snapshot() == committed);
}
static void configurationAndTransactions() {
    testing::ClassicFixture f;
    AlgorithmSelections s;
    AlgorithmModules m;
    s.matcher.version = 3;
    check(!resolveAlgorithms(AlgorithmSpec{s}));
    s = {};
    s.matcher.id = "unknown";
    check(!resolveAlgorithms(AlgorithmSpec{s}));
    s = {};
    m.matcher = std::make_shared<Matcher>();
    check(!resolveAlgorithms(AlgorithmSpec{s}, m));
    auto matcher = std::make_shared<Matcher>();
    matcher->incompatible = true;
    s.matcher = matcher->identity();
    m = {};
    m.matcher = matcher;
    auto bad = resolveAlgorithms(AlgorithmSpec{s}, m);
    check(!bad && bad.error().reason == "incompatible_match_contract" && matcher->calls == 0);
    AlgorithmDefaults defaults;
    defaults.peaks.polynomial_degree = 2;
    check(!resolveAlgorithms(AlgorithmSpec{{}, SearchMethod::classic_sm, defaults}));
    Workspace workspace;
    auto source = workspace.session().snapshot();
    auto available = workspace.configureAlgorithms(AlgorithmSpec{{}, SearchMethod::pfsm});
    check(available && workspace.session().snapshot() != source &&
          workspace.session().snapshot()->algorithms->spec.method() == SearchMethod::pfsm);
    check(bool(workspace.configureAlgorithms(AlgorithmSpec{})));
    source = workspace.session().snapshot();
    Reader reader;
    SearchInput input;
    input.method = SearchMethod::pfsm;
    auto denied = SearchEngine{}.run(input, reader, f.context);
    check(!denied && denied.error().code == ErrorCode::missing_data && reader.reads == 0 && reader.tops == 0);
    auto prepDenied =
        automaticPreprocessingRequest(source, PreprocessingAction::prepare_search, SearchMethod::pfsm);
    check(!prepDenied && prepDenied.error().reason == "pfsm_requires_observations");

    check(bool(workspace.session().commit(source, state())));
    source = workspace.session().snapshot();
    auto task = workspace.searches().begin().value();
    s = {};
    m = {};
    auto fom = std::make_shared<Matcher>();
    s.matcher = fom->identity();
    m.matcher = fom;
    check(!workspace.configureAlgorithms(AlgorithmSpec{s}, m));
    check(task.algorithms == source->algorithms && workspace.session().snapshot() == source);
    // A canonical revision change invalidates publication, but cannot change the
    // selection or actual numerical behavior of an already-owned worker input.
    auto changed = *source;
    changed.algorithms = assemble(s, m);
    check(bool(workspace.session().commit(source, changed)));
    input = SearchInput();
    input.source = task.source;
    input.taskId = task.id;
    input.parameters = f.parameters;
    auto pending = SearchEngine{task.algorithms}.run(input, reader, task.context);
    check(pending && pending.value().next.candidates.front().fom == 1 && fom->calls == 0);
    auto stale = workspace.searches().complete(task.id, std::move(pending));
    check(!stale && stale.error().code == ErrorCode::stale_snapshot);

    Workspace preparation;
    auto extractor = std::make_shared<Extractor>();
    s = {};
    m = {};
    s.experimental_peaks = extractor->identity();
    m.experimental_peaks = extractor;
    auto preparedState = state(assemble(s, m), true);
    check(bool(preparation.session().commit(preparation.session().snapshot(), preparedState)));
    const auto frozen = preparation.session().snapshot();
    auto request = automaticPreprocessingRequest(frozen, PreprocessingAction::prepare_search).value();
    auto admission = preparation.searches().begin().value();
    PreprocessingEvaluation evaluation(frozen);
    check(bool(evaluation.preview(request, admission.context.cancellation)));
    PreparedSearchResult prepared{frozen, admission.id, 0, *evaluation.preview()};
    check(bool(preparation.searches().complete(admission.id, Result<PreparedSearchResult>{prepared})));
    const auto committed = preparation.session().snapshot();
    check(committed->peaks.valid);
    admission = preparation.searches().begin().value();
    preparation.searches().cancel();
    input = SearchInput();
    input.source = admission.source;
    input.taskId = admission.id;
    input.parameters = f.parameters;
    auto cancelled = SearchEngine{}.run(input, reader, admission.context);
    check(!preparation.searches().complete(admission.id, std::move(cancelled)));
    check(preparation.session().snapshot() == committed && committed->peaks.intensity.front() == 91);
    check(bool(preparation.configureAlgorithms(AlgorithmSpec{})));
    check(!preparation.session().snapshot()->peaks.valid && committed->peaks.valid);
    PreprocessingEvaluation frozenEvaluation(frozen);
    check(bool(frozenEvaluation.preview(request)) &&
          frozenEvaluation.preview()->peaks.intensity.front() == 91);
    check(preparation.session().snapshot()->revision > committed->revision);
}
static void invalidation() {
    auto original = state();
    original.candidates = {card("A")};
    original.phases = {card("B")};
    original.candidates[0].fomCalculated = original.phases[0].fomCalculated = true;
    original.projectMetadata["peak_snapshot_origin"] = "manual";
    Workspace workspace;
    check(bool(workspace.session().commit(workspace.session().snapshot(), original)));
    auto source = workspace.session().snapshot();
    AlgorithmSelections selections;
    auto fom = std::make_shared<Matcher>();
    selections.matcher = fom->identity();
    AlgorithmModules modules;
    modules.matcher = fom;
    check(bool(workspace.configureAlgorithms(AlgorithmSpec{selections}, modules)));
    auto changed = workspace.session().snapshot();
    check(changed->revision == source->revision + 1 && !changed->candidates[0].fomCalculated &&
          !changed->phases[0].fomCalculated && changed->peaks.tth == source->peaks.tth);
    check(bool(workspace.configureAlgorithms(AlgorithmSpec{selections}, modules)) &&
          workspace.session().snapshot() == changed); // Same immutable specification is a no-op.

    original.algorithms = changed->algorithms;
    original.pattern.processing.background_subtracted = false;
    original.pattern.background = SampledSignal{original.pattern.observed, SampleWindow{0}};
    check(bool(workspace.session().commit(changed, original)));
    source = workspace.session().snapshot();
    check(bool(workspace.configureAlgorithms(AlgorithmSpec{selections}, modules)) &&
          workspace.session().snapshot() == source && source->candidates[0].fomCalculated &&
          source->phases[0].fomCalculated); // A fixed RIR operation is not an algorithm selection.
    AlgorithmDefaults defaults;
    ++defaults.background.coefficients;
    check(bool(workspace.configureAlgorithms(AlgorithmSpec{selections, SearchMethod::classic_sm, defaults},
                                             modules)));
    changed = workspace.session().snapshot();
    check(!changed->pattern.background && changed->peaks.valid && changed->peaks.tth == source->peaks.tth &&
          changed->candidates.empty() && !changed->phases[0].fomCalculated);

    preprocessing::PeakCalibration calibration;
    calibration.applied.search_range = .2f;
    calibration.formal_search_range = AngularWidthDegrees{.2f};
    original.preprocessing.peak_calibration = calibration;
    ManualPeakChange manual;
    manual.operation = ManualPeakChange::Operation::modify;
    manual.rows = {0};
    manual.two_theta = original.peaks.tth[0];
    manual.intensity = original.peaks.intensity[0] + 1;
    manual.fwhm = original.peaks.fwhm[0];
    manual.integrated_intensity = original.peaks.integratedIntensity->front();
    check(bool(prepareManualPeaks(original, manual, WavelengthAngstrom{1.54}, AngularWidthDegrees{.15})) &&
          !original.preprocessing.peak_calibration);
    source = workspace.session().snapshot();
    workspace.searches().close();
    auto closing = workspace.configureAlgorithms(AlgorithmSpec{});
    check(!closing && closing.error().reason == "closing" && workspace.session().snapshot() == source);
}
int main() try {
    independentPreprocessing();
    preprocessingModules();
    searchModules();
    preparationDecisions();
    configurationAndTransactions();
    invalidation();
    std::cout << "PASS: staged substitutions and fixed Classical operations through explicit assembly and product use cases, "
                 "frozen inputs, revision guards and unsupported PFSM\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
