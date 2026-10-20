#include "legacy_internal.h"
#include "analysis_signal.h"
#include "legacy_reduction.h"

namespace qualpf::preprocessing {
namespace {
using namespace detail;
using Points = std::vector<BackgroundPoint>;
int minimum(const Vector &y, int a, int b) {
    return int(std::min_element(y.begin() + a, y.begin() + b + 1) - y.begin());
}
void insert_points(Points &points, int first, int last, const Vector &x, const Vector &y) {
    Points added;
    float range = std::abs(points[last].y - points[first].y);
    for (int i = first; i < last; ++i) {
        int count = std::max(1, int(std::round(std::min((points[i + 1].x - points[i].x) / .5f, 5.f))));
        float dx = (points[i + 1].x - points[i].x) / float(count + 1);
        float dy = (points[i + 1].y - points[i].y) / float(count + 1);
        for (int j = 1; j <= count; ++j) {
            BackgroundPoint p{points[i].x + j * dx, points[i].y + j * dy};
            auto at = nearest(x, p.x);
            float diff = p.y - y[at];
            if (diff > 0 || y[at] <= std::max(points[first].y, points[last].y) ||
                std::abs(diff) < .4f * range)
                p.y = y[at];
            else
                continue;
            added.push_back(p);
        }
    }
    points.insert(points.end(), added.begin(), added.end());
    std::stable_sort(points.begin(), points.end(), [](auto a, auto b) { return a.x < b.x; });
}
Result<Points> select_points(const Vector &x, const Vector &y, float wave) {
    Vector steps;
    for (std::size_t i = 1; i < x.size(); ++i)
        steps.push_back(x[i] - x[i - 1]);
    float step = sum(steps) / float(steps.size());
    float size = 2.f * std::sqrt((64.f - 1.54056f * 1.54056f) / (64.f - wave * wave)) * (wave / 1.54056f);
    if (!std::isfinite(size) || size / step > float(std::numeric_limits<int>::max()))
        return Result<Points>{invalid("background.window", "Unrepresentable background window")};
    int window = int(std::round(size / step));
    if (window < 1)
        return Result<Points>{invalid("background.window", "Legacy zero window is undefined")};
    Points points;
    int end = -1, previous = 0, n = int(x.size());
    while (end < n - 1) {
        int begin = end + 1;
        end = std::min(begin + window - 1, n - 1);
        int pos = minimum(y, begin, end);
        if (pos == begin || pos == end) {
            begin = std::max(pos - window / 2, previous + 1);
            end = std::min(begin + window - 1, n - 1);
            if (begin > end)
                return Result<Points>{numerical("background.points", "Undefined empty minimum window")};
            pos = minimum(y, begin, end);
        }
        points.push_back({x[pos], y[pos]});
        previous = pos;
    }
    if (points.front().x != x.front())
        points.insert(points.begin(), {x.front(), y.front()});
    if (points.back().x != x.back())
        points.push_back({x.back(), y.back()});
    if (points.size() >= 3)
        insert_points(points, 0, 2, x, y);
    if (points.size() >= 9)
        insert_points(points, int(points.size()) - 4, int(points.size()) - 1, x, y);
    return Result<Points>{std::move(points)};
}
Vector basis(float x, int count, BackgroundMethod method, float lo, float hi, float origin) {
    Vector v(count, 1.f);
    if (method == BackgroundMethod::polynomial) {
        for (int j = 1; j < count; ++j)
            v[j] = power(x / origin - 1.f, j);
    } else if (method == BackgroundMethod::cosine) {
        const float dtor = 1.f / (180.f / (4.f * std::atan(1.f)));
        // Keep the argument's legacy binary32 rounding. Evaluating cos in
        // binary64 before narrowing removes the CRT cosf one-ulp discrepancy
        // that is amplified by cancellation in the Fourier sum.
        for (int j = 0; j < count; ++j)
            v[j] = float(std::cos(double(float(j) * x * dtor)));
    } else {
        float t = 2.f * (x - lo) / (hi - lo) - 1.f;
        v[0] = .5f;
        if (count > 1)
            v[1] = t;
        float previous = 1, current = t;
        for (int j = 2; j < count; ++j) {
            float next = 2.f * t * current - previous;
            v[j] = next;
            previous = current;
            current = next;
        }
    }
    return v;
}
Result<Vector> fit(const Points &points, int count, BackgroundMethod method, float lo, float hi,
                   float origin) {
    std::vector<double> a(count * count), rhs(count), diag(count), solution(count);
    std::vector<Vector> rows;
    for (auto p : points)
        rows.push_back(basis(p.x, count, method, lo, hi, origin));
    for (int j = 0; j < count; ++j) {
        rhs[j] = contiguous_matmul_dot<double>(
            points.size(), [&](auto k) { return double(rows[k][j]); },
            [&](auto k) { return double(points[k].y); });
        for (int i = 0; i < count; ++i)
            a[j * count + i] = contiguous_matmul_dot<double>(
                points.size(), [&](auto k) { return double(rows[k][j]); },
                [&](auto k) { return double(rows[k][i]); });
    }
    for (int i = 0; i < count; ++i) {
        double dot = 0;
        for (int k = 0; k < i; ++k)
            dot += a[i * count + k] * a[i * count + k];
        double value = a[i * count + i] - dot;
        if (!(value > 0) || !std::isfinite(value))
            return Result<Vector>{numerical(
                "background.fit", "Legacy nonpositive Cholesky pivot leaves coefficients undefined")};
        diag[i] = std::sqrt(value);
        for (int j = i + 1; j < count; ++j) {
            dot = 0;
            for (int k = 0; k < i; ++k)
                dot = cholesky_multiply_add(a[j * count + k], a[i * count + k], dot);
            a[j * count + i] = (a[i * count + j] - dot) / diag[i];
        }
    }
    for (int i = 0; i < count; ++i) {
        double dot = 0;
        for (int k = 0; k < i; ++k)
            dot += a[i * count + k] * solution[k];
        solution[i] = (rhs[i] - dot) / diag[i];
    }
    for (int i = count - 1; i >= 0; --i) {
        double dot = 0;
        for (int k = i + 1; k < count; ++k)
            dot += a[k * count + i] * solution[k];
        solution[i] = (solution[i] - dot) / diag[i];
    }
    Vector result;
    for (double v : solution)
        result.push_back(float(v));
    return Result<Vector>{std::move(result)};
}
Vector evaluate(const Vector &x, const Vector &c, BackgroundMethod method, float origin) {
    Vector out;
    for (float t : x) {
        float value = 0;
        if (method == BackgroundMethod::chebyshev) {
            float u = 2.f * (t - x.front()) / (x.back() - x.front()) - 1.f;
            float d = 0, dd = 0;
            for (int j = int(c.size()) - 1; j >= 1; --j) {
                float old = d;
                d = 2.f * u * d - dd + c[j];
                dd = old;
            }
            value = u * d - dd + .5f * c[0];
        } else {
            auto v = basis(t, int(c.size()), method, x.front(), x.back(), origin);
            for (std::size_t j = 0; j < c.size(); ++j)
                value += c[j] * v[j];
        }
        out.push_back(value);
    }
    return out;
}
Vector spline(const Vector &x, const Points &points) {
    const int n = int(points.size());
    Vector a(n), b(n, 1), c(n), r(n), u(n), gamma(n), slope(n);
    for (int i = 0; i < n - 1; ++i) {
        c[i] = points[i + 1].x - points[i].x;
        slope[i] = 6.f * ((points[i + 1].y - points[i].y) / c[i]);
    }
    for (int i = 1; i < n - 1; ++i) {
        a[i] = c[i - 1];
        b[i] = 2.f * (c[i] + a[i]);
        r[i] = slope[i] - slope[i - 1];
    }
    float h = points[1].x - points[0].x;
    r[0] = (3.f / h) * ((points[1].y - points[0].y) / h);
    c[0] = .5f;
    h = points[n - 1].x - points[n - 2].x;
    r[n - 1] = (-3.f / h) * ((points[n - 1].y - points[n - 2].y) / h);
    a[n - 1] = .5f;
    float beta = b[0];
    u[0] = r[0] / beta;
    for (int i = 1; i < n; ++i) {
        gamma[i] = c[i - 1] / beta;
        beta = b[i] - a[i] * gamma[i];
        u[i] = (r[i] - a[i] * u[i - 1]) / beta;
    }
    for (int i = n - 2; i >= 0; --i)
        u[i] -= gamma[i + 1] * u[i + 1];
    Vector out;
    for (float t : x) {
        int lo =
            int(std::upper_bound(points.begin(), points.end(), t, [](float v, auto p) { return v < p.x; }) -
                points.begin()) -
            1;
        lo = std::clamp(lo, 0, n - 2);
        int hi = lo + 1;
        h = points[hi].x - points[lo].x;
        float aa = (points[hi].x - t) / h, bb = (t - points[lo].x) / h;
        out.push_back(aa * points[lo].y + bb * points[hi].y +
                      ((power(aa, 3) - aa) * u[lo] + (power(bb, 3) - bb) * u[hi]) * (h * h) / 6.f);
    }
    return out;
}
double power64(double x, int n) {
    double v = 1;
    while (n) {
        if (n & 1)
            v *= x;
        n >>= 1;
        if (n)
            x *= x;
    }
    return v;
}
Vector bezier(const Vector &x, const Points &points) {
    const int degree = int(points.size()) - 1, count = int(points.size()) * 50;
    std::vector<double> binomial(degree + 1, 1), bx(count), by(count);
    for (int k = 1; k <= degree / 2; ++k)
        binomial[k] = binomial[k - 1] * (degree - k + 1) / k;
    for (int k = degree / 2 + 1; k < degree; ++k)
        binomial[k] = binomial[degree - k];
    Vector locations(count);
    for (int i = 0; i < count; ++i) {
        double t = double(i) / double(count - 1);
        for (int k = 0; k <= degree; ++k) {
            double weight = binomial[k] * power64(t, k) * power64(1 - t, degree - k);
            bx[i] += double(points[k].x) * weight;
            by[i] += double(points[k].y) * weight;
        }
        locations[i] = float(bx[i]);
    }
    Vector out;
    for (float t : x) {
        int lo = int(std::upper_bound(locations.begin(), locations.end(), t) - locations.begin()) - 1;
        lo = std::clamp(lo, 0, count - 2);
        double h = bx[lo + 1] - bx[lo];
        out.push_back(float((bx[lo + 1] - t) / h * by[lo] + (t - bx[lo]) / h * by[lo + 1]));
    }
    return out;
}
} // namespace

static Result<BackgroundResult> compute_background(const Pattern &p, const BackgroundOptions &options,
                                                   WavelengthAngstrom wavelength, const Points *manual) {
    auto gx = grid(p);
    auto gy = narrow(p.observed);
    if (!gx)
        return Result<BackgroundResult>{gx.error()};
    if (!gy)
        return Result<BackgroundResult>{gy.error()};
    if (!std::isfinite(wavelength.value) || wavelength.value <= 0 || wavelength.value >= 8 ||
        (int(options.method) <= 2 && (options.coefficients < 1 || options.coefficients > 36)) ||
        options.iterations < 0 || options.iterations > 10000 || options.half_window < 0 ||
        options.half_window > 100000 || int(options.method) < 0 || int(options.method) > 6)
        return Result<BackgroundResult>{invalid("background.options", "Invalid background parameters")};
    const auto &x = gx.value();
    const auto &y = gy.value();
    if (options.method == BackgroundMethod::bruckner && options.bruckner_range &&
        (float(options.bruckner_range->minimum.value) != x.front() ||
         float(options.bruckner_range->maximum.value) != x.back()))
        return Result<BackgroundResult>{Error{ErrorCode::undefined_preprocessing, "background.range",
                                              "Legacy partial Brueckner leaves output outside the interval "
                                              "unassigned",
                                              "partial_brueckner_unassigned_output"}};
    auto selected = manual ? Result<Points>{*manual} : select_points(x, y, float(wavelength.value));
    if (!selected)
        return Result<BackgroundResult>{selected.error()};
    BackgroundResult result;
    result.points = std::move(selected).value();
    float origin = (x.front() + x.back()) / 2.f;
    result.origin = origin;
    Vector out;
    if (int(options.method) <= 2) {
        int maximum = options.automatic ? std::min(int(std::round(float(result.points.size()) * .70f)),
                                                   options.method == BackgroundMethod::chebyshev ? 20 : 8)
                                        : options.coefficients;
        Vector targets, observed;
        std::vector<bool> used(x.size());
        for (auto point : result.points) {
            int at = int(nearest(x, point.x));
            for (int j = at - 3; j <= at + 3; ++j) {
                if (j < 0 || j >= int(x.size()))
                    break; // EXIT, not CYCLE, in the legacy routine.
                used[j] = true;
            }
        }
        for (std::size_t i = 0; i < x.size(); ++i)
            if (used[i]) {
                targets.push_back(x[i]);
                observed.push_back(y[i]);
            }
        float best = std::numeric_limits<float>::max();
        Vector chosen;
        for (int count = options.automatic ? 1 : maximum; count <= maximum; ++count) {
            auto c = fit(result.points, count, options.method, x.front(), x.back(), origin);
            if (!c)
                return Result<BackgroundResult>{c.error()};
            if (!options.automatic) {
                chosen = std::move(c).value();
                break;
            }
            auto predicted = evaluate(targets, c.value(), options.method, origin);
            float error = 0;
            for (std::size_t i = 0; i < targets.size(); ++i)
                error += std::abs(observed[i] - predicted[i]);
            float rp = (error / sum(observed)) * 100.f;
            if (rp < best) {
                best = rp;
                chosen = std::move(c).value();
            }
        }
        if (chosen.empty())
            return Result<BackgroundResult>{
                numerical("background.auto", "No defined minimum residual/coefficients")};
        out = evaluate(x, chosen, options.method, origin);
        result.coefficients = widen(chosen);
    } else if (options.method == BackgroundMethod::spline)
        out = spline(x, result.points);
    else if (options.method == BackgroundMethod::bezier)
        out = bezier(x, result.points);
    else if (options.method == BackgroundMethod::bruckner) {
        out = y;
        float limit = (*std::max_element(y.begin(), y.end()) + *std::min_element(y.begin(), y.end())) / 2.f;
        for (auto &v : out)
            v = std::min(v, limit);
        for (int i = 0; i < options.iterations; ++i) {
            auto filtered = average(out, options.half_window);
            for (std::size_t j = 0; j < out.size(); ++j)
                out[j] = std::min(out[j], filtered[j]);
        }
    } else
        out.assign(x.size(), 0);
    if (!std::all_of(out.begin(), out.end(), [](float v) { return std::isfinite(v); }))
        return Result<BackgroundResult>{numerical("background", "Nonfinite background")};
    result.values = widen(out);
    return Result<BackgroundResult>{std::move(result)};
}

Result<BackgroundResult> background(const Pattern &p, const BackgroundOptions &options,
                                    WavelengthAngstrom wavelength) {
    return compute_background(p, options, wavelength, nullptr);
}
Result<BackgroundResult> background_from_points(const Pattern &p, const BackgroundOptions &options,
                                                WavelengthAngstrom wavelength,
                                                std::span<const BackgroundPoint> points) {
    if (points.size() < 2)
        return Result<BackgroundResult>{
            invalid("background.points", "At least two ordered finite points required")};
    for (std::size_t i = 0; i < points.size(); ++i)
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y) ||
            (i && points[i].x < points[i - 1].x))
            return Result<BackgroundResult>{
                invalid("background.points", "At least two ordered finite points required")};
    Points owned(points.begin(), points.end());
    return compute_background(p, options, wavelength,
                              options.method == BackgroundMethod::spline ||
                                      options.method == BackgroundMethod::bezier ||
                                      options.method == BackgroundMethod::bruckner
                                  ? nullptr
                                  : &owned);
}
Result<SampledSignal> filter_background(const Pattern &p, const BackgroundFilter &options) {
    auto x = grid(p);
    if (!x)
        return Result<SampledSignal>{x.error()};
    if (!p.background || !p.background->window || p.background->values.empty() ||
        p.background->window->first > x.value().size() ||
        p.background->values.size() > x.value().size() - p.background->window->first)
        return Result<SampledSignal>{Error{ErrorCode::missing_data, "background.alignment",
                                           "An explicitly aligned existing background is required",
                                           "background_alignment_unknown"}};
    if (!std::isfinite(options.minimum) || !std::isfinite(options.maximum) ||
        options.minimum > options.maximum || options.iterations < 0 || options.iterations > 10000 ||
        options.half_window < 1 || options.half_window > 100000)
        return Result<SampledSignal>{invalid("background.filter", "Invalid filter parameters")};
    auto values = narrow(p.background->values);
    if (!values)
        return Result<SampledSignal>{values.error()};
    auto first = nearest(x.value(), float(options.minimum)),
         last = nearest(x.value(), float(options.maximum));
    const auto offset = p.background->window->first;
    if (first < offset || last < first || last >= offset + values.value().size())
        return Result<SampledSignal>{
            invalid("background.filter", "Filter interval exceeds existing background")};
    first -= offset;
    last -= offset;
    Vector working(values.value().begin() + first, values.value().begin() + last + 1);
    for (int i = 0; i < options.iterations; ++i) {
        auto filtered = average(working, options.half_window);
        for (std::size_t j = 0; j < working.size(); ++j)
            working[j] = std::min(working[j], filtered[j]);
    }
    if (!std::all_of(working.begin(), working.end(), [](float v) { return std::isfinite(v); }))
        return Result<SampledSignal>{numerical("background.filter", "Nonfinite filtered background")};
    auto out = *p.background;
    for (std::size_t j = 0; j < working.size(); ++j)
        out.values[first + j] = working[j];
    return Result<SampledSignal>{std::move(out)};
}

