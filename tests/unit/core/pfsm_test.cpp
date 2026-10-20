#include "qualpf/profile/pfsm.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <source_location>
#include <stdexcept>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

using namespace qualpf;
using namespace qualpf::profile;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok)
        throw std::runtime_error("PFSM contract at line " + std::to_string(at.line()));
}
static void near(double a, double b, double tol = 1e-11,
                 std::source_location at = std::source_location::current()) {
    if (!(std::abs(a - b) <= tol))
        throw std::runtime_error("PFSM value at line " + std::to_string(at.line()) + ": " +
                                 std::to_string(a) + " expected " + std::to_string(b));
}
static ExecutionContext ctx() { return {{}, NumericalPolicy::explicit_float64, {}}; }
template <class T> static T take(Result<T> r) {
    if (!r)
        throw std::runtime_error(r.error().field + ": " + r.error().message);
    return std::move(r).value();
}
// Independent direct mathematical expression, not values captured from the
// implementation.
static double il(double x, double q, double h) {
    return std::pow(1 + 4 * (std::pow(2., 2. / 3) - 1) * std::pow((x - q) / h, 2), -1.5);
}
static ProfileTemplate makeProfile(std::vector<ProfileLine> lines, double h = .2) {
    ProfileExpansionInput in;
    in.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees;
    in.lines = std::move(lines);
    in.fwhm_degrees = h;
    return take(IntermediateLorentzianExpander{}.prepare(in, ctx()));
}
static void expansion() {
    auto p = makeProfile({{1, 30, 7}});
    const std::vector<double> x{29, 29.9, 30, 30.1, 30.47, 31};
    auto y = take(p->evaluate(x, 0, true, ctx()));
    near(y.intensity[2], 7);
    near(y.intensity[1], 3.5);
    near(y.intensity[3], 3.5);
    auto plus = take(p->evaluate(x, 1e-6, false, ctx()));
    auto minus = take(p->evaluate(x, -1e-6, false, ctx()));
    for (std::size_t i = 0; i < x.size(); ++i) {
        near(y.intensity[i], 7 * il(x[i], 30, .2));
        near(y.shift_derivative[i], (plus.intensity[i] - minus.intensity[i]) / 2e-6, 5e-8);
    }
    check(y.shift_derivative[1] < 0 && y.shift_derivative[3] > 0);
    near(take(p->evaluate(std::vector<double>{30.07}, .07, false, ctx())).intensity[0], 7);
    auto overlap = makeProfile({{1, 30, 7}, {2, 30, 2}, {3, 50, 3}});
    auto sum = take(overlap->evaluate(x, 0, false, ctx()));
    // Derivative requests use the scalar expression; coordinate SIMD must keep
    // each reflection reduction and every intensity bit identical to that path.
    check(sum.intensity == take(overlap->evaluate(x, 0, true, ctx())).intensity);
    for (std::size_t i = 0; i < x.size(); ++i)
        near(sum.intensity[i], 9 * il(x[i], 30, .2) + 3 * il(x[i], 50, .2));
    // Finite-interval integral from the closed-form antiderivative.
    std::vector<double> grid;
    constexpr int n = 20000;
    constexpr double lo = 28, hi = 32, step = (hi - lo) / n;
    for (int i = 0; i <= n; ++i)
        grid.push_back(lo + step * i);
    auto samples = take(p->evaluate(grid, 0, false, ctx()));
    double integral = 0;
    for (int i = 0; i <= n; ++i)
        integral += (i == 0 || i == n ? 1 : i % 2 ? 4 : 2) * samples.intensity[i] * step / 3;
    const double a = 4 * (std::pow(2., 2. / 3) - 1) / (.2 * .2);
    near(integral, 28 / std::sqrt(1 + 4 * a), 2e-12);
    ProfileExpansionInput d;
    d.fwhm_degrees = .2;
    d.lines = {{1, .1, 1000}, {2, 2, 9}};
    d.radiation.lines = {{WavelengthAngstrom{1.}, 2}, {WavelengthAngstrom{1.1}, 1}};
    auto doublet = take(IntermediateLorentzianExpander{}.prepare(d, ctx()));
    auto ys = take(doublet->evaluate(x, 0, false, ctx()));
    for (std::size_t i = 0; i < x.size(); ++i)
        near(ys.intensity[i], 6 * il(x[i], 2 * std::asin(.25) * 180 / std::numbers::pi, .2) +
                                  3 * il(x[i], 2 * std::asin(.275) * 180 / std::numbers::pi, .2));
    d.radiation.lines.clear();
    check(!IntermediateLorentzianExpander{}.prepare(d, ctx()));
    std::stop_source stop;
    stop.request_stop();
    auto canceled = ctx();
    canceled.cancellation = stop.get_token();
    check(!p->evaluate(x, 0, false, canceled));
    check(!p->evaluate(std::vector<double>{1, 1}, 0, false, ctx()));
    // Independent power expression across the direct-square and overflow-safe
    // paths. Relative error is checked before results underflow to exact zero.
    const std::vector<double> wide{-1e151, -1e90, -1e20, -10, 0, .01, .1, 1, 10, 1e20, 1e90, 1e151};
    auto origin = makeProfile({{1, 0, 1}});
    auto wideValues = take(origin->evaluate(wide, 0, false, ctx()));
    for (std::size_t i = 0; i < wide.size(); ++i) {
        const long double distance = static_cast<long double>(wide[i]) / .2L;
        const auto expected =
            static_cast<double>(std::pow(1 + 4 * (std::pow(2.L, 2.L / 3) - 1) * distance * distance, -1.5L));
        if (expected == 0)
            check(wideValues.intensity[i] == 0);
        else
            check(std::abs(wideValues.intensity[i] / expected - 1) < 3e-14);
    }
}
static void expansionRemainder() {
#if defined(__SSE2__) || defined(_M_X64)
    // The accepted SSE2 path uses hardware sqrt for pairs, and the platform
    // sqrt for an odd last sample. Those providers can differ by one ULP on
    // Windows GCC. Assemble the reference with that distinction explicitly;
    // this sensitive finite input exposed a real accepted-phase fit mismatch.
    const double width = .18042705226676503;
    double (*volatile platformAsin)(double) = std::asin;
    const double position = 2 * platformAsin(.5 * 1.540598 / .918) * (180 / std::numbers::pi);
    const double shift = -.0004089228735565984;
    const double factor = std::sqrt(4 * (std::cbrt(4.) - 1)) / width;
    double (*volatile platformSqrt)(double) = std::sqrt;
    const std::vector<ProfileLine> lines{{1, position, 177.681595}, {2, 30, 7}, {3, 30, 2}};
    const auto profile = makeProfile(lines, width);
    for (std::size_t count = 1; count <= 15; ++count) {
        std::vector<double> x(count), expected(count);
        for (std::size_t i = 0; i < count; ++i)
            x[i] = 113.767 - double(count - 1 - i) * .029;
        for (const auto &line : lines)
            for (std::size_t i = 0; i < count; ++i) {
                const double v = (x[i] - line.position - shift) * factor;
                const double square = 1 + v * v;
                const double root = i + 1 == count && count % 2
                    ? platformSqrt(square)
                    : _mm_cvtsd_f64(_mm_sqrt_sd(_mm_setzero_pd(), _mm_set_sd(square)));
                const double inverse = 1 / root;
                expected[i] += line.intensity * ((inverse * inverse) * inverse);
            }
        check(take(profile->evaluate(x, shift, false, ctx())).intensity == expected);
    }
#endif
}
class Column final : public IProfileTemplate {
    const std::vector<double> y_;

