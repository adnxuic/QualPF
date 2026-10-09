#include "qualpf/preprocessing/legacy.h"
#include <iomanip>
#include <iostream>

using namespace qualpf;
using namespace qualpf::preprocessing;
namespace {
int error(const Error &e) {
    std::cout << "ERROR " << int(e.code) << ' ' << e.field << '\n';
    return 0;
}
void peak(const Peak &p, bool delta) {
    std::cout << p.two_theta.value << ' ' << p.d_spacing->value << ' ' << p.intensity << ' ' << p.fwhm->value
              << ' ' << *p.integrated_intensity;
    if (delta)
        std::cout << ' ' << p.delta_d->value;
    std::cout << '\n';
}
} // namespace
int main() {
    int op, n;
    double wave, delta;
    if (!(std::cin >> op >> n >> wave >> delta) || n < 0 || n > 1000000)
        return 1;
    Pattern pattern;
    pattern.id = {1};
    for (int i = 0; i < n; ++i) {
        double x, y;
        if (!(std::cin >> x >> y))
            return 1;
        pattern.two_theta.push_back(TwoThetaDegrees{x});
        pattern.observed.push_back(y);
    }
    std::cout << std::setprecision(17);
    if (op == 0) {
        int method;
        SmoothingOptions o;
        if (!(std::cin >> method >> o.half_window >> o.polynomial_degree >> o.derivative))
            return 1;
        o.method = SmoothingMethod(method);
        auto output = smooth(pattern.observed, o);
        if (!output)
            return error(output.error());
        std::cout << n << '\n';
        for (auto v : output.value())
            std::cout << v << '\n';
    } else if (op == 1) {
        int method, automatic;
        double lo, hi;
        BackgroundOptions o;
        if (!(std::cin >> method >> automatic >> o.coefficients >> o.iterations >> o.half_window >> lo >> hi))
            return 1;
        if (float(lo) != float(pattern.two_theta.front().value) ||
            float(hi) != float(pattern.two_theta.back().value))
            return error({ErrorCode::unsupported, "background.partial_range",
                          "Partial Brueckner output is undefined"});
        o.method = BackgroundMethod(method);
        o.automatic = automatic != 0;
        auto output = background(pattern, o, WavelengthAngstrom{wave});
        if (!output)
            return error(output.error());
        const auto &r = output.value();
        std::cout << n << ' ' << r.points.size() << ' ' << r.coefficients.size() << '\n' << r.origin << '\n';
        for (auto v : r.values)
            std::cout << v << '\n';
        for (auto p : r.points)
            std::cout << p.x << ' ' << p.y << '\n';
        for (auto v : r.coefficients)
            std::cout << v << '\n';
    } else if (op == 4) {
        int first,last; BackgroundFilter o;
        if (!(std::cin >> first >> last >> o.iterations >> o.half_window) || first < 0 || last < first || last >= n) return 1;
        o.minimum = pattern.two_theta[first].value; o.maximum = pattern.two_theta[last].value;
        pattern.background = SampledSignal{pattern.observed,SampleWindow{0}};
        auto result = filter_background(pattern,o);
        if (!result) return error(result.error());
        std::cout << n << '\n';
        for (auto v : result.value().values) std::cout << v << '\n';
    } else if (op == 5) {
        int method,automatic,count; BackgroundOptions o;
        if (!(std::cin >> method >> automatic >> o.coefficients >> count) || count < 0 || count > n) return 1;
        o.method = BackgroundMethod(method); o.automatic = automatic != 0;
        std::vector<BackgroundPoint> points(count);
        for (auto &p : points) if (!(std::cin >> p.x >> p.y)) return 1;
        auto output = background_from_points(pattern,o,WavelengthAngstrom{wave},points);
        if (!output) return error(output.error());
        const auto &r=output.value();
        std::cout << n << ' ' << r.points.size() << ' ' << r.coefficients.size() << '\n' << r.origin << '\n';
        for (auto v : r.values) std::cout << v << '\n';
        for (auto p : r.points) std::cout << p.x << ' ' << p.y << '\n';
        for (auto v : r.coefficients) std::cout << v << '\n';
    } else if (op == 3) {
        int format;
        if (!(std::cin >> format))
            return 1;
        PeakSet set;
        set.dataset = {1};
        for (int i = 0; i < n; ++i) {
            Peak p;
            p.id = {std::uint64_t(i + 1)};
            p.two_theta = TwoThetaDegrees{float(pattern.two_theta[i].value)};
            auto d = legacy_d(p.two_theta, WavelengthAngstrom{wave});
            if (!d)
                return error(d.error());
            p.d_spacing = d.value();
            p.intensity = float(pattern.observed[i]);
            p.fwhm = AngularWidthDegrees{double(.2f)};
            set.peaks.push_back(p);
        }
        auto output = export_peak_text(set, PeakTextFormat(format));
        if (!output)
            return error(output.error());
        std::cout << output.value();
    } else if (op == 2 || op == 6) {
        PeakOptions o;
        int automatic;
        if (!(std::cin >> o.threshold_percent >> o.slope >> o.search_range >> o.half_window >>
              o.polynomial_degree >> o.maximum_peaks >> o.minimum_peaks >> automatic))
            return 1;
        o.automatic_window = automatic != 0;
        if(op==6) {
            double first,last;if(!(std::cin >> first >> last)) return 1;
            o.detection_range=AngularRange{TwoThetaDegrees{first},TwoThetaDegrees{last}};
        }
        auto output = find_peaks(pattern, o, WavelengthAngstrom{wave}, AngularWidthDegrees{delta});
        if (!output)
            return error(output.error());
        const auto &r = output.value();
        std::cout << r.peaks.peaks.size() << ' ' << r.candidates.peaks.size() << ' ' << r.applied.half_window
                  << ' ' << r.applied.polynomial_degree << '\n';
        std::cout << r.applied.search_range << ' ' << r.applied.threshold_percent << ' '
                  << r.automatic_tolerance << '\n';
        for (const auto &p : r.peaks.peaks)
            peak(p, true);
        for (const auto &p : r.candidates.peaks)
            peak(p, false);
    } else
        return 1;
}
