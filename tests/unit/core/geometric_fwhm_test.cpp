#include "qualpf/preprocessing/geometric_fwhm.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <source_location>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::preprocessing;
static void check(bool v, std::source_location at = std::source_location::current()) {
    if (!v) throw std::runtime_error("Geometric FWHM contract at line " + std::to_string(at.line()));
}
static void near(double a, double b, double tolerance = 1e-12,
    std::source_location at = std::source_location::current()) {
    if (!(std::abs(a - b) <= tolerance))
        throw std::runtime_error("Geometric FWHM value at line " + std::to_string(at.line()) +
            ": " + std::to_string(a) + " expected " + std::to_string(b));
}
static ExecutionContext context() { return {{}, NumericalPolicy::explicit_float64, {}}; }
static GeometricFwhmInput samples(std::initializer_list<double> y) {
    GeometricFwhmInput in;
    in.dataset = {1}; in.intensity = y; in.background.resize(y.size());
    for (std::size_t i = 0; i < y.size(); ++i) in.two_theta.emplace_back(double(i));
    return in;
}
static GeometricFwhmResult run(const GeometricFwhmInput &in, GeometricFwhmOptions options = {}) {
    auto r = estimate_geometric_fwhm(in, options, context());
    if (!r) throw std::runtime_error(r.error().message);
    return std::move(r).value();
}
static const PeakFwhmDiagnostic &selected(const GeometricFwhmResult &r) {
    check(r.state == GeometricFwhmResult::State::measured && r.selected_candidate && r.default_fwhm);
    return r.candidates[*r.selected_candidate];
}
static GeometricFwhmInput example() {
    // The unlisted interior samples are independently defined monotone values.
    auto in = samples({0, 2000, 4900, 6200, 7600, 9000, 10000, 9600, 8600, 7600, 6600, 5800, 4800, 2000, 0});
    for (std::size_t i = 0; i < in.intensity.size(); ++i) {
        in.two_theta[i].value = 29.94 + double(i) / 100;
        in.background[i] = 200; in.intensity[i] += 200;
    }
    return in;
}
static void exact_example() {
    const auto in = example();
    auto r = run(in); const auto &d = selected(r);
    near(*d.height, 70225.0 / 7, 1e-8);
    near(d.apex->value, 42003.0 / 1400);
    near(d.left->position.value, 167781.0 / 5600);
    near(d.right->position.value, 1683239.0 / 56000);
    near(r.default_fwhm->value, 5429.0 / 56000);
    check(d.noise_state == FwhmNoiseState::noise_unknown && !d.uncertainty && d.preferred_sampling);
    auto scaled = in;
    for (auto &v : scaled.intensity) v *= 1e120;
    for (auto &v : scaled.background) v *= 1e120;
    near(run(scaled).default_fwhm->value, r.default_fwhm->value);
    for (auto &v : scaled.two_theta) v.value += 1000;
    near(run(scaled).default_fwhm->value, r.default_fwhm->value, 2e-12);
}
static GeometricFwhmInput analytic(int shape, double step, double offset = .231, bool uneven = false) {
    GeometricFwhmInput in; in.dataset = {1};
    for (int i = -int(4 / step); i <= int(4 / step); ++i) {
        double x = (i + offset) * step;
        if (uneven) x += .15 * step * std::sin(double(i));
        double y;
        if (shape == 0) y = std::exp(-4 * std::log(2.0) * x * x);
        else if (shape == 1) y = 1 / (1 + 4 * x * x);
        else if (shape == 2) y = std::pow(1 + 4 * (std::pow(2.0, 2.0 / 3) - 1) * x * x, -1.5);
        else { // Split Gaussian: left/right half widths .5 and 1, geometric width 1.5.
            const double u = x < 0 ? x : x / 2;
            y = std::exp(-4 * std::log(2.0) * u * u);
        }
        in.two_theta.emplace_back(30 + x); in.intensity.push_back(1000 * y + 200 + 3 * x);
        in.background.push_back(200 + 3 * x);
    }
    return in;
}
static void analytic_shapes() {
    for (int shape = 0; shape < 4; ++shape) for (bool uneven : {false, true}) {
        double previous_worst = 1;
        for (double step : {.08, .04, .02}) {
            double worst = 0;
            for (double offset : {0.0, .231, .47}) {
                auto r = run(analytic(shape, step, offset, uneven));
                const double expected = shape == 3 ? 1.5 : 1;
                const double error = std::abs(selected(r).width->value - expected);
                // Conservative independent curvature/slope bound over these analytic profiles.
                check(error < 4 * step * step);
                worst = std::max(worst, error);
            }
            // Grid phase can improve an individual coarse result accidentally;
            // convergence is checked over the phase envelope, not every phase.
            check(worst < previous_worst);
            previous_worst = worst;
        }
    }
}
static void rejection_and_selection() {
    auto triangle = samples({0, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0});
    auto r = run(triangle);
    near(selected(r).width->value, 8); near(*selected(r).asymmetry, 0);
    check(selected(r).left->fraction == 0 && selected(r).right->fraction == 1);
    triangle.intensity.insert(triangle.intensity.end(), {0, 0, 20, 0, 0});
    triangle.background.resize(triangle.intensity.size());
    for (auto i = triangle.two_theta.size(); i < triangle.intensity.size(); ++i) triangle.two_theta.emplace_back(double(i));
    r = run(triangle); check(selected(r).sample_index == 8);
    check(r.candidates.back().rejection == FwhmRejection::undersampled);
    auto plateau = samples({0, 1, 3, 3, 3, 1, 0});
    check(run(plateau).candidates[0].rejection == FwhmRejection::plateau);
    auto half = samples({0, 4, 4, 6, 8, 6, 4, 2, 0});
    check(run(half).candidates[0].rejection == FwhmRejection::half_height_plateau);
    auto overlap = samples({0, 2, 6, 10, 8, 7, 9, 7, 3, 0});
    check(run(overlap).candidates[0].rejection == FwhmRejection::overlapping_peak);
    auto blocked = samples({0, 2, 6, 10, 8, 7, 9, 9, 7, 3, 0});
    check(run(blocked).candidates[0].rejection == FwhmRejection::overlapping_peak);
    auto ripple = samples({0, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6.8, 6.9, 6, 5, 4, 3, 2, 1, 0});
    auto ripples = run(ripple);
    check(selected(ripples).sample_index == 8 && ripples.candidates[1].rejection == FwhmRejection::insignificant);
    auto truncated = samples({6, 7, 9, 7, 4, 0});
    check(run(truncated).candidates[0].rejection == FwhmRejection::missing_left_crossing);
    auto in = example(); in.saturated.resize(in.intensity.size()); in.saturated[6] = 1;
    check(run(in).candidates[0].rejection == FwhmRejection::saturated);
    in = example(); in.intensity[3] = std::numeric_limits<double>::quiet_NaN();
    check(run(in).candidates[0].rejection == FwhmRejection::missing_left_crossing);
    in = example(); in.break_before.resize(in.intensity.size()); in.break_before[4] = 1;
    check(!run(in).default_fwhm);
    in = example(); in.valid.assign(in.intensity.size(), 1); in.valid[3] = 0;
    check(!run(in).default_fwhm);
    auto options = GeometricFwhmOptions{}; options.maximum_width = AngularWidthDegrees{.09};
    check(run(example(), options).candidates[0].rejection == FwhmRejection::width_out_of_range);
    options = {}; options.minimum_width = AngularWidthDegrees{.1};
    check(!run(example(), options).default_fwhm);
    auto density = samples({0, 2, 4, 6, 8, 10, 12, 10, 8, 6, 4, 2, 0});
    check(run(density).default_fwhm.has_value()); // Q == 6 is included.
    options = {}; options.minimum_samples_per_fwhm = std::nextafter(6.0, 7.0);
    check(!run(density, options).default_fwhm);
    auto prominence = samples({96, 100, 95});
    check(!run(prominence).candidates[0].significant);
    prominence.intensity[0] = 95;
    check(run(prominence).candidates[0].significant); // Exactly five percent.
    auto negative = samples({-8, -4, -2, -4, -8});
    check(run(negative).candidates[0].rejection == FwhmRejection::nonpositive_height);
    check(!run(samples({0, 1, 2, 3})).default_fwhm);
    auto ties = samples({0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0});
    check(selected(run(ties)).sample_index == 6);
}
static void invalid_inputs() {
    auto in = example(); in.background.pop_back(); check(!estimate_geometric_fwhm(in, {}, context()));
    in = example(); in.two_theta[2] = in.two_theta[1]; check(!estimate_geometric_fwhm(in, {}, context()));
    in = example(); in.two_theta[2].value = std::numeric_limits<double>::infinity();
    check(!estimate_geometric_fwhm(in, {}, context()));
    in = example(); in.valid = {1}; check(!estimate_geometric_fwhm(in, {}, context()));
    auto c = context(); c.numerical_policy = NumericalPolicy::legacy_mixed_precision;
    check(!estimate_geometric_fwhm(example(), {}, c));
    std::stop_source stop; stop.request_stop(); c = context(); c.cancellation = stop.get_token();
    auto r = estimate_geometric_fwhm(example(), {}, c); check(!r && r.error().code == ErrorCode::cancelled);
    check(run(samples({})).state == GeometricFwhmResult::State::no_eligible_peak);
}
static void noise_and_covariance() {
    auto in = example(); in.noise = FwhmPoissonCounts{};
    const auto r = run(in); const auto &d = selected(r);
    // Independent rational g^T diag(N) g from the seven participating samples.
    const double expected_variance = 17311769493.0 / 12984608000000000.0;
    near(d.uncertainty->value, std::sqrt(expected_variance), 1e-13);
    check(d.noise_state == FwhmNoiseState::noise_verified && d.conditional_uncertainty && d.preferred_snr);
    near(*d.snr, (70225.0 / 7) / std::sqrt(10200.0), 1e-9);
    auto rates = in; FwhmPoissonRates model; model.counts = in.intensity;
    model.seconds.assign(in.intensity.size(), 10);
    for (auto &v : rates.intensity) v /= 10;
    for (auto &v : rates.background) v /= 10;
    rates.noise = model;
    near(selected(run(rates)).uncertainty->value, d.uncertainty->value, 1e-13);
    std::get<FwhmPoissonRates>(rates.noise).seconds[0] = 0;
    check(!estimate_geometric_fwhm(rates, {}, context()));
    rates.noise = model; rates.intensity[0] += 1;
    check(!estimate_geometric_fwhm(rates, {}, context()));

    auto triangle = samples({0, 1, 2, 3, 4, 5, 6, 7, 8, 7, 6, 5, 4, 3, 2, 1, 0});
    auto options = GeometricFwhmOptions{};
    options.minimum_snr = 0; options.minimum_prominence_snr = 0;
    const auto n = triangle.intensity.size();
    triangle.noise = FwhmStandardDeviations{std::vector<double>(n, 1)};
    // At the exact symmetric apex/crossings: gH = e4 - e8 + e12.
    near(selected(run(triangle, options)).uncertainty->value, std::sqrt(3.0));
    FwhmCovariance cov; cov.diagonal.assign(n, 1);
    cov.upper = {{4, 12, -.5}}; triangle.noise = cov;
    near(selected(run(triangle, options)).uncertainty->value, std::sqrt(2.0));
    cov.upper = {{4, 8, .5}}; triangle.noise = cov;
    near(selected(run(triangle, options)).uncertainty->value, std::sqrt(2.0));
    cov.upper = {{4, 12, .5}}; triangle.noise = cov;
    near(selected(run(triangle, options)).uncertainty->value, 2);
    cov.upper.clear();
    for (std::size_t i = 0; i < n; ++i) for (auto j = i + 1; j < n; ++j) cov.upper.push_back({i, j, 1});
    triangle.noise = cov; // Rank-one PSD covariance, including distant correlations.
    near(selected(run(triangle, options)).uncertainty->value, 1);
    cov.upper = {{4, 8, .9}, {4, 12, .9}, {8, 12, -.9}}; triangle.noise = cov;
    auto bad = estimate_geometric_fwhm(triangle, options, context());
    check(!bad && bad.error().message == "indefinite_local_covariance");
    cov.upper = {{4, 8, 1.1}}; triangle.noise = cov; check(!estimate_geometric_fwhm(triangle, options, context()));
    cov.upper = {{4, 8, .1}, {4, 8, .1}}; triangle.noise = cov;
    check(!estimate_geometric_fwhm(triangle, options, context()));
    cov.upper = {{8, 4, .1}}; triangle.noise = cov; check(!estimate_geometric_fwhm(triangle, options, context()));
    cov.upper.clear(); cov.diagonal[0] = -1; triangle.noise = cov;
    check(!estimate_geometric_fwhm(triangle, options, context()));
    cov.diagonal.assign(n, 0); triangle.noise = cov;
    auto zero = run(triangle); near(selected(zero).uncertainty->value, 0);
    check(selected(zero).noise_state == FwhmNoiseState::zero_noise && !selected(zero).snr);
    cov.upper = {{4, 8, .1}}; triangle.noise = cov; check(!estimate_geometric_fwhm(triangle, options, context()));
    auto spike = samples({0, 8, 0}); spike.noise = FwhmStandardDeviations{{1, 1, 1}};
    auto narrow = options; narrow.minimum_samples_per_fwhm = 1;
    // Shared apex/bracket indices cancel exactly: gH = (e0 + e2) / 16.
    near(selected(run(spike, narrow)).uncertainty->value, std::sqrt(1.0 / 128));
    auto tied = samples({0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0, 1, 2, 3, 4, 5, 6, 5, 4, 3, 2, 1, 0});
    FwhmStandardDeviations tied_noise{std::vector<double>(tied.intensity.size(), .1)};
    for (std::size_t i = 13; i < tied_noise.values.size(); ++i) tied_noise.values[i] = .05;
    tied.noise = tied_noise;
    check(selected(run(tied)).sample_index == 18); // Uncertainty precedes index tie-break.
    triangle.noise = FwhmStandardDeviations{std::vector<double>(n, .4)};
    check(run(triangle).default_fwhm.has_value()); // SNR 20, inclusive.
    std::get<FwhmStandardDeviations>(triangle.noise).values.assign(n, .5);
    check(run(triangle).candidates[0].rejection == FwhmRejection::insufficient_snr);
    std::get<FwhmStandardDeviations>(triangle.noise).values.assign(n, 2);
    check(run(triangle).candidates[0].rejection == FwhmRejection::insignificant);
    std::get<FwhmStandardDeviations>(triangle.noise).values.assign(n, 1.6);
    check(run(triangle).candidates[0].significant); // Prominence SNR 5, inclusive.

    // Low-noise Monte Carlo: fixed brackets/apex and independently generated noise.
    in = example(); in.noise = FwhmStandardDeviations{std::vector<double>(in.intensity.size(), .1)};
    const double predicted = selected(run(in)).uncertainty->value;
    std::mt19937_64 engine(14371); std::normal_distribution<double> normal(0, .1);
    double mean = 0, m2 = 0;
    for (int k = 1; k <= 4000; ++k) {
        auto perturbed = in;
        for (auto &y : perturbed.intensity) y += normal(engine);
        auto out = run(perturbed); const auto &peak = selected(out);
        check(peak.sample_index == 6 && peak.left->first == 2 && peak.right->first == 11);
        const double value = peak.width->value, delta = value - mean;
        mean += delta / k; m2 += delta * (value - mean);
    }
    const double empirical = std::sqrt(m2 / 3999);
    check(std::abs(empirical / predicted - 1) < .08);

    // Nonuniform-grid derivative cross-check, using independent central differences
    // of each observation instead of the analytic propagation implementation.
    in = analytic(0, .04, .231, true);
    in.noise = FwhmStandardDeviations{std::vector<double>(in.intensity.size(), .1)};
    auto base = run(in); const auto &peak = selected(base);
    double finite_difference_variance = 0;
    for (auto i : {peak.sample_index - 1, peak.sample_index, peak.sample_index + 1,
        peak.left->first, peak.left->second, peak.right->first, peak.right->second}) {
        auto plus = in, minus = in;
        plus.intensity[i] += .001; minus.intensity[i] -= .001;
        const double g = (run(plus).default_fwhm->value - run(minus).default_fwhm->value) / .002;
        finite_difference_variance += .01 * g * g;
    }
    near(peak.uncertainty->value, std::sqrt(finite_difference_variance), 2e-10);
}
int main() {
    try { exact_example(); analytic_shapes(); rejection_and_selection(); invalid_inputs(); noise_and_covariance(); }
    catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
    std::cout << "Geometric FWHM geometry, selection and error contracts passed\n";
}