  public:
    explicit Column(std::vector<double> y) : y_(std::move(y)) {}
    double fwhmDegrees() const noexcept override { return .2; }
    Result<ProfileSamples> evaluate(std::span<const double>, double, bool,
                                    const ExecutionContext &) const override {
        return Result<ProfileSamples>{ProfileSamples{y_, {}}};
    }
};
static ProfileFitInput linear(std::vector<double> y, std::vector<std::vector<double>> columns) {
    ProfileFitInput in;
    auto obs = std::make_shared<ProfileObservations>();
    obs->observed = std::move(y);
    obs->background.resize(obs->observed.size());
    obs->weights.assign(obs->observed.size(), 1);
    for (std::size_t i = 0; i < obs->observed.size(); ++i)
        obs->coordinates.push_back(double(i));
    in.observations = obs;
    for (auto &c : columns)
        in.phases.push_back({"column", std::make_shared<Column>(std::move(c)), 0, 0, 0});
    return in;
}
static ProfileFitResult fit(const ProfileFitInput &in) { return take(NnlsTrfProfileFitter{}.fit(in, ctx())); }
static void nnls() {
    auto one = fit(linear({2, 5, -1}, {{1, 2, 1}}));
    near(one.parameters.scales[0], 11. / 6);
    near(one.q, 30 - 121. / 6);
    auto negative = fit(linear({-1, -2}, {{1, 2}}));
    near(negative.parameters.scales[0], 0);
    near(negative.rwp_net, 1);
    check(negative.diagnostics.unidentifiable_shifts[0]);
    auto two = fit(linear({2, 3, 5}, {{1, 0, 1}, {0, 1, 1}}));
    near(two.parameters.scales[0], 2);
    near(two.parameters.scales[1], 3);
    check(two.q < 1e-28);
    auto active = fit(linear({1, -1}, {{1, 1}, {1, 0}}));
    near(active.parameters.scales[0], 0);
    near(active.parameters.scales[1], 1);
    near(active.q, 1);
    auto duplicate = fit(linear({2, 4}, {{1, 2}, {2, 4}, {0, 0}}));
    check(duplicate.diagnostics.rank_deficient && duplicate.diagnostics.rank == 1 &&
          duplicate.diagnostics.zero_templates[2]);
    near(duplicate.parameters.scales[0] + 2 * duplicate.parameters.scales[1], 2);
    // [1,1,1,1] and [1+d,1-d,1+d,1-d] have orthogonal mean/contrast
    // components. Their normalized smaller singular value is approximately
    // d/sqrt(2); these exact binary perturbations straddle eps*4*sigma_max.
    for (int exponent : {-24, -52}) {
        const double d = std::ldexp(1., exponent);
        auto correlated = fit(linear({5 + 2 * d, 5 - 2 * d, 5 + 2 * d, 5 - 2 * d},
                                     {{1, 1, 1, 1}, {1 + d, 1 - d, 1 + d, 1 - d}}));
        check(correlated.diagnostics.rank == (exponent == -24 ? 2 : 1));
        check(correlated.diagnostics.rank_deficient == (exponent == -52));
        check(correlated.diagnostics.kkt_violation < 1e-10);
        check(correlated.q < 1e-12);
    }
    auto empty = fit(linear({2, -3}, {}));
    near(empty.q, 13);
    near(empty.rwp_net, 1);
    // With negative observations both scales stay zero. Nevertheless the full
    // template rank changes from one to two when identical IL peaks separate.
    // Diagnose the final selected shifts, including the all-inactive case and
    // both orders of the last visited starts/difference perturbations.
    for (double final_shift : {0., .1}) {
        auto inactive = linear({-1, -1, -1, -1}, {});
        const auto profile = makeProfile({{1, 1.5, 1}});
        inactive.phases = {{"fixed", profile, 0, 0, 0}, {"moving", profile, -.5, .5, 0}};
        inactive.starts = {{0, .1 - final_shift}, {0, final_shift}};
        auto result = fit(inactive);
        check(result.parameters.scales == std::vector<double>({0, 0}));
        near(result.parameters.shifts[1], final_shift);
        check(result.diagnostics.rank == (final_shift == 0 ? 1 : 2));
        check(result.diagnostics.rank_deficient == (final_shift == 0));
        check(result.diagnostics.kkt_checked);
    }
    auto zero = linear({0, 0}, {{1, 2}});
    check(!NnlsTrfProfileFitter{}.fit(zero, ctx()));
    auto in = linear({2, 5, -1}, {{1e90, 2e90, 1e90}});
    auto scaled = fit(in);
    near(scaled.parameters.scales[0] * 1e90, 11. / 6);
    near(scaled.q, one.q);
    auto masked = linear({7, 11, 111}, {{1, 2, 3}});
    auto observations = std::make_shared<ProfileObservations>(*masked.observations);
    observations->background = {5, 7, 11};
    observations->weights = {1, 4, 0};
    masked.observations = observations;
    auto complete = fit(masked);
    near(complete.parameters.scales[0], 2);
    near(complete.q, 0);
    near(complete.calculated[2], 17);
    near(complete.residual[2], 94);
    std::stop_source stop;
    stop.request_stop();
    auto canceled = ctx();
    canceled.cancellation = stop.get_token();
    check(!NnlsTrfProfileFitter{}.fit(in, canceled));
}
static ProfileFitInput nonlinear(bool two = false, double shift = .073) {
    ProfileFitInput in;
    auto obs = std::make_shared<ProfileObservations>();
    for (int i = 0; i < 401; ++i) {
        const double x = 28 + i * .01 + .0003 * std::sin(double(i));
        obs->coordinates.push_back(x);
        obs->background.push_back(3 + .1 * (x - 30));
        obs->weights.push_back(.5 + i * .001);
        obs->observed.push_back(obs->background.back() + 17 * il(x, 30 + shift, .2) +
                                (two ? 11 * il(x, 30.34 - .039, .2) : 0));
    }
    in.observations = obs;
    in.phases.push_back({"a", makeProfile({{1, 30, 1}}), -.5, .5, 0});
    if (two)
        in.phases.push_back({"b", makeProfile({{2, 30.34, 1}}), -.5, .5, 0});
    return in;
}
static void trf() {
    auto in = nonlinear();
    auto r = fit(in);
    near(r.parameters.shifts[0], .073, 2e-8);
    near(r.parameters.scales[0], 17, 1e-7);
    check(r.rwp_net < 1e-7);
    check(r.diagnostics.difference_evaluations > 0 && r.diagnostics.kkt_violation < 1e-10);
    in.phases[0].profile = makeProfile({{1, 30, 1e30}});
    auto scaled = fit(in);
    near(scaled.parameters.shifts[0], r.parameters.shifts[0], 1e-9);
    near(scaled.parameters.scales[0] * 1e30, 17, 1e-7);
    in = nonlinear(true);
    r = fit(in);
    near(r.parameters.shifts[0], .073, 2e-7);
    near(r.parameters.shifts[1], -.039, 2e-7);
    check(r.rwp_net < 1e-6);
    std::reverse(in.phases.begin(), in.phases.end());
    auto reverse = fit(in);
    near(reverse.parameters.shifts[0], r.parameters.shifts[1], 1e-8);
    near(reverse.q, r.q, 1e-10);
    in = nonlinear(true);
    in.phases[0].lower_shift = in.phases[0].upper_shift = in.phases[0].initial_shift = .073;
    auto partlyFixed = fit(in);
    near(partlyFixed.parameters.shifts[0], .073, 0);
    near(partlyFixed.parameters.shifts[1], -.039, 2e-7);
    near(partlyFixed.parameters.scales[0], 17, 1e-5);
    near(partlyFixed.parameters.scales[1], 11, 1e-5);
    check(partlyFixed.rwp_net < 1e-6);
    in = nonlinear(false, .23);
    in.phases[0].upper_shift = .1;
    r = fit(in);
    near(r.parameters.shifts[0], .1, 2e-8);
    check(r.diagnostics.at_bound[0]);
    // At a fixed boundary the scale still has the independent analytic
    // least-squares value.
    double dot = 0, norm = 0;
    for (std::size_t i = 0; i < in.observations->coordinates.size(); ++i) {
        double p = il(in.observations->coordinates[i], 30.1, .2), w = in.observations->weights[i];
        dot += w * p * (in.observations->observed[i] - in.observations->background[i]);
        norm += w * p * p;
    }
    near(r.parameters.scales[0], dot / norm, 2e-6);
    in = nonlinear();
    in.options.maximum_evaluations = 1;
    r = fit(in);
    check(r.termination == ProfileFitTermination::budget);
    in.options.maximum_evaluations = 0;
    in.phases[0].initial_shift = -.5;
    r = fit(in);
    near(r.parameters.shifts[0], .073, 2e-7);
    in.fallback = ProfileFitParameters{{17}, {.073}};
    r = fit(in);
    check(r.rwp_net < 1e-12);
}
static void scaleReentry() {
    auto in = nonlinear(false, .2);
    auto observations = std::make_shared<ProfileObservations>(*in.observations);
    for (double &y : observations->observed)
        y -= 2; // Prescribed negative net tails, with unchanged fixed background.
    in.observations = observations;
    in.phases[0].initial_shift = -.5;
    const std::vector<double> bad{-.5}, good{.2};
    const auto independentProjection = [&](double shift) {
        double dot = 0, norm = 0;
        for (std::size_t i = 0; i < observations->coordinates.size(); ++i) {
            const double p = il(observations->coordinates[i], 30 + shift, .2);
            dot += observations->weights[i] * p * (observations->observed[i] - observations->background[i]);
            norm += observations->weights[i] * p * p;
        }
        return dot / norm;
    };
    check(independentProjection(bad[0]) < 0);
    check(independentProjection(good[0]) > 0);
    const auto inactive = take(NnlsTrfProfileFitter{}.evaluateAt(in, bad, ctx()));
    const auto active = take(NnlsTrfProfileFitter{}.evaluateAt(in, good, ctx()));
    check(inactive.parameters.scales[0] == 0);
    near(active.parameters.scales[0], independentProjection(good[0]));
    in.starts = {bad, good};
    const auto reentered = fit(in);
    check(reentered.diagnostics.starts == 2 && reentered.parameters.scales[0] > 0);
    check(reentered.q <= active.q && reentered.q < inactive.q);
    check(reentered.diagnostics.kkt_checked && !reentered.diagnostics.unidentifiable_shifts[0]);
}
static void interpolation() {
    for (double h : {.013, .2}) {
        ProfileExpansionInput input;
        input.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees;
        input.fwhm_degrees = h;
        input.interpolation_window = {{0, 50}};
        for (std::size_t j = 0; j < 24; ++j)
            input.lines.push_back({j, 1.27 + double(j) * 1.931, 1 + double(j % 5)});
        auto cached = take(IntermediateLorentzianExpander{}.prepare(input, ctx()));
        check(cached->usesApproximation());
        std::vector<double> x;
        for (int i = 0; i < 3001; ++i)
            x.push_back(.6 + double(i) * .0163);
        for (double shift : {-.37, 0., .291}) {
            auto values = take(cached->evaluate(x, shift, false, ctx()));
            auto exact = take(cached->evaluateExact(x, shift, false, ctx()));
            for (std::size_t i = 0; i < x.size(); ++i) {
                double independent = 0;
                for (const auto &line : input.lines)
                    independent += line.intensity * il(x[i], line.position + shift, h);
                check(std::abs(values.intensity[i] / independent - 1) < 2e-10);
                check(std::abs(exact.intensity[i] / independent - 1) < 2e-10);
                // Point evaluation must not depend on grouping into a vector.
                // This also compares scalar and coordinate-pair recurrences.
                check(values.intensity[i] ==
                      take(cached->evaluate(std::span<const double>(x).subspan(i, 1), shift, false, ctx()))
                          .intensity[0]);
            }
        }
        const std::vector<double> outside{-1, 51};
        check(take(cached->evaluate(outside, 0, false, ctx())).intensity ==
              take(cached->evaluateExact(outside, 0, false, ctx())).intensity);
    }
    auto in = nonlinear(true);
    const auto direct = fit(in);
    for (std::size_t j = 0; j < in.phases.size(); ++j) {
        ProfileExpansionInput input;
        input.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees;
        input.lines = {{j, 30 + .34 * double(j), 1}};
        input.fwhm_degrees = .2;
        input.interpolation_window = {{20, 40}};
        in.phases[j].profile = take(IntermediateLorentzianExpander{}.prepare(input, ctx()));
    }
    auto cached = fit(in);
    check(cached.diagnostics.interpolated_iterations && cached.diagnostics.exact_final_verification &&
          cached.diagnostics.kkt_checked);
    for (std::size_t j = 0; j < in.phases.size(); ++j)
        near(cached.parameters.shifts[j], direct.parameters.shifts[j], 1e-8);
    near(cached.q, direct.q, 1e-10);
}
static void interpolationRelativeCoordinates() {
    // Prescribed narrow profiles expose both rounded preparation nodes and
    // finite-difference shifts lost at nonzero physical coordinates.
    for (double center : {30., 120.}) {
        for (double width : {1e-9, 1e-11}) {
            ProfileExpansionInput input;
            input.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees;
            input.lines = {{0, center, 1}};
            input.fwhm_degrees = width;
            auto observations = std::make_shared<ProfileObservations>();
            const double alpha = 4 * (std::pow(2., 2. / 3) - 1);
            for (int i = -8; i <= 8; ++i) {
                const double x = center + double(i) * width / 4;
                const double u = (x - center - .05 * width) / width;
                observations->coordinates.push_back(x);
                observations->observed.push_back(std::pow(1 + alpha * u * u, -1.5));
                observations->background.push_back(0);
                observations->weights.push_back(1);
            }
            const auto &x = observations->coordinates;
            input.interpolation_window = {{x.front() - .2 * width, x.back() + .2 * width}};
            const auto cached = take(IntermediateLorentzianExpander{}.prepare(input, ctx()));
            check(cached->usesApproximation());
            for (double shift : {-.13 * width, 0., .13 * width}) {
                const auto values = take(cached->evaluate(x, shift, false, ctx()));
                for (std::size_t i = 0; i < x.size(); ++i) {
                    const long double u = (static_cast<long double>(x[i]) - center - shift) / width;
                    const double expected = static_cast<double>(
                        std::pow(1 + 4 * (std::pow(2.L, 2.L / 3) - 1) * u * u, -1.5L));
                    check(std::abs(values.intensity[i] / expected - 1) < 2e-10);
                    check(values.intensity[i] ==
                          take(cached->evaluate(std::span<const double>(x).subspan(i, 1), shift, false, ctx()))
                              .intensity[0]);
                }
            }
            const double step = std::cbrt(std::numeric_limits<double>::epsilon());
            const auto plus = take(cached->evaluate(x, step * width, false, ctx()));
            const auto minus = take(cached->evaluate(x, -step * width, false, ctx()));
            for (std::size_t i = 0; i < x.size(); ++i) {
                const double u = (x[i] - center) / width;
                const double expected = 3 * alpha * u * std::pow(1 + alpha * u * u, -2.5);
                near((plus.intensity[i] - minus.intensity[i]) / (2 * step), expected, 2e-8);
            }
            ProfileFitInput fitInput;
            fitInput.observations = observations;
            fitInput.phases.push_back({"prescribed", cached, -.2 * width, .2 * width, 0});
            const auto result = take(NnlsTrfProfileFitter{}.fit(fitInput, ctx()));
            check(result.q < 2e-8 && result.diagnostics.exact_final_verification);
            near(result.parameters.scales[0], 1, 2e-5);
            near(result.parameters.shifts[0], .05 * width, 2e-5);
            // Resolve the interior optimum within 0.002% of one FWHM. This is
            // approximately three default central-difference stencil steps;
            // a physically tiny but wholly missed shift must fail this check.
            near(result.parameters.shifts[0] / width, .05, 2e-5);
        }
    }
    // Optional cache use must preserve direct behavior when large physical
    // coordinates and shifts cancel, irrespective of the full IL's conditioning.
    ProfileExpansionInput input;
    input.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees;
    input.lines = {{0, 30, 1}, {1, 31, .3}};
    input.fwhm_degrees = .2;
    input.interpolation_window = {{30, 32}};
    auto cached = take(IntermediateLorentzianExpander{}.prepare(input, ctx()));
    for (double shift : {-1e18, 1e18}) {
        const std::vector<double> x{shift};
        check(take(cached->evaluate(x, shift, false, ctx())).intensity ==
              take(cached->evaluateExact(x, shift, false, ctx())).intensity);
    }
    // Translated line positions exceed the coordinate SIMD bound. Their full
    // far tails still agree with the independent expression.
    input.interpolation_window = {{1e12, 1e12 + 1}};
    cached = take(IntermediateLorentzianExpander{}.prepare(input, ctx()));
    const std::vector<double> x{1e12, 1e12 + .5, 1e12 + 1};
    const auto values = take(cached->evaluate(x, 0, false, ctx()));
    for (std::size_t i = 0; i < x.size(); ++i) {
        long double expected = 0;
        for (const auto &line : input.lines) {
            const long double u = (static_cast<long double>(x[i]) - line.position) / input.fwhm_degrees;
            expected += line.intensity * std::pow(1 + 4 * (std::pow(2.L, 2.L / 3) - 1) * u * u, -1.5L);
        }
        check(std::abs(values.intensity[i] / static_cast<double>(expected) - 1) < 2e-10);
    }
    // Internal cache nodes can hit an overflowing peak that is absent from
    // the finite observation samples. Preserve the original direct problem.
    for (double width : {1e-14, 1e-15}) {
        input.lines = {{0, 30, 1e308}, {1, 30, 1e308}};
        input.fwhm_degrees = width;
        const std::vector<double> observation{30 - 2 * width, 30 + 2 * width};
        input.interpolation_window = {{observation.front() - width, observation.back() + width}};
        cached = take(IntermediateLorentzianExpander{}.prepare(input, ctx()));
        check(!cached->usesApproximation());
        const auto direct = take(cached->evaluate(observation, 0, false, ctx()));
        for (std::size_t i = 0; i < observation.size(); ++i) {
            const long double u = (static_cast<long double>(observation[i]) - 30) / width;
            const double oneLine = static_cast<double>(1e308 *
                std::pow(1 + 4 * (std::pow(2.L, 2.L / 3) - 1) * u * u, -1.5L));
            check(std::isfinite(direct.intensity[i]));
            check(std::abs(direct.intensity[i] / (2 * oneLine) - 1) < 2e-10);
        }
        std::stop_source stop;
        stop.request_stop();
        auto canceled = ctx();
        canceled.cancellation = stop.get_token();
        const auto rejected = IntermediateLorentzianExpander{}.prepare(input, canceled);
        check(!rejected && rejected.error().code == ErrorCode::cancelled);
        auto invalidPolicy = ctx();
        invalidPolicy.numerical_policy = static_cast<NumericalPolicy>(255);
        const auto invalid = IntermediateLorentzianExpander{}.prepare(input, invalidPolicy);
        check(!invalid && invalid.error().code == ErrorCode::invalid_input);
    }
}

