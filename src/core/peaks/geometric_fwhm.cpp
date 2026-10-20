#include "qualpf/preprocessing/geometric_fwhm.h"
#include "geometric_fwhm_noise.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace qualpf::preprocessing {
namespace {
constexpr double precision = 64 * std::numeric_limits<double>::epsilon();
using Return = Result<GeometricFwhmResult>;
Error invalid(const char *reason) {
    return {ErrorCode::invalid_input, "fwhm.geometric", reason, reason};
}
Error cancelled() { return {ErrorCode::cancelled, "fwhm.geometric", "FWHM estimation canceled"}; }
bool flag(const std::vector<std::uint8_t> &v, std::size_t i) { return !v.empty() && v[i] != 0; }
bool valid_flags(const std::vector<std::uint8_t> &v, std::size_t n) {
    return v.empty() || (v.size() == n && std::all_of(v.begin(), v.end(), [](auto b) { return b <= 1; }));
}

// Iterative range-minimum tree: O(n) storage, O(log n) inclusive queries.
class RangeMinimum {
    std::size_t count;
    std::vector<double> values;
  public:
    RangeMinimum(const std::vector<double> &z, const ExecutionContext &context) : count(z.size()), values(2 * count) {
        for (std::size_t i = 0; i < count; ++i) {
            if ((i & 1023) == 0 && context.cancellation.stop_requested()) return;
            values[count + i] = z[i];
        }
        for (auto i = count; i > 1; --i) {
            if ((i & 1023) == 0 && context.cancellation.stop_requested()) return;
            values[i - 1] = std::min(values[2 * (i - 1)], values[2 * (i - 1) + 1]);
        }
    }
    double minimum(std::size_t first, std::size_t last) const {
        double v = std::numeric_limits<double>::infinity();
        for (first += count, last += count + 1; first < last; first /= 2, last /= 2) {
            if (first & 1) v = std::min(v, values[first++]);
            if (last & 1) v = std::min(v, values[--last]);
        }
        return v;
    }
};

bool apex(const GeometricFwhmInput &in, const std::vector<double> &z, PeakFwhmDiagnostic &d) {
    const auto p = d.sample_index;
    const double hl = in.two_theta[p].value - in.two_theta[p - 1].value;
    const double hr = in.two_theta[p + 1].value - in.two_theta[p].value;
    const double xs = std::max(hl, hr);
    const double ys = std::max({std::abs(z[p - 1]), std::abs(z[p]), std::abs(z[p + 1])});
    if (!std::isfinite(xs) || !(ys > 0)) return false;
    const double l = hl / xs, r = hr / xs;
    const double zm = z[p - 1] / ys, z0 = z[p] / ys, zp = z[p + 1] / ys;
    const double sl = (z0 - zm) / l, sr = (zp - z0) / r;
    const double a = (sr - sl) / (l + r), b = (r * sl + l * sr) / (l + r);
    if (!std::isfinite(a) || !std::isfinite(b) || a >= -precision) return false;
    const double u = -b / (2 * a);
    const double height = (z0 + b * u / 2) * ys;
    const double mu = in.two_theta[p].value + xs * u;
    if (!std::isfinite(height) || !std::isfinite(mu) || u < -l / 2 - precision ||
        u > r / 2 + precision || !(height / 2 > 0 && height / 2 < z[p])) return false;
    d.apex = TwoThetaDegrees{mu};
    d.apex_offset_degrees = xs * u;
    d.height = height;
    return true;
}

FwhmRejection crossing(const GeometricFwhmInput &in, const std::vector<double> &z,
    const std::vector<bool> &obstacle, std::size_t begin, std::size_t end,
    PeakFwhmDiagnostic &d, bool left, const ExecutionContext &context) {
    const double t = *d.height / 2;
    auto inside = d.sample_index;
    while (left ? inside > begin : inside + 1 < end) {
        if ((inside & 1023) == 0 && context.cancellation.stop_requested()) return FwhmRejection::none;
        const auto outside = left ? inside - 1 : inside + 1;
        if (flag(in.saturated, outside)) return FwhmRejection::saturated;
        if (obstacle[outside]) return FwhmRejection::overlapping_peak;
        if (z[outside] <= t) {
            if (z[outside] == t && (left ? outside > begin : outside + 1 < end) &&
                z[left ? outside - 1 : outside + 1] == t)
                return FwhmRejection::half_height_plateau;
            const auto a = std::min(inside, outside), b = std::max(inside, outside);
            // Scale before subtraction to avoid overflow on large signed samples.
            const double scale = std::max({std::abs(z[a]), std::abs(z[b]), t});
            const double za = z[a] / scale, zb = z[b] / scale;
            const double fraction = (t / scale - za) / (zb - za);
            const double x = std::lerp(in.two_theta[a].value, in.two_theta[b].value, fraction);
            if (!std::isfinite(x) || !std::isfinite(fraction) || fraction < 0 || fraction > 1)
                return FwhmRejection::invalid_width;
            FwhmCrossing c{a, b, TwoThetaDegrees{x}, fraction};
            (left ? d.left : d.right) = c;
            return FwhmRejection::none;
        }
        inside = outside;
    }
    return left ? FwhmRejection::missing_left_crossing : FwhmRejection::missing_right_crossing;
}

void measure(const GeometricFwhmInput &in, const GeometricFwhmOptions &options,
    const std::vector<double> &z, const std::vector<bool> &obstacle,
    std::size_t begin, std::size_t end, PeakFwhmDiagnostic &d, const ExecutionContext &context) {
    const auto reject = [&](FwhmRejection reason) { d.rejection = reason; };
    const auto p = d.sample_index;
    if (d.plateau_last != p) { reject(FwhmRejection::plateau); return; }
    if (flag(in.saturated, p - 1) || flag(in.saturated, p) || flag(in.saturated, p + 1)) {
        reject(FwhmRejection::saturated); return;
    }
    if (!apex(in, z, d)) { reject(FwhmRejection::invalid_apex); return; }
    for (bool left : {true, false}) {
        auto reason = crossing(in, z, obstacle, begin, end, d, left, context);
        if (reason != FwhmRejection::none) { reject(reason); return; }
        if (context.cancellation.stop_requested()) return;
    }
    const double hl = d.apex->value - d.left->position.value;
    const double hr = d.right->position.value - d.apex->value;
    const double width = d.right->position.value - d.left->position.value;
    if (!(hl > 0 && hr > 0 && width > 0) || !std::isfinite(width)) {
        reject(FwhmRejection::invalid_width); return;
    }
    d.width = AngularWidthDegrees{width};
    d.left_width = AngularWidthDegrees{hl}; d.right_width = AngularWidthDegrees{hr};
    d.asymmetry = std::abs(hr - hl) / width;
    double hmax = 0;
    for (auto i = d.left->first; i < d.right->second; ++i) {
        if ((i & 1023) == 0 && context.cancellation.stop_requested()) return;
        hmax = std::max(hmax, in.two_theta[i + 1].value - in.two_theta[i].value);
    }
    const double q = width / hmax;
    d.sampling_density = q;
    d.preferred_sampling = q >= options.preferred_samples_per_fwhm;
    if (q < options.minimum_samples_per_fwhm) { reject(FwhmRejection::undersampled); return; }
    if ((options.minimum_width && width < options.minimum_width->value) ||
        (options.maximum_width && width > options.maximum_width->value)) {
        reject(FwhmRejection::width_out_of_range); return;
    }
    d.eligible = true;
}

bool better(const PeakFwhmDiagnostic &a, const PeakFwhmDiagnostic &b) {
    if (*a.height != *b.height) return *a.height > *b.height;
    if (a.relative_uncertainty != b.relative_uncertainty) {
        if (!a.relative_uncertainty) return false;
        if (!b.relative_uncertainty) return true;
        return *a.relative_uncertainty < *b.relative_uncertainty;
    }
    if (a.prominence != b.prominence) return a.prominence > b.prominence;
    return a.sample_index < b.sample_index;
}
} // namespace

