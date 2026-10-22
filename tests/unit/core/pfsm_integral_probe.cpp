// Independent-oracle text protocol; never writes expected values.
#include "qualpf/profile/pfsm_integral_screen.h"
#include <iomanip>
#include <iostream>
using namespace qualpf;
using namespace qualpf::profile;
template<class T> T take(Result<T> r) { if (!r) throw std::runtime_error(r.error().message); return std::move(r).value(); }
int main() try {
    const ExecutionContext c{{}, NumericalPolicy::explicit_float64, {}};
    std::size_t n, m; double h;
    if (!(std::cin >> n >> m >> h) || n > 100000 || !m || m > 64) return 2;
    auto o = std::make_shared<ProfileObservations>();
    for (std::size_t i = 0; i < n; ++i) {
        double x,y,w; if (!(std::cin >> x >> y >> w)) return 2;
        o->coordinates.push_back(x); o->observed.push_back(y); o->weights.push_back(w); o->background.push_back(0);
    }
    std::vector<ProfileExpansionInput> refs; std::vector<double> shifts;
    for (std::size_t j = 0; j < m; ++j) {
        double shift; std::size_t count; if (!(std::cin >> shift >> count) || count > 10000) return 2;
        ProfileExpansionInput in; in.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees; in.fwhm_degrees = h;
        for (std::size_t k = 0; k < count; ++k) {
            double q,i; if (!(std::cin >> q >> i)) return 2; in.lines.push_back({k,q,i});
        }
        refs.push_back(std::move(in)); shifts.push_back(shift);
    }
    auto last = refs.back(); refs.pop_back(); double shift = shifts.back(); shifts.pop_back();
    auto d = take(preparePfsmIntegralData(o, {}, h, {}, c));
    auto r = take(preparePfsmIntegralRound(d, refs, shifts, o->observed, {}, c));
    auto e = take(evaluatePfsmIntegralCandidate(r, last, shift, shift, {}, c));
    if (!e.integral_complete || !e.uncertainty.empty()) return 1;
    std::cout << std::setprecision(17) << r.coarse_baseline_q << ' ' << r.fine_baseline_q << ' '
        << *e.gain << ' ' << *e.fine_gain << ' ' << d->coarse.denominator << ' ' << d->fine.denominator << '\n';
    return 0;
} catch (const std::exception &e) { std::cerr << e.what(); return 1; }
