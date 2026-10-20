#include "qualpf/application/geometric_fwhm.h"
#include "qualpf/io/pattern_reader.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <source_location>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
static void check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("Geometric workflow at line " + std::to_string(at.line()));
}
static void contracts() {
    GeometricFwhmRequest request;
    auto &in = request.input; in.dataset = {71}; in.source_id = "owned-draft";
    in.radiation = {RadiationKind::x_ray, {{WavelengthAngstrom{1.54}, 1}, {WavelengthAngstrom{1.545}, .5}}};
    in.processing = {false, false, true};
    for (int i = 0; i <= 20; ++i) {
        in.two_theta.emplace_back(20 + .01 * i);
        in.intensity.push_back(28 - std::abs(i - 10)); in.background.push_back(20);
    }
    ProductState state; state.pattern.id = in.dataset; state.pattern.two_theta = in.two_theta;
    // Deliberately different: implicit selection/subtraction would fail the width contract.
    state.pattern.observed.assign(21, 999); state.pattern.original_observed = std::vector<double>(21, 888);
    state.pattern.weights = std::vector<double>(21, 777);
    ProductSearchSession session(state);
    auto source = session.snapshot();
    ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    auto result = evaluateGeometricFwhm(source, request, context);
    check(bool(result)); const auto &measurement = result.value().measurement;
    check(result.value().source == source && session.snapshot() == source);
    check(measurement.default_fwhm && std::abs(measurement.default_fwhm->value - .08) < 1e-12);
    check(measurement.source_id == "owned-draft" && measurement.radiation.lines.size() == 2);
    check(measurement.processing.kalpha2_subtracted == false);
    check(measurement.noise_model == preprocessing::FwhmNoiseModel::unknown);
    check(source->algorithms->modules.fwhm->identity() == preprocessing::ClassicFwhmEstimator{}.identity());
    check(request.input.intensity.front() - request.input.background.front() == -2);
    auto wrong = request; wrong.input.dataset = {72};
    auto error = evaluateGeometricFwhm(source, wrong, context);
    check(!error && error.error().code == ErrorCode::stale_snapshot);
    check(!evaluateGeometricFwhm({}, request, context));
    wrong = request; wrong.input.background.clear(); check(!evaluateGeometricFwhm(source, wrong, context));
    std::stop_source stop; stop.request_stop(); context.cancellation = stop.get_token();
    error = evaluateGeometricFwhm(source, request, context);
    check(!error && error.error().code == ErrorCode::cancelled && session.snapshot() == source);
    // A held snapshot/result survives subsequent owner changes without publishing itself.
    session.reset(); check(result.value().source == source && source != session.snapshot());
}
static void real_patterns(const std::string &root) {
    for (const char *file : {"02_multiphase_benchmarks/01_smoke/example1.dat",
         "02_multiphase_benchmarks/03_model_mismatch/CPD2.dat", "02_multiphase_benchmarks/04_complex/MIXT_5.dat"}) {
        const std::string path = root + "/" + file;
        std::ifstream stream(path); check(bool(stream));
        auto parsed = io::read_pattern(stream, ".dat", file); check(bool(parsed));
        auto pattern = std::move(parsed).value().pattern;
        auto background = preprocessing::CppBackgroundEstimator{}.estimate(pattern, {},
            pattern.radiation.lines.front().wavelength, std::nullopt,
            ExecutionContext{{}, NumericalPolicy::legacy_mixed_precision, {}});
        check(bool(background));
        ProductState state; state.pattern = pattern;
        ProductSearchSession session(state); auto source = session.snapshot();
        GeometricFwhmRequest request;
        auto &in = request.input;
        in.dataset = pattern.id; in.source_id = pattern.source_id; in.radiation = pattern.radiation;
        in.processing = pattern.processing; in.two_theta = pattern.two_theta;
        in.intensity = pattern.observed; in.background = background.value().values;
        auto result = evaluateGeometricFwhm(source, request, ExecutionContext{{}, NumericalPolicy::explicit_float64, {}});
        check(bool(result) && session.snapshot() == source);
        const auto &out = result.value().measurement;
        std::size_t accepted = 0; std::map<std::string, std::size_t> rejected;
        for (const auto &d : out.candidates) {
            if (d.eligible) ++accepted;
            else ++rejected[preprocessing::fwhm_rejection_name(d.rejection)];
        }
        std::cout << file << ": candidates=" << out.candidates.size() << " eligible=" << accepted;
        if (out.selected_candidate) {
            const auto &d = out.candidates[*out.selected_candidate];
            check(d.eligible && d.noise_state == preprocessing::FwhmNoiseState::noise_unknown);
            check(d.width && d.left && d.right && *d.sampling_density >= request.options.minimum_samples_per_fwhm);
            std::cout << " selected_index=" << d.sample_index << " apex=" << d.apex->value
                      << " fwhm=" << out.default_fwhm->value << " noise_unknown";
        } else check(accepted == 0 && !out.default_fwhm);
        for (auto [reason, count] : rejected) std::cout << " " << reason << "=" << count;
        std::cout << '\n';
    }
}
int main(int argc, char **argv) {
    try {
        contracts();
        if (argc == 3 && std::string(argv[1]) == "--real-data") real_patterns(argv[2]);
        else if (argc != 1) throw std::runtime_error("Usage: geometric_fwhm_application [--real-data EXAMPLE_ROOT]");
    }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "Source-bound geometric FWHM evaluation passed; real spectra have no accuracy goldens\n";
}
