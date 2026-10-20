#include "qualpf/application/preprocessing_evaluation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
namespace {
void require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("Peak rejection contract at line " + std::to_string(at.line()));
}
ProductState fixture(const char *name) {
    std::ifstream input(std::string(QUALPF_PREPROCESSING_FIXTURES) + "/" + name + "-peaks.wire");
    int operation, n;
    double wavelength, tolerance;
    require(bool(input >> operation >> n >> wavelength >> tolerance));
    ProductState state;
    auto &p = state.pattern;
    p.id = {1};
    p.radiation = {RadiationKind::x_ray, {{WavelengthAngstrom{1.54059307}, 1}}};
    p.processing.background_subtracted = false;
    p.processing.intensity_scaled = true;
    p.processing.kalpha2_subtracted = false;
    for (int i = 0; i < n; ++i) {
        double x, y;
        require(bool(input >> x >> y));
        p.two_theta.push_back(TwoThetaDegrees{x});
        p.observed.push_back(y);
    }
    p.original_observed = p.observed;
    const float maximum = float(*std::max_element(p.observed.begin(), p.observed.end()));
    for (auto &y : p.observed) y = (1000.f * float(y)) / maximum;
    p.intensity_scale = IntensityScale::relative;
    return state;
}
void apply(ProductSearchSession &session, const PreprocessingRequest &request) {
    ProductPreprocessingEdit edit(session);
    const auto status = edit.preview(request);
    if (!status) throw std::runtime_error(status.error().reason + ": " + status.error().message);
    require(bool(edit.commit()));
}
// Preserve the original subtraction -> SG reproduction and its literal failed
// candidate observation through the retained numerical kernels. Product stages
// now consume raw/background inputs and may legitimately select other triggers.
std::vector<preprocessing::RejectedPeakCandidate> historicalDiagnostics(const char *name) {
    auto pattern = fixture(name).pattern;
    auto background = preprocessing::background(pattern, {}, WavelengthAngstrom{1.54059307});
    require(bool(background));
    pattern.background = SampledSignal{background.value().values,SampleWindow{0}};
    auto net = preprocessing::subtract_background(pattern); require(bool(net));
    auto smoothed = preprocessing::smooth(net.value().observed, {}); require(bool(smoothed));
    net.value().observed = std::move(smoothed).value();
    preprocessing::PeakOptions options; options.search_range = -1;
    options.half_window = 42; options.automatic_window = false;
    options.detection_range = AngularRange{TwoThetaDegrees{15.98},TwoThetaDegrees{113.77}};
    auto peaks = preprocessing::find_peaks(net.value(), options, WavelengthAngstrom{1.54059307}, AngularWidthDegrees{.15});
    require(bool(peaks)); return peaks.value().rejected_candidates;
}
bool finitePool(const ProductState &state) {
    require(bool(state.preprocessing.peak_candidates));
    for (const auto &p : state.preprocessing.peak_candidates->peaks)
        if (!std::isfinite(p.intensity) || !p.fwhm || !std::isfinite(p.fwhm->value) ||
            !p.integrated_intensity || !std::isfinite(*p.integrated_intensity)) return false;
    return !state.peaks.tth.empty();
}
} // namespace
void peakRejectionContracts() {
    for (const auto *name : {"example1", "CPD2", "MIXT_5"}) {
        ProductSearchSession session(fixture(name));
        auto subtract = automaticPreprocessingRequest(session.snapshot(), PreprocessingAction::generate_background);
        require(bool(subtract));
        apply(session, subtract.value());
        PreprocessingRequest smooth;
        smooth.smoothing = preprocessing::SmoothingOptions{}; // SG half-window 4, degree 5.
        smooth.wavelength = WavelengthAngstrom{1.54059307};
        smooth.tolerance = AngularWidthDegrees{.15};
        apply(session, smooth);
        const auto source = session.snapshot();
        auto request = automaticPreprocessingRequest(source, PreprocessingAction::automatic_peaks);
        require(bool(request));
        request.value().peaks->search_range = -1;
        request.value().peaks->half_window = 42; // Conditions sensitivity 17.
        request.value().peaks->automatic_window = false;
        request.value().selection = PeakSelection{15.98, 113.77, 1.5f, -1, false};
        PreprocessingEvaluation evaluation(source);
        auto status = evaluation.preview(request.value());
        if (!status) throw std::runtime_error(std::string(name) + ": " + status.error().message);
        require(finitePool(*evaluation.preview()) && session.snapshot() == source);
        const auto diagnostics = evaluation.preview()->preprocessing.rejected_candidates;
        require(!diagnostics.empty());
        require(bool(validatePeakRejections(diagnostics, source->pattern)));
        if (std::string(name) == "example1")
            { const auto historical = historicalDiagnostics(name);
            require(std::any_of(historical.begin(), historical.end(), [](const auto &r) {
                return r.stage == preprocessing::PeakDetectionStage::initial && r.candidate_index == 37 &&
                    r.sample_index == 343 && std::abs(r.two_theta.value - 25.926) < .00001 &&
                    r.reason == "coincident_baseline_endpoints";
            })); }
        require(bool(evaluation.preview(request.value())));
        require(evaluation.preview()->preprocessing.rejected_candidates == diagnostics);
        auto formal = request.value();
        formal.peaks->search_range = .15f; // Skip initial/FWHM; exercise a sliced formal pass.
        formal.peaks->automatic_window = true;
        formal.selection->minimum = 20;
        formal.selection->maximum = 80;
        PreprocessingEvaluation sliced(source);
        require(bool(sliced.preview(formal)) && finitePool(*sliced.preview()));
        const auto &formalDiagnostics = sliced.preview()->preprocessing.rejected_candidates;
        if (std::string(name) == "example1") require(!formalDiagnostics.empty());
        for (const auto &r : formalDiagnostics)
            require(r.stage == preprocessing::PeakDetectionStage::formal &&
                    r.two_theta.value >= 20 && r.two_theta.value <= 80 &&
                    r.two_theta.value == double(float(source->pattern.two_theta[r.sample_index].value)));
        auto select = request.value();
        select.peak_operation = PeakOperation::reselect;
        select.selection->threshold_percent = 5;
        select.selection->maximum_peaks = 3;
        require(bool(evaluation.preview(select)) && evaluation.preview()->peaks.tth.size() <= 3);
        require(evaluation.preview()->preprocessing.rejected_candidates == diagnostics);
        select.selection->append = true;
        select.selection->minimum = 30;
        select.selection->maximum = 50;
        require(bool(evaluation.preview(select)) && finitePool(*evaluation.preview()));
        require(evaluation.preview()->preprocessing.rejected_candidates == diagnostics);
        std::stop_source canceled;
        canceled.request_stop();
        require(!evaluation.preview(request.value(), canceled.get_token()) && !evaluation.preview());
        require(!evaluation.preview(select) && !evaluation.preview());
        require(session.snapshot() == source);
        auto invalid = request.value();
        invalid.peaks->polynomial_degree = 2;
        require(!evaluation.preview(invalid) && !evaluation.preview());
        require(!evaluation.preview(select) && !evaluation.preview() && session.snapshot() == source);
        ProductPreprocessingEdit edit(session);
        require(bool(edit.preview(request.value())));
        edit.cancel();
        require(session.snapshot() == source);
        ProductPreprocessingEdit stale(session);
        require(bool(stale.preview(request.value())));
        session.replacePeaks(source->peaks);
        const auto changed = session.snapshot();
        require(!stale.commit() && session.snapshot() == changed);
        apply(session, request.value());
        require(finitePool(*session.snapshot()) && session.snapshot()->preprocessing.rejected_candidates == diagnostics);
        auto represented = preprocessingValues(session.snapshot()->preprocessing);
        require(!represented.contains("rejected_candidates")); // Runtime only; QXP remains unchanged.
        apply(session, smooth);
        require(!session.snapshot()->preprocessing.peak_candidates &&
                session.snapshot()->preprocessing.rejected_candidates.empty());
        // Automatic and Search preparation share the explicit rejection rule.
        for (const auto action : {PreprocessingAction::automatic_peaks, PreprocessingAction::prepare_search}) {
            ProductSearchSession automatic(*source);
            auto ready = automaticPreprocessingRequest(automatic.snapshot(), action);
            require(bool(ready));
            apply(automatic, ready.value());
            require(finitePool(*automatic.snapshot()) &&
                    !automatic.snapshot()->preprocessing.rejected_candidates.empty());
        }
        std::cout << "PASS: " << name << " subtract + SG + peak rejection/reselection/publication\n";
    }
    for (bool initial : {true, false}) {
        AlgorithmSelections selected;
        (initial ? selected.initial_peaks : selected.experimental_peaks).version = 1;
        const auto old = resolveAlgorithms(AlgorithmSpec{selected});
        require(!old && old.error().reason == "unsupported_algorithm_contract");
    }
    for (bool initial : {true, false}) {
        AlgorithmSelections selected;
        (initial ? selected.initial_peaks : selected.fwhm).version = initial ? 2 : 1;
        const auto old = resolveAlgorithms(AlgorithmSpec{selected});
        require(!old && old.error().reason == "unsupported_algorithm_contract");
    }
}
