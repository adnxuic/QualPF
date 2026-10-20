#include "classic_fixture.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/search_coordinator.h"
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v, std::source_location at = std::source_location::current()) {
    if (!v) throw std::runtime_error("Algorithm payload at line " + std::to_string(at.line()));
}
static void corrupt(PeakSet &p, int kind) {
    switch (kind) {
    case 0: p.effective_wavelength.reset(); break;
    case 1: p.peaks[0].d_spacing.reset(); break;
    case 2: p.peaks[0].delta_d.reset(); break;
    case 3: p.peaks[0].original_intensity.reset(); break;
    case 4: p.peaks[0].fwhm.reset(); break;
    case 5: p.peaks[0].integrated_intensity.reset(); break;
    case 6: p.peaks[0].intensity = std::numeric_limits<double>::quiet_NaN(); break;
    case 7: p.peaks[1].id = p.peaks[0].id; break;
    case 8: p.effective_wavelength = WavelengthAngstrom{2}; break;
    case 9: p.dataset = DatasetId{42}; break;
    }
}
struct Extractor : preprocessing::IExperimentalPeakExtractor {
    int kind;
    explicit Extractor(int k) : kind(k) {}
    AlgorithmIdentity identity() const override { return {"test.payload.extractor", 1}; }
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &spectrum, const preprocessing::PeakCalibration &c,
        WavelengthAngstrom w, AngularWidthDegrees, const ExecutionContext &) const override {
        const auto &pattern = spectrum.pattern();
        preprocessing::PeakSearchResult r; r.applied = c.applied; r.automatic_tolerance = .13;
        r.peaks = testing::ClassicFixture{}.peaks; r.peaks.effective_wavelength = w;
        corrupt(r.peaks, kind); r.candidates = r.peaks;
        if (kind == 10) r.candidates.dataset = DatasetId{42};
        if (kind >= 11) {
            r.rejected_candidates.push_back({preprocessing::PeakDetectionStage::formal, 0, 0,
                TwoThetaDegrees{double(float(pattern.two_theta.front().value))}, "coincident_baseline_endpoints"});
            auto &rejected = r.rejected_candidates.front();
            if (kind == 11) rejected.sample_index = pattern.two_theta.size();
            if (kind == 12) rejected.two_theta.value = std::numeric_limits<double>::quiet_NaN();
            if (kind == 13) rejected.two_theta.value += 1;
            if (kind == 14) rejected.stage = preprocessing::PeakDetectionStage::initial;
            if (kind == 15) rejected.reason = "unknown_rejection";
        }
        return Result<preprocessing::PeakSearchResult>{r};
    }
};
struct MalformedMatcher : search::IClassicMatcher {
    int kind;
    explicit MalformedMatcher(int value) : kind(value) {}
    AlgorithmIdentity identity() const override { return {"test.payload.matcher", 1}; }
    AlgorithmIdentity produces() const override { return search::classic_match_contract(); }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &input, const ExecutionContext &context) const override {
        auto result = search::ClassicMatcher{}.match(input, context);
        if (!result) return result;
        auto &evidence = result.value();
        switch (kind) {
        case 0: evidence.merged.clear(); break;
        case 1: evidence.sample_y[0] = std::numeric_limits<float>::quiet_NaN(); break;
        case 2: evidence.reflection_indices[0] = 42; break;
        case 3: evidence.experimental_indices[0] = 42; break;
        case 4: evidence.position_difference_sum = -1; break;
        case 5: std::swap(evidence.sample_x.front(), evidence.sample_x.back()); break;
        case 6: evidence.contract = {"test.invalid.contract", 1}; break;
        case 7: evidence.merged[0] = -1; break;
        }
        return result;
    }
};
struct Reader : CandidateReader {
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        throw std::runtime_error("No database work expected");
    }
};
int main() try {
    testing::ClassicFixture f;
    ProductState state; state.pattern = f.pattern;
    state.pattern.radiation.lines = {{WavelengthAngstrom{1.54}, 1}};
    auto peaks = f.peaks; peaks.effective_wavelength = WavelengthAngstrom{1.54};
    state.peaks = productMeasuredPeaks(peaks).value();
    ProductCard phase; phase.id = "A"; phase.scale = 3; phase.fomCalculated = true;
    for (const auto &r : f.card.reflections) { phase.tth.push_back(r.two_theta->value); phase.intensity.push_back(r.intensity); }
    Reader reader;
    for (int kind : {0,1,2,3,4,5,6,7,9,10,11,12,13,14,15}) {
        auto extractor = std::make_shared<Extractor>(kind); AlgorithmSelections s; s.experimental_peaks = extractor->identity();
        AlgorithmModules m; m.experimental_peaks = extractor;
        state.algorithms = resolveAlgorithms(AlgorithmSpec{s}, m).value();
        ProductSearchSession session(state); const auto source = session.snapshot();
        PreprocessingEvaluation evaluation(source);
        PreprocessingRequest request; request.wavelength = WavelengthAngstrom{1.54}; request.tolerance = AngularWidthDegrees{.25};
        request.peaks = preprocessing::PeakOptions{}; request.peaks->search_range = .2f;
        auto status = evaluation.preview(request);
        if (kind == 5) { // Area is legitimately optional for newly extracted tables.
            check(bool(status) && evaluation.preview() && !evaluation.preview()->peaks.integratedIntensity);
        } else {
            check(!status && status.error().reason == "invalid_algorithm_output" && !evaluation.preview() && session.snapshot() == source);
            SearchCoordinator coordinator(session); const auto task = coordinator.begin().value();
            check(!coordinator.complete(task.id, Result<PreparedSearchResult>{status.error()}) && !coordinator.running() && session.snapshot() == source);
        }
    }
    // Preserve measured-boundary assertions without a residual replacement port.
    // Dataset binding is checked by the formal-use-case cases above, rather
    // than by this source-independent measured-column converter.
    for (int kind = 0; kind != 9; ++kind) {
        auto malformed = peaks; corrupt(malformed, kind);
        auto measured = productMeasuredPeaks(malformed, true, WavelengthAngstrom{1.54});
        if (kind == 0) check(bool(measured)); // Source wavelength legitimately supplies the missing value.
        else check(!measured && measured.error().reason == "invalid_algorithm_output");
    }
    for (int kind = 0; kind != 8; ++kind) {
        auto matcher = std::make_shared<MalformedMatcher>(kind);
        AlgorithmSelections selected; selected.matcher = matcher->identity();
        AlgorithmModules modules; modules.matcher = matcher;
        state.algorithms = resolveAlgorithms(AlgorithmSpec{selected}, modules).value();
        auto remaining = phase; remaining.id = "B"; state.candidates = {remaining};
        ProductSearchSession session(state); SearchCoordinator coordinator(session);
        auto task = coordinator.begin().value();
        SearchInput input; input.source = task.source; input.taskId = task.id; input.operation = SearchOperation::accept_phase;
        input.phase = phase; input.parameters = f.parameters;
        auto result = SearchEngine{task.algorithms}.run(input, reader, task.context);
        check(!result); // Invalid matcher output fails after the fixed residual has been prepared.
        auto completed = coordinator.complete(task.id, std::move(result));
        check(!completed && !coordinator.running() && session.snapshot() == task.source && task.source->phases.empty() &&
              task.source->peaks.intensity == state.peaks.intensity);
    }
    state.algorithms = defaultAlgorithms();
    const auto fixed = SearchEngine{}.residual(state, phase, f.parameters, f.context);
    check(fixed && fixed.value().intensity == std::vector<double>({70,140,70}));
    check(search::update_classic_residual(peaks, f.card, phase.scale, f.parameters, f.context)
              .value().peaks.dataset == peaks.dataset);
    auto invalidPhase = phase; invalidPhase.scale = std::numeric_limits<double>::quiet_NaN();
    check(!SearchEngine{}.residual(state, invalidPhase, f.parameters, f.context));
    std::cout << "PASS: malformed extractor/matcher successes reject atomically; fixed residual and optional measured fields remain covered\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