Result<Pattern> subtract_background(const Pattern &p) {
    if (!p.processing.background_subtracted || !p.processing.intensity_scaled)
        return Result<Pattern>{Error{ErrorCode::missing_data, "background.state",
                                     "Explicit subtraction/scaling state required"}};
    if (*p.processing.background_subtracted)
        return Result<Pattern>{p};
    if (!p.background || !p.background->window || p.background->window->first != 0 ||
        p.background->values.size() != p.observed.size())
        return Result<Pattern>{
            Error{ErrorCode::missing_data, "background.alignment", "Complete aligned background required"}};
    auto y = narrow(p.observed), b = narrow(p.background->values);
    if (!y)
        return Result<Pattern>{y.error()};
    if (!b)
        return Result<Pattern>{b.error()};
    if (y.value().empty())
        return Result<Pattern>{invalid("pattern", "Empty pattern")};
    for (std::size_t i = 0; i < y.value().size(); ++i)
        y.value()[i] = std::max(0.f, y.value()[i] - b.value()[i]);
    if (*p.processing.intensity_scaled) {
        float maximum = *std::max_element(y.value().begin(), y.value().end());
        if (maximum == 0)
            return Result<Pattern>{numerical("background.scale", "Legacy all-zero rescaling is undefined")};
        for (auto &v : y.value())
            v = 1000.f * v / maximum;
    }
    if (!std::all_of(y.value().begin(), y.value().end(), [](float v) { return std::isfinite(v); }))
        return Result<Pattern>{numerical("background.subtract", "Nonfinite subtraction")};
    Pattern out = p;
    out.observed = widen(y.value());
    out.background->values.assign(out.observed.size(), 0);
    out.processing.background_subtracted = true;
    return Result<Pattern>{std::move(out)};
}
namespace detail {
Result<Pattern> prepare_analysis_signal(const AnalysisSpectrum &spectrum) {
    const auto &input = spectrum.pattern();
    if (spectrum.purpose() == AnalysisPurpose::classical_search)
        return subtract_background(input);
    Pattern prepared = input;
    for (std::size_t i = 0; i < prepared.observed.size(); ++i)
        prepared.observed[i] = float(input.observed[i]) - float(input.background->values[i]);
    prepared.background->values.assign(prepared.observed.size(), 0);
    prepared.processing.background_subtracted = true;
    return Result<Pattern>{std::move(prepared)};
}
} // namespace detail
} // namespace qualpf::preprocessing