// A replacement fitter that implements only the original two numerical methods.
class ScanAdapter final : public IProfileFitter {
  public:
    int corrupt = 0;
    AlgorithmIdentity identity() const override { return {"test.scan-adapter", 1}; }
    Result<ProfileFitResult> fit(const ProfileFitInput &in, const ExecutionContext &context) const override {
        return NnlsTrfProfileFitter{}.fit(in, context);
    }
    Result<ProfileFitResult> evaluateAt(const ProfileFitInput &in, std::span<const double> shifts,
                                       const ExecutionContext &context) const override {
        auto result = NnlsTrfProfileFitter{}.evaluateAt(in, shifts, context);
        if (result) {
            auto &r = result.value();
            if (corrupt == 1) r.calculated.pop_back();
            if (corrupt == 2) r.residual[0] = std::numeric_limits<double>::infinity();
            if (corrupt == 3) r.parameters.scales[0] = -1;
            if (corrupt == 4) r.rwp_net += 1;
            if (corrupt == 5) r.q = -1;
        }
        return result;
    }
};
class CountedTemplate final : public IProfileTemplate {
    ProfileTemplate profile_;
    std::shared_ptr<std::atomic<std::size_t>> calls_;
  public:
    CountedTemplate(ProfileTemplate p, std::shared_ptr<std::atomic<std::size_t>> calls)
        : profile_(std::move(p)), calls_(std::move(calls)) {}
    double fwhmDegrees() const noexcept override { return profile_->fwhmDegrees(); }
    bool usesApproximation() const noexcept override { return profile_->usesApproximation(); }
    Result<ProfileSamples> evaluate(std::span<const double> x, double shift, bool derivative,
                                    const ExecutionContext &context) const override {
        ++*calls_;
        return profile_->evaluate(x, shift, derivative, context);
    }
};
static void batchedScan() {
    for (bool cache : {false, true}) {
        ProfileExpansionInput expansion;
        expansion.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees;
        expansion.fwhm_degrees = .2;
        expansion.lines = {{1, 30, 7}, {2, 30.18, 3}};
        if (cache) expansion.interpolation_window = {{28, 32}};
        const auto profile = take(IntermediateLorentzianExpander{}.prepare(expansion, ctx()));
        ProfileFitInput in;
        auto obs = std::make_shared<ProfileObservations>();
        for (int i = 0; i < 401; ++i) {
            double x = 28 + i * .01;
            obs->coordinates.push_back(x);
            obs->observed.push_back(5 + 11 * il(x, 30.061, .2) - .3);
            obs->background.push_back(5);
            obs->weights.push_back(i % 7 ? 1 + i * .01 : 0);
        }
        in.observations = obs;
        const auto calls = std::make_shared<std::atomic<std::size_t>>(0);
        // Duplicate and zero columns exercise deficient free sets and zero weights.
        in.phases = {{"selected", std::make_shared<CountedTemplate>(profile, calls), -.5, .5, .02},
                     {"duplicate", profile, -.5, .5, .02},
                     {"zero", makeProfile({}), -.5, .5, 0}};
        const std::vector<double> fixed{.02, .02, 0}, values{-.5, .19, 0, .19, .5};
        std::vector<double> expected;
        std::size_t residuals = 0, iterations = 0;
        for (double value : values) {
            auto shifts = fixed; shifts[1] = value;
            auto r = take(NnlsTrfProfileFitter{}.evaluateAt(in, shifts, ctx()));
            expected.push_back(r.q);
            residuals += r.diagnostics.residual_evaluations;
            iterations += r.diagnostics.nnls_iterations;
        }
        calls->store(0);
        auto batch = take(NnlsTrfProfileFitter{}.scanAt(in, fixed, 1, values, ctx()));
        check(batch.q == expected && batch.residual_evaluations == residuals && batch.nnls_iterations == iterations);
        check(calls->load() == 1); // The fixed selected column is sampled once.
        auto adapter = take(ScanAdapter{}.scanAt(in, fixed, 1, values, ctx()));
        check(adapter.q == batch.q && adapter.residual_evaluations == residuals && adapter.nnls_iterations == iterations);
        for (int corrupt = 1; corrupt <= 5; ++corrupt) {
            ScanAdapter replacement; replacement.corrupt = corrupt;
            auto rejected = replacement.scanAt(in, fixed, 1, values, ctx());
            check(!rejected && rejected.error().reason == "invalid_algorithm_output");
        }
        check(!NnlsTrfProfileFitter{}.scanAt(in, fixed, 3, values, ctx()));
        check(!NnlsTrfProfileFitter{}.scanAt(in, std::vector<double>{0}, 1, values, ctx()));
        check(!NnlsTrfProfileFitter{}.scanAt(in, fixed, 1, std::vector<double>{.51}, ctx()));
        std::stop_source stop; stop.request_stop(); auto canceled = ctx(); canceled.cancellation = stop.get_token();
        check(!NnlsTrfProfileFitter{}.scanAt(in, fixed, 1, values, canceled));
        check(!ScanAdapter{}.scanAt(in, fixed, 1, values, canceled));
        check(take(NnlsTrfProfileFitter{}.scanAt(in, fixed, 1, {}, ctx())).q.empty());
    }
}

void candidateRecallContracts();
void candidateRecallBackprojectionContracts();
void candidateRecallAnchoredContracts();
void candidateRecallProtectionContracts();
int main() try {
    candidateRecallContracts();
    candidateRecallBackprojectionContracts();
    candidateRecallAnchoredContracts();
    candidateRecallProtectionContracts();
    expansion();
    expansionRemainder();
    nnls();
    trf();
    scaleReentry();
    interpolation();
    interpolationRelativeCoordinates();
    batchedScan();
    std::cout << "PFSM core contracts passed\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
