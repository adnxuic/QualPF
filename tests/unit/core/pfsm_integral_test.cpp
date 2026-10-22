#include "qualpf/profile/pfsm_integral_screen.h"
#include "qualpf/profile/pfsm_relaxed_screen.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::profile;
namespace {
ExecutionContext context() { return {{}, NumericalPolicy::explicit_float64, {}}; }
void check(bool ok, std::source_location where = std::source_location::current()) {
    if (!ok) throw std::runtime_error("Integral prescreen contract line " + std::to_string(where.line()));
}
void near(double a, double b, double tolerance = 2e-12, std::source_location where = std::source_location::current()) {
    if (std::abs(a - b) > tolerance * std::max({1., std::abs(a), std::abs(b)}))
        throw std::runtime_error("Integral prescreen numerical contract line " + std::to_string(where.line()));
}
template<class T> T take(Result<T> r) {
    if (!r) throw std::runtime_error(r.error().field + ": " + r.error().message);
    return std::move(r).value();
}
std::shared_ptr<ProfileObservations> observed(std::vector<double> x, std::vector<double> y) {
    auto o = std::make_shared<ProfileObservations>();
    o->coordinates = std::move(x); o->observed = std::move(y);
    o->background.assign(o->coordinates.size(), 0); o->weights.assign(o->coordinates.size(), 1);
    return o;
}
double weighted(std::span<const double> p, std::span<const double> w) {
    return std::inner_product(p.begin(), p.end(), w.begin(), 0.);
}
void massContracts() {
    const double a = 1 / std::sqrt(4 * (std::cbrt(4.) - 1));
    near(take(pfsmIntegralMass(-a, a, 1)), std::sqrt(.5));
    near(take(pfsmIntegralMass(0, a, 1)), std::sqrt(.125));
    check(!pfsmIntegralMass(1, 0, 1) && !pfsmIntegralMass(0, 1, 0));
    for (double h : {1., 1e-3, 1e-8}) for (auto pair : {std::pair{-3., 2.}, std::pair{40., 41.}, std::pair{-41., -40.}}) {
        // Independent long-double Simpson quadrature after u=a*tan(theta):
        // unit IL mass is the integral of cos(theta)/2, not a CDF subtraction.
        const long double aa = h / std::sqrt(4.L * (std::pow(2.L, 2.L / 3) - 1));
        const long double lo = std::atan(pair.first * h / aa), hi = std::atan(pair.second * h / aa);
        constexpr int n = 4096;
        const long double step = (hi - lo) / n;
        long double total = 0;
        for (int i = 0; i <= n; ++i)
            total += (i == 0 || i == n ? 1 : i % 2 ? 4 : 2) * std::cos(lo + i * step) / 2;
        near(take(pfsmIntegralMass(pair.first * h, pair.second * h, h)), double(total * step / 3));
    }
    // Same-side far tails must not disappear through saturated F(b)-F(a).
    check(take(pfsmIntegralMass(100000., 100001., 1)) > 0);
}
void dataContracts() {
    auto unsupported = PfsmIntegralOptions{}; unsupported.fine_bins = 4096;
    check(!validatePfsmIntegralOptions(unsupported));
    auto o = observed({0, .2, .9, 1.7, 2}, {-3, 2, -1, 4, 8});
    o->weights = {2, 0, 3, 4, 1};
    const auto d = take(preparePfsmIntegralData(o, {}, .001, {}, context()));
    check(d->fine.weights.size() == 4 && d->coarse.weights.size() == 2);
    near(std::accumulate(d->fine.weights.begin(), d->fine.weights.end(), 0.), 10);
    near(weighted(d->fine.net, d->fine.weights), 15);
    near(weighted(d->coarse.net, d->coarse.weights), 15);
    check(d->regions.size() == 2 && d->regions[0].second == .1 && d->regions[1].first == .55);
    for (const auto &cell : d->overlaps) check(cell.sample != 1);
    auto gaps = observed({0, .1, .2, 3, 3.1, 3.2}, {1, 2, 3, 4, 5, 6});
    auto g = take(preparePfsmIntegralData(gaps, {}, .01, {}, context()));
    check(g->regions.size() == 2 && g->regions[0].second == .2 && g->regions[1].first == 3);
    std::vector<std::uint8_t> breaks{0, 1, 0, 0, 0, 0};
    check(!preparePfsmIntegralData(gaps, breaks, .01, {}, context())); // Isolated zero-length endpoint.
    std::stop_source stop; stop.request_stop(); auto c = context(); c.cancellation = stop.get_token();
    const auto canceled = preparePfsmIntegralData(o, {}, .01, {}, c);
    check(!canceled && canceled.error().code == ErrorCode::cancelled);
}
void narrowContracts() {
    std::vector<double> x(4097), y(4097, 1);
    for (std::size_t i = 0; i < x.size(); ++i) x[i] = double(i) / 4096;
    auto o = observed(x, y);
    std::size_t bytes = 0, overlaps = 0;
    for (double h : {1e-3, 1e-5, 1e-8}) {
        auto d = take(preparePfsmIntegralData(o, {}, h, {}, context()));
        check(d->fine.weights.size() == 2048 && d->coarse.weights.size() == 1024);
        if (bytes) check(d->buffer_bytes == bytes && d->overlaps.size() == overlaps);
        bytes = d->buffer_bytes; overlaps = d->overlaps.size();
        check(overlaps <= x.size() + d->fine.weights.size());
        // Constant sampling density in the interior: a complete narrow peak
        // integrates to (1-tail_epsilon)/dx regardless of its bin/cell position.
        for (double q : {.5, .5 + 1./8192, .5 + 1./4096}) {
            const std::vector<ProfileLine> lines{{0, q, 1}};
            const auto p = take(integratePfsmScreenTemplate(*d, lines, 0, context()));
            near(weighted(p.fine, d->fine.weights), 4096 * (1 - 1e-4));
            near(weighted(p.coarse, d->coarse.weights), 4096 * (1 - 1e-4));
        }
        const auto edge = take(integratePfsmScreenTemplate(*d, std::vector<ProfileLine>{{0, 0, 1}}, 0, context()));
        check(weighted(edge.fine, d->fine.weights) > 0);
        if (h == 1e-8) near(weighted(edge.fine, d->fine.weights), 4096 * (1 - 1e-4)); // Half mass, double endpoint density.
        const auto outside = take(integratePfsmScreenTemplate(*d, std::vector<ProfileLine>{{0, 3, 1}}, 0, context()));
        near(weighted(outside.fine, d->fine.weights), 0);
    }
}
ProfileExpansionInput reference(std::initializer_list<double> positions, double h = .001) {
    ProfileExpansionInput in; in.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees; in.fwhm_degrees = h;
    for (double x : positions) in.lines.push_back({in.lines.size(), x, 1});
    return in;
}
void evaluationContracts() {
    auto o = observed({0, 1}, {1, 2});
    auto d = take(preparePfsmIntegralData(o, {}, .001, {}, context()));
    std::vector<ProfileExpansionInput> selected{reference({.25, .75})};
    const auto round = take(preparePfsmIntegralRound(d, selected, std::vector<double>{0}, std::vector<double>{-.5, .5}, {}, context()));
    near(round.coarse_baseline_q, 0); near(round.fine_baseline_q, .5);
    auto e = take(evaluatePfsmIntegralCandidate(round, reference({.75}), 0, 0, {}, context()));
    near(*e.fine_gain, .5); near(*e.gain, 0);
    check(e.integral_complete && e.uncertainty.empty() && e.original.uncertain); // Weak support does not veto valid joint NNLS.
    check(e.coarse_evaluations == 1 && e.fine_evaluations == 1);
    ProfileFitOptions limited; limited.maximum_nnls_iterations = 1;
    auto failed = take(evaluatePfsmIntegralCandidate(round, reference({.75}), 0, 0, limited, context()));
    check(!failed.integral_complete && failed.uncertainty == std::vector<std::string>{"integral_numerical_failure"});
    std::vector<PfsmScreenEvidence> localFailure{failed, e};
    localFailure[0].original_index = 0; localFailure[1].original_index = 1;
    const auto protectedSelection = take(selectPfsmIntegralCandidates(localFailure, {}, context()));
    check(!protectedSelection.full_fallback && localFailure[0].retained && localFailure[1].integral_complete);
    auto outside = take(evaluatePfsmIntegralCandidate(round, reference({10}), -.1, .1, {}, context()));
    check(outside.integral_complete && outside.uncertainty == std::vector<std::string>{"integral_template_insufficient"});
    const auto narrow = take(evaluatePfsmIntegralCandidate(round, reference({.75}), -.123, .151, {}, context()));
    check(narrow.coarse_evaluations == 66 && narrow.fine_evaluations <= 32 && narrow.scan_evaluations <= 98);
    std::stop_source stop; stop.request_stop(); auto c = context(); c.cancellation = stop.get_token();
    auto canceled = evaluatePfsmIntegralCandidate(round, reference({.75}), 0, 0, {}, c);
    check(!canceled && canceled.error().code == ErrorCode::cancelled);
    std::vector<PfsmScreenEvidence> rows(300);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        rows[i].original_index = i; rows[i].integral_complete = true;
        rows[i].normalized_fine_gain = double(300 - i) / 300;
        rows[i].normalized_gain = double(i) / 300;
    }
    rows[140].uncertainty = {"integral_numerical_failure"}; rows[140].integral_complete = false;
    auto selection = take(selectPfsmIntegralCandidates(rows, {}, context()));
    check(!selection.full_fallback && selection.indices.size() == 81 && rows[140].retained);
    check(rows[0].main_selected && rows[299].coarse_selected && !rows[150].retained);
    for (auto &row : rows) row.normalized_fine_gain = 0;
    selection = take(selectPfsmIntegralCandidates(rows, {}, context()));
    check(!selection.full_fallback && selection.indices.size() == 300); // No union cap or low-compression fallback.
    std::vector<double> x(257), y(257);
    for (std::size_t i = 0; i < x.size(); ++i) { x[i] = double(i) / 256; y[i] = 1 + std::sin(12 * x[i]); }
    auto input = observed(x, y);
    std::size_t bytes = 0;
    for (double h : {1e-3, 1e-5, 1e-8}) {
        auto data = take(preparePfsmIntegralData(input, {}, h, {}, context()));
        auto r = take(preparePfsmIntegralRound(data, {}, {}, y, {}, context()));
        auto score = take(evaluatePfsmIntegralCandidate(r, reference({.3, .6}, h), -.2, .2, {}, context()));
        check(score.integral_complete && score.coarse_evaluations <= 66 && score.fine_evaluations <= 32);
        if (bytes) check(score.buffer_bytes == bytes);
        bytes = score.buffer_bytes;
    }
}
}
void integralContracts() {
    massContracts(); dataContracts(); narrowContracts(); evaluationContracts();
    const auto reference = [](std::initializer_list<std::pair<double,double>> lines,double h) {
        ProfileExpansionInput input; input.coordinates=ProfileExpansionInput::Coordinates::two_theta_degrees;
        input.fwhm_degrees=h;
        for (const auto &[q,s] : lines) input.lines.push_back({input.lines.size(),q,s});
        return input;
    };
    std::vector<double> x(251),y(251),w(251);
    const double h=.25;
    for (std::size_t i=0;i<x.size();++i) {
        x[i]=29+double(i)*4/250;
        const auto il=[&](double q) { return std::pow(1+4*(std::pow(2.,2./3)-1)*std::pow((x[i]-q)/h,2),-1.5); };
        y[i]=il(30.03)+.8*(il(31.3)+.4*il(32.3))+.6*(.8*il(31.22)+.5*il(32.26));
        w[i]=i%13 ? 1+.2*std::sin(x[i]) : 0;
    }
    auto observation=observed(x,y); observation->weights=w;
    PfsmScreenEvidence seed; seed.integral_complete=true; seed.fine_shift=.1; seed.original.shift=.12;
    for (double width : {.25,.025,.0025}) {
        auto data=take(preparePfsmIntegralData(observation,{},width,{},context()));
        std::vector<ProfileExpansionInput> selected{reference({{30,1}},width),reference({{31,1},{32,.4}},width)};
        auto candidate=reference({{31.12,.8},{32.16,.5}},width);
        const auto round=take(preparePfsmRelaxedRound(data,selected,std::vector<double>{.03,-.02},-.5,.5,{}, {},context()));
        auto evidence=take(evaluatePfsmRelaxedCandidate(round,candidate,seed,context()));
        check(evidence.complete && evidence.uncertainty.empty() && evidence.evaluations<=205 && evidence.cache_columns<=32);
        check(round.bank_columns<=514 && round.data->fine.weights.size()<=2048);
        if (width==h) check(evidence.best.q<evidence.fixed_q*.1);
        for (std::size_t j=0;j<2;++j)
            check(evidence.best.shifts[j]>=std::max(-.5,round.old_shifts[j]-round.radius) &&
                  evidence.best.shifts[j]<=std::min(.5,round.old_shifts[j]+round.radius));
        auto invalid=PfsmRelaxedOptions{}; invalid.maximum_evaluations=204; check(!validatePfsmRelaxedOptions(invalid));
        auto empty=reference({},width);
        auto missing=take(evaluatePfsmRelaxedCandidate(round,empty,seed,context()));
        check(missing.complete && missing.uncertainty=="no_candidate_contribution");
        std::vector<PfsmScreenEvidence> protectedRows(1); protectedRows[0].relaxed=missing;
        check(take(selectPfsmRelaxedCandidates(protectedRows,{},context())).indices==std::vector<std::size_t>{0});
        std::stop_source stop;stop.request_stop();auto canceled=context();canceled.cancellation=stop.get_token();
        auto result=evaluatePfsmRelaxedCandidate(round,candidate,seed,canceled);
        check(!result && result.error().code==ErrorCode::cancelled);
    }
}
