#include "../preprocessing/legacy_internal.h"
#include "legacy_polynomial.h"
#include "legacy_peak_measurement.h"
#include "qualpf/preprocessing/algorithms.h"
#include "../preprocessing/analysis_signal.h"

namespace qualpf::preprocessing {
namespace {
using namespace detail;
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
} // namespace
namespace detail {
Result<std::vector<RejectedPeakCandidate>> measure_peak_candidates(std::vector<Measured> &peaks,
    std::span<const float> x, std::span<const float> y, PeakDetectionStage stage, std::size_t sample_offset) {
    std::vector<RejectedPeakCandidate> rejected;
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
            if (!peaks[j].removed && peaks[j].y > peak.y) {
                first = minimum(centers[j], center);
                break;
            }
        }
        last = minimum(center, last);
        for (int j = i + 1; j < int(peaks.size()); ++j) {
            if (last < centers[j])
                break;
            if (!peaks[j].removed && peaks[j].y > peak.y) {
                last = minimum(center, centers[j]);
                break;
            }
        }
        if (first == last) {
            rejected.push_back({stage, std::size_t(i), sample_offset + std::size_t(first),
                               TwoThetaDegrees{peak.x}, "coincident_baseline_endpoints"});
            peak.removed = true;
            continue; // Never divide by zero or manufacture a finite measurement.
        }
        float slope = (y[last] - y[first]) / (x[last] - x[first]);
        float intercept = -slope * x[first] + y[first];
        peak.y -= slope * peak.x + intercept;
        float integral = 0;
        for (int k = first; k < last; ++k)
            integral += (x[k + 1] - x[k]) * (y[k + 1] + y[k]);
        peak.area = .5f * integral;
    }
    std::erase_if(peaks, [](const auto &p) { return p.removed; });
    if (peaks.empty() && !rejected.empty())
        return Result<std::vector<RejectedPeakCandidate>>{Error{ErrorCode::undefined_preprocessing,
            "peaks.intensity", "All candidates have coincident local baseline endpoints; first candidate " +
                std::to_string(rejected.front().candidate_index) + " at sample " +
                std::to_string(rejected.front().sample_index), "coincident_baseline_endpoints"}};
    return Result<std::vector<RejectedPeakCandidate>>{std::move(rejected)};
}
} // namespace detail
namespace detail {
Result<std::vector<std::size_t>> detect_peak_candidates(const Vector &x, const Vector &y,
    PeakOptions &o, std::optional<float> normalizationMaximum) {
    using Output = std::vector<std::size_t>;
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
    Output triggers;
    for (int i = 0; i < int(x.size()) - 1; ++i) {
        if (!(d[i] < 0 && d[i + 1] > 0 && d[i + 1] - d[i] > o.slope && normalized[i] > 0))
            continue;
        triggers.push_back(std::size_t(i));
    }
    return Result<Output>{std::move(triggers)};
}
Result<RefinedPeakCandidates> refine_peak_candidates(const Vector &x, const Vector &y,
    PeakOptions &o, std::span<const std::size_t> triggers, PeakDetectionStage stage,
    std::vector<RejectedPeakCandidate> &rejected, std::size_t sample_offset,
    std::optional<float> normalizationMaximum) {
    using Output = RefinedPeakCandidates;
    rejected.clear();
    const float maxy = normalizationMaximum.value_or(*std::max_element(y.begin(), y.end()));
    if (!(maxy > 0))
        return Result<Output>{numerical("peaks.normalization", "Legacy nonpositive signal normalization is undefined")};
    Vector normalized;
    for (float v : y)
        normalized.push_back((1000.f / maxy) * v);
    const int radius = int(std::floor(std::max(.7f * o.half_window, 2.f)));
    std::vector<Measured> peaks;
    for (std::size_t trigger : triggers) {
        const int i = int(trigger);
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
    auto intensities = measure_peak_candidates(peaks, x, y, stage, sample_offset);
    if (!intensities)
        return Result<Output>{intensities.error()};
    rejected = std::move(intensities).value();
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
Result<FwhmEstimate> summarize_fwhm(std::span<const Peak> input) {
    FwhmEstimate result;
    std::vector<Peak> p(input.begin(), input.end());
    for (const auto &peak : p)
        if (!std::isfinite(peak.intensity))
            return Result<FwhmEstimate>{invalid("peaks.calibration", "Invalid initial height")};
    if (!p.empty()) {
        std::stable_sort(p.begin(), p.end(), [](const auto &a, const auto &b) { return a.intensity > b.intensity; });
        int n = std::min(3, int(p.size()));
        float sum = 0;
        for (int i = 0; i < n; ++i) {
            if (!p[i].fwhm || !std::isfinite(p[i].fwhm->value) || p[i].fwhm->value < 0)
                return Result<FwhmEstimate>{invalid("peaks.calibration", "Missing initial fitted width")};
            sum += float(p[i].fwhm->value);
        }
        result.width = AngularWidthDegrees{sum / float(n)};
        result.peak_count = std::size_t(n);
        result.state = FwhmEstimate::State::measured;
    }
    auto valid = validate_fwhm_estimate(result);
    if (!valid) return Result<FwhmEstimate>{valid.error()};
    return Result<FwhmEstimate>{std::move(result)};
}
} // namespace detail
namespace {
using namespace detail;
Result<RefinedPeakCandidates> detect(const Vector &x, const Vector &y, PeakOptions &o,
    PeakDetectionStage stage, std::vector<RejectedPeakCandidate> &rejected,
    std::size_t sample_offset = 0, std::optional<float> normalizationMaximum = {}) {
    rejected.clear();
    auto triggers = detect_peak_candidates(x, y, o, normalizationMaximum);
    if (!triggers) return Result<RefinedPeakCandidates>{triggers.error()};
    return refine_peak_candidates(x, y, o, triggers.value(), stage, rejected, sample_offset, normalizationMaximum);
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
Status validate_initial_peak_options(const InitialPeakOptions &o) {
    if (!std::isfinite(o.slope) || o.slope < 0 || o.slope > 200 ||
        o.half_window < 2 || o.half_window > 100 || o.polynomial_degree < 3 || o.polynomial_degree > 20)
        return Status{Error{ErrorCode::invalid_input, "peaks.initial.options",
            "Derivative threshold must be from 0 to 200, half-window from 2 to 100 and degree from 3 to 20",
            "invalid_initial_peak_options"}};
    return Status{std::monostate{}};
}
static Result<InitialPeakResult> initial_kernel(const Pattern &pattern, WavelengthAngstrom wavelength,
    const InitialPeakOptions &options, const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<InitialPeakResult>{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    auto valid = validate_initial_peak_options(options);
    if (!valid) return Result<InitialPeakResult>{valid.error()};
    auto x = detail::grid(pattern), y = detail::narrow(pattern.observed);
    if (!x) return Result<InitialPeakResult>{x.error()};
    if (!y) return Result<InitialPeakResult>{y.error()};
    if (!std::isfinite(wavelength.value) || wavelength.value <= 0 || wavelength.value >= 8)
        return Result<InitialPeakResult>{detail::invalid("peaks.options", "Invalid peak parameters")};
    PeakOptions initial;
    initial.slope = options.slope;
    initial.automatic_window = options.automatic_window;
    initial.half_window = options.half_window;
    initial.polynomial_degree = options.polynomial_degree;
    const float w = float(wavelength.value);
    initial.search_range =
        .15f * std::sqrt((64.f - 1.54056f * 1.54056f) / (64.f - w * w)) * (w / 1.54056f);
    auto found = detail::detect_peak_candidates(x.value(), y.value(), initial);
    if (!found) return Result<InitialPeakResult>{found.error()};
    InitialPeakResult result;
    result.dataset = pattern.id;
    result.applied = initial;
    for (auto index : found.value()) result.candidates.push_back({index});
    return Result<InitialPeakResult>{std::move(result)};
}
Status validate_fwhm_estimate(const FwhmEstimate &estimate) {
    const bool empty = estimate.state == FwhmEstimate::State::no_initial_peaks &&
        !estimate.width && estimate.peak_count == 0;
    const bool measured = estimate.state == FwhmEstimate::State::measured && estimate.width &&
        estimate.peak_count > 0 && std::isfinite(estimate.width->value) && estimate.width->value >= 0 &&
        std::isfinite(float(estimate.width->value));
    if (!empty && !measured)
        return Status{Error{ErrorCode::invalid_input, "peaks.calibration",
            "Invalid physical FWHM evidence", "invalid_algorithm_output"}};
    return Status{std::monostate{}};
}
Status validate_initial_peak_candidates(const InitialPeakResult &initial, const Pattern &pattern) {
    const auto bad = [](const char *field, const char *message) {
        return Status{Error{ErrorCode::invalid_input, field, message, "invalid_algorithm_output"}};
    };
    if (initial.dataset != pattern.id)
        return bad("algorithm.output.dataset", "Initial candidates belong to another pattern");
    const auto &o = initial.applied;
    if (!std::isfinite(o.threshold_percent) || o.threshold_percent < 0 || o.threshold_percent > 100 ||
        !std::isfinite(o.slope) || o.slope < 0 || o.slope > 1e6f ||
        !std::isfinite(o.search_range) || o.search_range < 0 ||
        o.half_window < 1 || o.half_window > 100 || o.polynomial_degree < 3 || o.polynomial_degree > 20 ||
        o.maximum_peaks < -1 || o.maximum_peaks > 1000000 ||
        o.minimum_peaks < -1 || o.minimum_peaks > 1000000 || o.detection_range)
        return bad("algorithm.output.initial", "Invalid initial detection parameters");
    for (std::size_t i = 0; i < initial.candidates.size(); ++i) {
        const auto at = initial.candidates[i].sample_index;
        if (pattern.two_theta.size() < 2 || at >= pattern.two_theta.size() - 1 ||
            at > std::size_t(std::numeric_limits<int>::max()) ||
            (i && at <= initial.candidates[i - 1].sample_index))
            return bad("algorithm.output.initial", "Initial candidate indices must be ordered derivative triggers on the input grid");
    }
    return Status{std::monostate{}};
}
Status validate_fwhm_result(const FwhmEstimationResult &result, const InitialPeakResult &initial,
    const Pattern &pattern) {
    auto valid = validate_fwhm_estimate(result.estimate);
    if (!valid) return valid;
    if (result.estimate.peak_count > initial.candidates.size())
        return Status{Error{ErrorCode::invalid_input, "algorithm.output.fwhm",
            "Width evidence exceeds the initial candidate count", "invalid_algorithm_output"}};
    for (const auto &r : result.rejected_candidates)
        if (r.stage != PeakDetectionStage::initial || r.reason != "coincident_baseline_endpoints" ||
            r.candidate_index >= initial.candidates.size() ||
            !std::isfinite(r.two_theta.value) || r.sample_index >= pattern.two_theta.size() ||
            r.two_theta.value != double(float(pattern.two_theta[r.sample_index].value)))
            return Status{Error{ErrorCode::invalid_input, "algorithm.output.rejected_candidates",
                "Invalid initial rejected candidate diagnostic", "invalid_algorithm_output"}};
    return Status{std::monostate{}};
}
static Result<FwhmEstimationResult> fwhm_kernel(const Pattern &pattern,
    const InitialPeakResult &initial, const ExecutionContext &context) {
    using Return = Result<FwhmEstimationResult>;
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    auto x = detail::grid(pattern), y = detail::narrow(pattern.observed);
    if (!x) return Return{x.error()};
    if (!y) return Return{y.error()};
    auto valid = validate_initial_peak_candidates(initial, pattern);
    if (!valid) return Return{valid.error()};
    FwhmEstimationResult result;
    // No derivative work here. Even a nonempty raw candidate list may refine to
    // no usable measurements, retaining the original wavelength-fallback path.
    std::vector<std::size_t> triggers;
    for (const auto &candidate : initial.candidates) triggers.push_back(candidate.sample_index);
    auto applied = initial.applied;
    auto refined = detail::refine_peak_candidates(x.value(), y.value(), applied, triggers,
        PeakDetectionStage::initial, result.rejected_candidates);
    if (!refined) return Return{refined.error()};
    std::vector<Peak> measurements;
    for (const auto &p : refined.value().first) {
        Peak peak;
        peak.intensity = p.y;
        peak.fwhm = AngularWidthDegrees{p.width};
        measurements.push_back(peak);
    }
    auto width = detail::summarize_fwhm(measurements);
    if (!width) return Return{width.error()};
    result.estimate = std::move(width).value();
    valid = validate_fwhm_result(result, initial, pattern);
    if (!valid) return Return{valid.error()};
    return Return{std::move(result)};
}
Result<PeakCalibration> prepare_classic_peak_parameters(const PeakOptions &options, const FwhmEstimate &width,
    WavelengthAngstrom wavelength, const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<PeakCalibration>{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    if (!std::isfinite(options.search_range) || !std::isfinite(wavelength.value) ||
        wavelength.value <= 0 || wavelength.value >= 8)
        return Result<PeakCalibration>{detail::invalid("peaks.options", "Invalid peak parameters")};
    PeakCalibration result;
    result.applied = options;
    if (options.search_range < 0) {
        auto valid = validate_fwhm_estimate(width);
        if (!valid) return Result<PeakCalibration>{valid.error()};
        if (width.state == FwhmEstimate::State::measured) {
            result.applied.search_range = float(width.width->value) * .9f;
            result.estimated_fwhm = width.width;
            result.width_count = width.peak_count;
            result.source = PeakCalibration::Source::strongest_initial_widths;
        } else {
            const float w = float(wavelength.value);
            result.applied.search_range =
                .15f * std::sqrt((64.f - 1.54056f * 1.54056f) / (64.f - w * w)) * (w / 1.54056f);
            result.source = PeakCalibration::Source::wavelength_seed;
        }
    }
    result.formal_search_range = AngularWidthDegrees{result.applied.search_range};
    return Result<PeakCalibration>{std::move(result)};
}
Result<PeakCalibration> prepare_supplied_fwhm_parameters(const PeakOptions &options, AngularWidthDegrees width,
    PeakCalibration::Source source, const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<PeakCalibration>{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    if (!std::isfinite(width.value) || width.value <= 0 || width.value > 180 ||
        (source != PeakCalibration::Source::fixed_fwhm && source != PeakCalibration::Source::geometric_fwhm))
        return Result<PeakCalibration>{detail::invalid("fwhm.width", "Invalid supplied physical FWHM")};
    PeakCalibration result;
    result.applied = options;
    result.applied.search_range = float(width.value) * .9f;
    if (!(result.applied.search_range > 0))
        return Result<PeakCalibration>{detail::invalid("fwhm.width", "FWHM is below the Classical parameter precision")};
    result.estimated_fwhm = width;
    result.formal_search_range = AngularWidthDegrees{result.applied.search_range};
    result.source = source;
    result.width_count = source == PeakCalibration::Source::geometric_fwhm ? 1 : 0;
    return Result<PeakCalibration>{std::move(result)};
}
template<class Initial, class Width, class Formal>
static Result<PeakSearchResult> orchestrate_peaks(const Pattern &pattern, const PeakOptions &options,
    WavelengthAngstrom wavelength, AngularWidthDegrees tolerance, Initial initial, Width fwhm, Formal extractor,
    const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<PeakSearchResult>{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    auto x = detail::grid(pattern), y = detail::narrow(pattern.observed);
    if (!x) return Result<PeakSearchResult>{x.error()};
    if (!y) return Result<PeakSearchResult>{y.error()};
    if (!std::isfinite(options.threshold_percent) || options.threshold_percent < 0 ||
        !std::isfinite(options.slope) || options.slope < 0 || !std::isfinite(options.search_range) ||
        options.polynomial_degree < 3 || options.polynomial_degree > 20 || !std::isfinite(wavelength.value) ||
        wavelength.value <= 0 || wavelength.value >= 8)
        return Result<PeakSearchResult>{detail::invalid("peaks.options", "Invalid peak parameters")};
    std::optional<FwhmEstimate> width;
    std::vector<RejectedPeakCandidate> rejected;
    PeakCalibration prepared;
    prepared.applied = options;
    prepared.formal_search_range = AngularWidthDegrees{options.search_range};
    if (options.search_range < 0) {
        auto found = initial(pattern, wavelength, context);
        if (!found) return Result<PeakSearchResult>{found.error()};
        auto valid = validate_initial_peak_candidates(found.value(), pattern);
        if (!valid) return Result<PeakSearchResult>{valid.error()};
        auto estimated = fwhm(pattern, found.value(), context);
        if (!estimated) return Result<PeakSearchResult>{estimated.error()};
        valid = validate_fwhm_result(estimated.value(), found.value(), pattern);
        if (!valid) return Result<PeakSearchResult>{valid.error()};
        rejected = std::move(estimated.value().rejected_candidates);
        width = std::move(estimated.value().estimate);
        auto calibrated = prepare_classic_peak_parameters(options, *width, wavelength, context);
        if (!calibrated) return Result<PeakSearchResult>{calibrated.error()};
        prepared = std::move(calibrated).value();
    }
    if (context.cancellation.stop_requested())
        return Result<PeakSearchResult>{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    if (!std::isfinite(prepared.formal_search_range.value) || prepared.formal_search_range.value < 0 ||
        prepared.formal_search_range.value != double(prepared.applied.search_range))
        return Result<PeakSearchResult>{detail::invalid("peaks.calibration", "Inconsistent calibrated search range")};
    auto result = extractor(pattern, prepared, wavelength, tolerance, context);
    if (result) {
        result.value().calibration = prepared;
        result.value().fwhm = width;
        auto &formal = result.value().rejected_candidates;
        formal.insert(formal.begin(), rejected.begin(), rejected.end());
    }
    return result;
}
static Result<PeakSearchResult> formal_kernel(const Pattern &pattern, const PeakCalibration &calibration,
    WavelengthAngstrom wavelength, AngularWidthDegrees tolerance, const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<PeakSearchResult>{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    const auto &options = calibration.applied;
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
    if (options.search_range < 0)
        return Result<PeakSearchResult>{detail::invalid("peaks.calibration", "Formal extraction requires calibrated parameters")};
    PeakOptions applied = options;
    Vector detectionX = x.value(), detectionY = y.value();
    std::size_t sampleOffset = 0;
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
        sampleOffset = first;
    }
    const float normalizationMaximum = *std::max_element(y.value().begin(), y.value().end());
    std::vector<RejectedPeakCandidate> rejected;
    auto found = detect(detectionX, detectionY, applied, PeakDetectionStage::formal, rejected,
                        sampleOffset, normalizationMaximum);
    if (!found)
        return Result<PeakSearchResult>{found.error()};
    if (int(found.value().first.size()) < applied.minimum_peaks) {
        applied.threshold_percent = 1.5f * .4f;
        found = detect(detectionX, detectionY, applied, PeakDetectionStage::formal, rejected,
                       sampleOffset, normalizationMaximum);
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
    PeakSearchResult result{std::move(peaks).value(), std::move(candidates).value(), applied, automatic.value()};
    result.rejected_candidates = std::move(rejected);
    return Result<PeakSearchResult>{std::move(result)};
}
Result<InitialPeakResult> ClassicInitialPeakFinder::find(const AnalysisSpectrum &spectrum, WavelengthAngstrom wavelength,
    const InitialPeakOptions &options, const ExecutionContext &context) const {
    if (context.cancellation.stop_requested()) return Result<InitialPeakResult>{Error{ErrorCode::cancelled,"preprocessing","Preprocessing canceled"}};
    auto prepared = detail::prepare_analysis_signal(spectrum);
    if (!prepared) return Result<InitialPeakResult>{prepared.error()};
    return initial_kernel(prepared.value(), wavelength, options, context);
}
Result<FwhmEstimationResult> ClassicFwhmEstimator::estimate(const AnalysisSpectrum &spectrum,
    const InitialPeakResult &initial, const ExecutionContext &context) const {
    if (context.cancellation.stop_requested()) return Result<FwhmEstimationResult>{Error{ErrorCode::cancelled,"preprocessing","Preprocessing canceled"}};
    auto prepared = detail::prepare_analysis_signal(spectrum);
    if (!prepared) return Result<FwhmEstimationResult>{prepared.error()};
    return fwhm_kernel(prepared.value(), initial, context);
}
Result<PeakSearchResult> ClassicPeakExtractor::extract(const AnalysisSpectrum &spectrum, const PeakCalibration &calibration,
    WavelengthAngstrom wavelength, AngularWidthDegrees tolerance, const ExecutionContext &context) const {
    if (context.cancellation.stop_requested()) return Result<PeakSearchResult>{Error{ErrorCode::cancelled,"preprocessing","Preprocessing canceled"}};
    auto prepared = detail::prepare_analysis_signal(spectrum);
    if (!prepared) return Result<PeakSearchResult>{prepared.error()};
    return formal_kernel(prepared.value(), calibration, wavelength, tolerance, context);
}
Result<PeakSearchResult> extract_experimental_peaks(const AnalysisSpectrum &spectrum, const PeakOptions &options,
    WavelengthAngstrom wavelength, AngularWidthDegrees tolerance, const IInitialPeakFinder &initial,
    const IFwhmEstimator &fwhm, const IExperimentalPeakExtractor &extractor, const ExecutionContext &context) {
    return orchestrate_peaks(spectrum.pattern(), options, wavelength, tolerance,
        [&](const Pattern &, auto w, const auto &c) { return initial.find(spectrum, w, c); },
        [&](const Pattern &, const auto &i, const auto &c) { return fwhm.estimate(spectrum, i, c); },
        [&](const Pattern &, const auto &p, auto w, auto t, const auto &c) { return extractor.extract(spectrum, p, w, t, c); }, context);
}
// Retained numerical API for independent frozen preprocessing references. This
// operates on an explicitly prepared buffer and bypasses the product input ports.
Result<PeakSearchResult> find_peaks(const Pattern &pattern, const PeakOptions &options,
    WavelengthAngstrom wavelength, AngularWidthDegrees tolerance) {
    return orchestrate_peaks(pattern, options, wavelength, tolerance,
        [](const Pattern &p, auto w, const auto &c) { return initial_kernel(p, w, InitialPeakOptions{}, c); },
        fwhm_kernel, formal_kernel, ExecutionContext{{}, NumericalPolicy::legacy_mixed_precision, {}});
}
} // namespace qualpf::preprocessing
