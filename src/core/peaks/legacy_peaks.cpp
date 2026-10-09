#include "../preprocessing/legacy_internal.h"
#include "legacy_polynomial.h"

namespace qualpf::preprocessing {
namespace {
using namespace detail;
struct Measured {
    float x, y, width, area = 0;
    bool removed = false;
};
struct WidthTrial {
    std::optional<float> width;
    bool undefined = false;
};
WidthTrial gaussian_width(const Vector &x, const Vector &y, int at, int radius) {
    int begin = std::max(0, at - radius), end = std::min(int(x.size()) - 1, at + radius), n = end - begin + 1;
    auto values = std::span(x).subspan(begin, n);
    float mean = sum(values) / float(n), squares = 0, deviations = 0;
    for (float v : values) {
        float s = v - mean;
        squares += s * s;
        deviations += s;
    }
    const float variance = (squares - deviations * deviations / float(n)) / float(n - 1);
    if (!(variance > 0) || !std::isfinite(variance))
        return {};
    float sd = std::sqrt(variance);
    if (!(sd > 0))
        return {};
    std::vector<std::array<float, 3>> matrix(n);
    Vector rhs(n);
    for (int i = 0; i < n; ++i) {
        if (y[begin + i] <= 0)
            return {};
        float t = (x[begin + i] - mean) / sd;
        matrix[i] = {1, t, t * t};
        rhs[i] = std::log(y[begin + i]);
    }
    auto c = fit_log_quadratic(std::move(matrix), rhs);
    if (c[2] > 0 || !std::isfinite(c[2]))
        return {};
    if (c[2] == 0)
        return {{}, true};
    const float divisor = std::sqrt(-2.f * c[2]);
    if (!std::isfinite(divisor) || divisor <= 0 ||
        double(sd) * 2.35482 / double(divisor) > std::numeric_limits<float>::max())
        return {{}, true};
    return {sd * 2.35482f / divisor};
}
float previous_width(const std::vector<Measured> &peaks) {
    if (peaks.size() == 1)
        return .2f;
    int first = std::max(0, int(peaks.size()) - 6), last = int(peaks.size()) - 2;
    if (first == last)
        return peaks[first].width;
    float sum = 0;
    for (int i = first; i <= last; ++i)
        sum += peaks[i].width;
    return sum / float(last - first); // Historical denominator excludes one element.
}
int local_maximum(const Vector &y, int first, int last) {
    for (;;) {
        int at = int(std::max_element(y.begin() + first, y.begin() + last + 1) - y.begin());
        if (first == last)
            return at;
        if (at == first) {
            ++first;
            continue;
        }
        if (at == last) {
            --last;
            continue;
        }
        return at;
    }
}
Status estimate_intensities(std::vector<Measured> &peaks, const Vector &x, const Vector &y) {
    std::vector<int> centers;
    for (const auto &peak : peaks)
        centers.push_back(int(nearest(x, peak.x)));
    auto minimum = [&](int first, int last) {
        return int(std::min_element(y.begin() + first, y.begin() + last + 1) - y.begin());
    };
    for (int i = 0; i < int(peaks.size()); ++i) {
        auto &peak = peaks[i];
        int center = centers[i];
        int first = int(nearest(x, peak.x - 2 * peak.width)), last = int(nearest(x, peak.x + 2 * peak.width));
        first = minimum(first, center);
        for (int j = i - 1; j >= 0; --j) {
            if (first > centers[j])
                break;
            if (peaks[j].y > peak.y) {
                first = minimum(centers[j], center);
                break;
            }
        }
        last = minimum(center, last);
        for (int j = i + 1; j < int(peaks.size()); ++j) {
            if (last < centers[j])
                break;
            if (peaks[j].y > peak.y) {
                last = minimum(center, centers[j]);
                break;
            }
        }
        if (first == last)
            return Status{Error{ErrorCode::undefined_preprocessing, "peaks.intensity",
                                "Undefined local baseline for candidate " + std::to_string(i) +
                                    " at sample " + std::to_string(first),
                                "coincident_baseline_endpoints"}};
        float slope = (y[last] - y[first]) / (x[last] - x[first]);
        float intercept = -slope * x[first] + y[first];
        peak.y -= slope * peak.x + intercept;
        float integral = 0;
        for (int k = first; k < last; ++k)
            integral += (x[k + 1] - x[k]) * (y[k + 1] + y[k]);
        peak.area = .5f * integral;
    }
    return Status{std::monostate{}};
}
Result<std::pair<std::vector<Measured>, std::vector<Measured>>>
detect(const Vector &x, const Vector &y, PeakOptions &o, std::optional<float> normalizationMaximum = {}) {
    using Output = std::pair<std::vector<Measured>, std::vector<Measured>>;
    float maxy = normalizationMaximum.value_or(*std::max_element(y.begin(), y.end()));
    if (!(maxy > 0))
        return Result<Output>{
            numerical("peaks.normalization", "Legacy nonpositive signal normalization is undefined")};
    int w = o.half_window;
    if (o.automatic_window) {
        Vector steps;
        for (std::size_t i = 1; i < x.size(); ++i)
            steps.push_back(x[i] - x[i - 1]);
        float step = sum(steps) / float(steps.size());
        float count = o.search_range / step;
        if (!std::isfinite(count) || count < 0)
            return Result<Output>{invalid("peaks.window", "Invalid automatic window")};
        w = count > 100 ? 100 : int(std::round(count));
        if (w == 0)
            w = 2;
        if (2 * w < 3)
            w = int(std::ceil(float(w) / 2)) + 1;
        o.half_window = w;
    }
    Vector normalized;
    for (float v : y)
        normalized.push_back((1000.f / maxy) * v);
    auto derivative = sg(normalized, w, std::min(o.polynomial_degree, 2 * w), 3);
    if (!derivative)
        return Result<Output>{derivative.error()};
    auto &d = derivative.value();
    float maximum = 0;
    for (float v : d)
        maximum = std::max(maximum, std::abs(v));
    if (maximum == 0)
        return Result<Output>{
            numerical("peaks.derivative", "Legacy zero derivative normalization is undefined")};
    for (auto &v : d)
        v = 100.f * v / maximum;
    int radius = int(std::floor(std::max(.7f * w, 2.f)));
    std::vector<Measured> peaks;
    for (int i = 0; i < int(x.size()) - 1; ++i) {
        if (!(d[i] < 0 && d[i + 1] > 0 && d[i + 1] - d[i] > o.slope && normalized[i] > 0))
            continue;
        int first = std::max(0, i - radius), last = std::min(int(x.size()) - 1, i + radius);
        std::optional<float> fitted;
        for (int r = std::max(3, radius - 6); r <= radius + 6; ++r) {
            auto trial = gaussian_width(x, y, i, r);
            if (trial.undefined)
                return Result<Output>{Error{ErrorCode::undefined_preprocessing, "peaks.width",
                                            "Undefined Gaussian width", "invalid_fitted_width"}};
            fitted = trial.width;
            if (fitted)
                break;
        }
        if (!fitted)
            continue; // Legacy rejects a candidate whose trial fits all fail.
        const float fitWidth = *fitted;
        if (!std::isfinite(fitWidth) || fitWidth <= 0)
            return Result<Output>{Error{ErrorCode::undefined_preprocessing, "peaks.width",
                                        "Undefined fitted width", "invalid_fitted_width"}};
        float width = fitWidth;
        int at = local_maximum(normalized, first, last);
        if (std::any_of(peaks.begin(), peaks.end(), [&](auto p) {
                return std::abs(p.x - x[at]) <= std::numeric_limits<float>::epsilon();
            }))
            continue;
        if (!peaks.empty() && std::abs(peaks.back().x - x[at]) < (peaks.back().width + width) * .15f) {
            if (peaks.back().y >= y[at])
                continue;
            peaks.back().removed = true;
        }
        if (normalized[at] / width < (o.threshold_percent / 100.f) / (o.search_range * 3.f))
            continue;
        if (width > 2.f) {
            at = int(std::max_element(normalized.begin() + first, normalized.begin() + last + 1) -
                     normalized.begin());
            width = peaks.empty() ? o.search_range * 1.1f : previous_width(peaks);
        }
        if (!std::isfinite(width) || width <= 0)
            return Result<Output>{Error{ErrorCode::undefined_preprocessing, "peaks.width",
                                        "Undefined fitted width", "invalid_fitted_width"}};
        peaks.push_back({x[at], y[at], width});
    }
    std::erase_if(peaks, [](auto p) { return p.removed; });
    std::stable_sort(peaks.begin(), peaks.end(), [](auto a, auto b) { return a.x < b.x; });
    auto intensities = estimate_intensities(peaks, x, y);
    if (!intensities)
        return Result<Output>{intensities.error()};
    for (auto peak : peaks)
        if (!std::isfinite(peak.y) || !std::isfinite(peak.area))
            return Result<Output>{Error{ErrorCode::undefined_preprocessing, "peaks.intensity",
                                        "Nonfinite local baseline/integration",
                                        "nonfinite_candidate_measurement"}};
    auto candidates = peaks;
    if (!peaks.empty()) {
        float largest =
            std::max_element(peaks.begin(), peaks.end(), [](auto a, auto b) { return a.y < b.y; })->y;
        if (largest == 0)
            return Result<Output>{numerical("peaks.filter", "Undefined zero maximum height")};
        std::erase_if(peaks, [&](auto p) { return !(p.y / largest >= o.threshold_percent / 100.f); });
    }
    if (o.maximum_peaks > 0 && int(peaks.size()) > o.maximum_peaks) {
        std::stable_sort(peaks.begin(), peaks.end(), [](auto a, auto b) { return a.y > b.y; });
        o.threshold_percent = 100.f * peaks[o.maximum_peaks].y / peaks.front().y;
        peaks.resize(o.maximum_peaks);
        std::stable_sort(peaks.begin(), peaks.end(), [](auto a, auto b) { return a.x < b.x; });
    }
    return Result<Output>{Output{std::move(peaks), std::move(candidates)}};
}
} // namespace

Result<DSpacingAngstrom> legacy_d(TwoThetaDegrees angle, WavelengthAngstrom wavelength) {
    if (!std::isfinite(angle.value) || angle.value <= 0 || angle.value > 180 ||
        !std::isfinite(wavelength.value) || wavelength.value <= 0)
        return Result<DSpacingAngstrom>{detail::invalid("peaks.coordinates", "Invalid angle/wavelength")};
    const float dtor = 1.f / (180.f / (4.f * std::atan(1.f)));
    float d = float(wavelength.value) / (2.f * std::sin(dtor * float(angle.value) * .5f));
    if (!std::isfinite(d) || d <= 0)
        return Result<DSpacingAngstrom>{detail::numerical("peaks.d", "Unrepresentable d")};
    return Result<DSpacingAngstrom>{DSpacingAngstrom{double(d)}};
}
Result<TwoThetaDegrees> legacy_two_theta(DSpacingAngstrom spacing, WavelengthAngstrom wavelength) {
    if (!std::isfinite(spacing.value) || spacing.value <= 0 || !std::isfinite(wavelength.value) ||
        wavelength.value <= 0)
        return Result<TwoThetaDegrees>{detail::invalid("peaks.coordinates", "Invalid d/wavelength")};
    float ratio = .5f * float(wavelength.value) / float(spacing.value);
    if (!std::isfinite(ratio) || ratio <= 0 || ratio > 1)
        return Result<TwoThetaDegrees>{detail::invalid("peaks.d", "Legacy asin sentinel domain")};
    float angle = 2.f * std::asin(ratio) * (180.f / (4.f * std::atan(1.f)));
    return Result<TwoThetaDegrees>{TwoThetaDegrees{double(angle)}};
}
Result<PeakSet> export_peaks(const PeakSet &input, AngularWidthDegrees tolerance) {
    if (!input.effective_wavelength || !std::isfinite(input.effective_wavelength->value) ||
        input.effective_wavelength->value <= 0 || !std::isfinite(tolerance.value) || tolerance.value <= 0)
        return Result<PeakSet>{
            detail::invalid("peaks.export", "Explicit positive wavelength and tolerance required")};
    PeakSet out = input;
    float wave = float(input.effective_wavelength->value), delta = float(tolerance.value);
    for (auto &peak : out.peaks) {
        if (!peak.d_spacing || !std::isfinite(peak.d_spacing->value) || peak.d_spacing->value <= 0)
            return Result<PeakSet>{detail::invalid("peaks.d", "Missing/invalid d")};
        float d = float(peak.d_spacing->value);
        float value =
            (3.14159265f / 180.f) * delta * (d * d) * std::sqrt(1.f - wave * wave / (4.f * (d * d))) / wave;
        if (!std::isfinite(value) || value < 0)
            return Result<PeakSet>{detail::numerical("peaks.delta_d", "Undefined delta d")};
        peak.delta_d = DSpacingAngstrom{double(value)};
    }
    return Result<PeakSet>{std::move(out)};
}
Result<double> automatic_tolerance(const PeakSet &peaks) {
    if (peaks.peaks.empty())
        return Result<double>{.5};
    float total = 0;
    for (auto p : peaks.peaks) {
        if (!p.fwhm || !std::isfinite(p.fwhm->value) || p.fwhm->value < 0)
            return Result<double>{detail::invalid("peaks.fwhm", "Missing/invalid width")};
        total += float(p.fwhm->value);
    }
    if (!std::isfinite(total))
        return Result<double>{detail::numerical("peaks.tolerance", "Width sum overflow")};
    return Result<double>{double(std::min(.5f, 1.3f * (total / float(peaks.peaks.size()))))};
}
Result<PeakSearchResult> find_peaks(const Pattern &pattern, const PeakOptions &options,
                                    WavelengthAngstrom wavelength, AngularWidthDegrees tolerance) {
    auto x = detail::grid(pattern), y = detail::narrow(pattern.observed);
    if (!x)
        return Result<PeakSearchResult>{x.error()};
    if (!y)
        return Result<PeakSearchResult>{y.error()};
    if (!std::isfinite(options.threshold_percent) || options.threshold_percent < 0 ||
        !std::isfinite(options.slope) || options.slope < 0 || !std::isfinite(options.search_range) ||
        options.polynomial_degree < 3 || options.polynomial_degree > 20 || !std::isfinite(wavelength.value) ||
        wavelength.value <= 0 || wavelength.value >= 8)
        return Result<PeakSearchResult>{detail::invalid("peaks.options", "Invalid peak parameters")};
    PeakOptions applied = options;
    if (applied.search_range < 0) {
        PeakOptions initial;
        const float w = float(wavelength.value);
        initial.search_range =
            .15f * std::sqrt((64.f - 1.54056f * 1.54056f) / (64.f - w * w)) * (w / 1.54056f);
        auto found = detect(x.value(), y.value(), initial);
        if (!found)
            return Result<PeakSearchResult>{found.error()};
        auto p = found.value().first;
        applied.search_range = initial.search_range;
        if (!p.empty()) {
            std::stable_sort(p.begin(), p.end(), [](auto a, auto b) { return a.y > b.y; });
            int n = std::min(3, int(p.size()));
            float sum = 0;
            for (int i = 0; i < n; ++i)
                sum += p[i].width;
            applied.search_range = (sum / float(n)) * .9f;
        }
    }
    Vector detectionX = x.value(), detectionY = y.value();
    if (applied.detection_range) {
        const auto range = *applied.detection_range;
        if (!std::isfinite(range.minimum.value) || !std::isfinite(range.maximum.value) ||
            range.minimum.value > range.maximum.value)
            return Result<PeakSearchResult>{detail::invalid("peaks.range", "Invalid detection range")};
        auto first = nearest(x.value(), float(range.minimum.value)),
             last = nearest(x.value(), float(range.maximum.value));
        if (last <= first)
            return Result<PeakSearchResult>{Error{ErrorCode::undefined_preprocessing, "peaks.range",
                                                  "Legacy degenerate range leaves previous output unchanged",
                                                  "degenerate_detection_range"}};
        detectionX.assign(x.value().begin() + first, x.value().begin() + last + 1);
        detectionY.assign(y.value().begin() + first, y.value().begin() + last + 1);
    }
    const float normalizationMaximum = *std::max_element(y.value().begin(), y.value().end());
    auto found = detect(detectionX, detectionY, applied, normalizationMaximum);
    if (!found)
        return Result<PeakSearchResult>{found.error()};
    if (int(found.value().first.size()) < applied.minimum_peaks) {
        applied.threshold_percent = 1.5f * .4f;
        found = detect(detectionX, detectionY, applied, normalizationMaximum);
        if (!found)
            return Result<PeakSearchResult>{found.error()};
    }
    auto convert = [&](const std::vector<Measured> &list) -> Result<PeakSet> {
        PeakSet set;
        set.dataset = pattern.id;
        set.effective_wavelength = WavelengthAngstrom{double(float(wavelength.value))};
        set.source_precision = SourcePrecision::float32_abi;
        set.intensity_scale = pattern.intensity_scale;
        for (auto p : list) {
            auto d = legacy_d(TwoThetaDegrees{p.x}, wavelength);
            if (!d)
                return Result<PeakSet>{d.error()};
            Peak peak;
            peak.id = {set.peaks.size() + 1};
            peak.two_theta = TwoThetaDegrees{p.x};
            peak.d_spacing = d.value();
            peak.intensity = p.y;
            peak.original_intensity = p.y;
            peak.fwhm = AngularWidthDegrees{p.width};
            peak.integrated_intensity = p.area;
            set.peaks.push_back(peak);
        }
        return export_peaks(set, tolerance);
    };
    auto peaks = convert(found.value().first), candidates = convert(found.value().second);
    if (!peaks)
        return Result<PeakSearchResult>{peaks.error()};
    if (!candidates)
        return Result<PeakSearchResult>{candidates.error()};
    auto automatic = automatic_tolerance(peaks.value());
    if (!automatic)
        return Result<PeakSearchResult>{automatic.error()};
    return Result<PeakSearchResult>{PeakSearchResult{std::move(peaks).value(), std::move(candidates).value(),
                                                     applied, automatic.value()}};
}
} // namespace qualpf::preprocessing