const char *fwhm_rejection_name(FwhmRejection reason) noexcept {
    switch (reason) {
    case FwhmRejection::none: return "none";
    case FwhmRejection::nonpositive_height: return "nonpositive_height";
    case FwhmRejection::insignificant: return "insignificant";
    case FwhmRejection::plateau: return "plateau";
    case FwhmRejection::saturated: return "saturated";
    case FwhmRejection::invalid_apex: return "invalid_apex";
    case FwhmRejection::missing_left_crossing: return "missing_left_crossing";
    case FwhmRejection::missing_right_crossing: return "missing_right_crossing";
    case FwhmRejection::overlapping_peak: return "overlapping_peak";
    case FwhmRejection::half_height_plateau: return "half_height_plateau";
    case FwhmRejection::invalid_width: return "invalid_width";
    case FwhmRejection::undersampled: return "undersampled";
    case FwhmRejection::width_out_of_range: return "width_out_of_range";
    case FwhmRejection::insufficient_snr: return "insufficient_snr";
    }
    return "unknown";
}

Result<GeometricFwhmResult> estimate_geometric_fwhm(const GeometricFwhmInput &in,
    const GeometricFwhmOptions &options, const ExecutionContext &context) {
    if (context.cancellation.stop_requested()) return Return{cancelled()};
    if (context.numerical_policy != NumericalPolicy::explicit_float64) return Return{invalid("float64_policy_required")};
    const auto n = in.two_theta.size();
    if (!in.dataset.value || in.intensity.size() != n || in.background.size() != n ||
        !valid_flags(in.valid, n) || !valid_flags(in.saturated, n) || !valid_flags(in.break_before, n))
        return Return{invalid("invalid_input_shape")};
    if (!std::isfinite(options.minimum_relative_prominence) || options.minimum_relative_prominence < 0 ||
        options.minimum_relative_prominence > 1 || !std::isfinite(options.minimum_samples_per_fwhm) ||
        options.minimum_samples_per_fwhm <= 0 || !std::isfinite(options.preferred_samples_per_fwhm) ||
        options.preferred_samples_per_fwhm < options.minimum_samples_per_fwhm)
        return Return{invalid("invalid_thresholds")};
    if (!std::isfinite(options.minimum_prominence_snr) || options.minimum_prominence_snr < 0 ||
        !std::isfinite(options.minimum_snr) || options.minimum_snr < 0 ||
        !std::isfinite(options.preferred_snr) || options.preferred_snr < options.minimum_snr)
        return Return{invalid("invalid_noise_thresholds")};
    for (auto bound : {options.minimum_width, options.maximum_width})
        if (bound && (!std::isfinite(bound->value) || bound->value <= 0)) return Return{invalid("invalid_width_bounds")};
    if (options.minimum_width && options.maximum_width && options.minimum_width->value > options.maximum_width->value)
        return Return{invalid("invalid_width_bounds")};
    std::vector<double> z(n);
    std::vector<bool> valid(n);
    for (std::size_t i = 0; i < n; ++i) {
        if ((i & 1023) == 0 && context.cancellation.stop_requested()) return Return{cancelled()};
        if (!std::isfinite(in.two_theta[i].value) || (i && !(in.two_theta[i - 1].value < in.two_theta[i].value)))
            return Return{invalid("invalid_grid")};
        z[i] = in.intensity[i] - in.background[i];
        valid[i] = std::isfinite(z[i]) && (in.valid.empty() || flag(in.valid, i));
        if (!valid[i]) z[i] = 0; // Inaccessible outside its segment; never bridged.
    }
    GeometricFwhmResult result;
    auto prepared_noise = geometric_detail::prepare_noise(in, valid, context);
    if (!prepared_noise) return Return{prepared_noise.error()};
    const auto &noise = prepared_noise.value();
    result.noise_model = noise.model; result.noise_source = in.noise_source;
    result.dataset = in.dataset; result.source_id = in.source_id;
    result.radiation = in.radiation; result.processing = in.processing; result.applied = options;
    RangeMinimum minima(z, context);
    if (context.cancellation.stop_requested()) return Return{cancelled()};
    std::vector<std::size_t> lower(n), upper(n), stack;
    std::vector<bool> obstacle(n);
    for (std::size_t begin = 0; begin < n;) {
        if ((begin & 1023) == 0 && context.cancellation.stop_requested()) return Return{cancelled()};
        if (!valid[begin]) { ++begin; continue; }
        auto end = begin + 1;
        while (end < n && valid[end] && !flag(in.break_before, end)) {
            if ((end & 1023) == 0 && context.cancellation.stop_requested()) return Return{cancelled()};
            ++end;
        }
        stack.clear();
        for (auto i = begin; i < end; ++i) {
            if ((i & 1023) == 0 && context.cancellation.stop_requested()) return Return{cancelled()};
            while (!stack.empty() && z[stack.back()] <= z[i]) stack.pop_back();
            lower[i] = stack.empty() ? begin : stack.back(); stack.push_back(i);
        }
        stack.clear();
        for (auto i = end; i-- > begin;) {
            if ((i & 1023) == 0 && context.cancellation.stop_requested()) return Return{cancelled()};
            while (!stack.empty() && z[stack.back()] <= z[i]) stack.pop_back();
            upper[i] = stack.empty() ? end - 1 : stack.back(); stack.push_back(i);
        }
        const auto first_candidate = result.candidates.size();
        for (auto p = begin + 1; p + 1 < end; ++p) {
            if ((p & 1023) == 0 && context.cancellation.stop_requested()) return Return{cancelled()};
            if (z[p] <= z[p - 1]) continue;
            auto last = p;
            while (last + 1 < end && z[last + 1] == z[p]) {
                if ((last & 1023) == 0 && context.cancellation.stop_requested()) return Return{cancelled()};
                ++last;
            }
            if (last + 1 < end && z[last] > z[last + 1]) {
                PeakFwhmDiagnostic d;
                d.sample_index = p; d.plateau_last = last; d.sampled_height = z[p];
                d.prominence = z[p] - std::max(minima.minimum(lower[p], p), minima.minimum(last, upper[last]));
                if (!std::isfinite(d.prominence)) return Return{invalid("prominence_overflow")};
                if (z[p] <= 0) d.rejection = FwhmRejection::nonpositive_height;
                else if (!(d.prominence > 0) || (noise.known() ?
                    d.prominence < options.minimum_prominence_snr * noise.sigma(p - 1, last + 1) :
                    d.prominence / z[p] < options.minimum_relative_prominence))
                    d.rejection = FwhmRejection::insignificant;
                else {
                    d.significant = true;
                    for (auto i = p; i <= last; ++i) obstacle[i] = true;
                }
                result.candidates.push_back(d);
            }
            p = last;
        }
        for (auto i = first_candidate; i < result.candidates.size(); ++i) {
            if (context.cancellation.stop_requested()) return Return{cancelled()};
            auto &d = result.candidates[i];
            if (noise.known()) d.noise_state = FwhmNoiseState::noise_verified;
            if (d.significant) measure(in, options, z, obstacle, begin, end, d, context);
            if (d.width && noise.known()) {
                const auto p = d.sample_index;
                const double sigma = noise.sigma(p - 1, p + 1);
                if (sigma == 0) { d.noise_state = FwhmNoiseState::zero_noise; d.preferred_snr = true; }
                else {
                    const double snr = *d.height / sigma;
                    if (!std::isfinite(snr)) return Return{invalid("snr_overflow")};
                    d.snr = snr; d.preferred_snr = snr >= options.preferred_snr;
                    if (d.eligible && snr < options.minimum_snr) {
                        d.eligible = false; d.rejection = FwhmRejection::insufficient_snr;
                    }
                }
                auto propagated = geometric_detail::propagate(in, z, noise, d);
                if (!propagated) return Return{propagated.error()};
            }
            if (d.eligible && (!result.selected_candidate || better(d, result.candidates[*result.selected_candidate])))
                result.selected_candidate = i;
        }
        begin = end;
    }
    if (context.cancellation.stop_requested()) return Return{cancelled()};
    if (result.selected_candidate) {
        result.state = GeometricFwhmResult::State::measured;
        result.default_fwhm = result.candidates[*result.selected_candidate].width;
    }
    return Return{std::move(result)};
}
} // namespace qualpf::preprocessing
