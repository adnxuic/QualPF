#include "qualpf/preprocessing/kalpha2.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace qualpf::preprocessing {
namespace {
constexpr std::array<float, 3> levers{1.002353500f, 1.002453600f, 1.002578830f};
constexpr std::array<float, 3> weights{-.152766460f, -.268687600f, -.110173100f};
constexpr float pi = 3.1415927410125732421875f;
constexpr float rtod = 180.f / pi;
constexpr float dtor = 1.f / rtod;
// Round the transcendental result to binary32 explicitly. MSVC's float
// intrinsics differ from the captured MinGW runtime by one ulp; that error
// is amplified by narrow-bin interpolation. Binary64 evaluation avoids it.
float sine32(float x) { return float(std::sin(double(x))); }
float asin32(float x) { return float(std::asin(double(x))); }
Error invalid(const char *reason, const char *message, ErrorCode code = ErrorCode::invalid_input) {
    return {code, "pattern.kalpha2", message, reason};
}
bool cu(const Radiation &r) {
    if (r.kind != RadiationKind::x_ray || r.lines.empty() || r.lines.size() > 2)
        return false;
    for (const auto &line : r.lines)
        if (!std::isfinite(line.wavelength.value) || !std::isfinite(line.relative_weight) ||
            line.relative_weight <= 0)
            return false;
    // Explicit identification ranges include conventional rounded Cu values.
    const auto a = r.lines[0].wavelength.value;
    if (r.lines.size() == 1)
        return a >= 1.5405 && a <= 1.54195;
    const auto b = r.lines[1].wavelength.value;
    return a >= 1.5405 && a <= 1.5407 && b >= 1.5443 && b <= 1.5445 &&
           std::abs(r.lines[1].relative_weight / r.lines[0].relative_weight - .5) <= 1e-6;
}
} // namespace
Status validate_kalpha2(const Pattern &p) {
    if (!p.processing.kalpha2_subtracted)
        return Status{invalid("kalpha2_state_unknown",
                              "Stripping history is unknown. Import the original file.",
                              ErrorCode::missing_data)};
    if (*p.processing.kalpha2_subtracted)
        return Status{invalid("kalpha2_already_subtracted", "K-alpha2 has already been stripped.")};
    if (!cu(p.radiation))
        return Status{invalid("unsupported_kalpha2_radiation",
                              "This compatibility model requires known Cu X-ray radiation.",
                              ErrorCode::unsupported)};
    if (p.observed.size() < 2 || p.observed.size() != p.two_theta.size())
        return Status{invalid("kalpha2_invalid_grid", "At least two aligned samples are required.")};
    float previous = -1.f;
    for (std::size_t i = 0; i < p.observed.size(); ++i) {
        if (!std::isfinite(p.two_theta[i].value) || !std::isfinite(p.observed[i]) ||
            std::abs(p.two_theta[i].value) > std::numeric_limits<float>::max() ||
            std::abs(p.observed[i]) > std::numeric_limits<float>::max())
            return Status{invalid("kalpha2_nonfinite_input", "Samples must be finite in binary32.")};
        const float x = float(p.two_theta[i].value), y = float(p.observed[i]);
        if (!std::isfinite(x) || !std::isfinite(y))
            return Status{invalid("kalpha2_nonfinite_input", "Samples must be finite in binary32.")};
        if (x <= previous)
            return Status{
                invalid("kalpha2_invalid_grid", "Angles must remain strictly increasing in binary32.")};
        if (x < 0 || x >= 180 || sine32(.5f * x * dtor) * levers.back() > 1.f)
            return Status{
                invalid("kalpha2_angular_domain", "Angle is outside the Cu compatibility model domain.")};
        previous = x;
    }
    return Status{std::monostate{}};
}
Result<std::vector<double>> strip_kalpha2(const Pattern &p) {
    auto valid = validate_kalpha2(p);
    if (!valid)
        return Result<std::vector<double>>{valid.error()};
    std::vector<float> x, y;
    x.reserve(p.observed.size());
    y.reserve(p.observed.size());
    for (std::size_t i = 0; i < p.observed.size(); ++i) {
        x.push_back(float(p.two_theta[i].value));
        y.push_back(float(p.observed[i]));
    }
    const float minimum = *std::min_element(y.begin(), y.end());
    for (auto &v : y)
        v -= minimum;
    bool exhausted = false;
    for (std::size_t i = 0; i < y.size() && !exhausted; ++i) {
        const float count = y[i];
        if (count < 0)
            continue;
        const float sine = sine32(.5f * x[i] * dtor);
        std::size_t upper = i + 1;
        for (std::size_t j = 0; j < levers.size(); ++j) {
            const float angle = asin32(sine * levers[j]) * rtod;
            const float amount = weights[j] * count;
            // The captured model stops the complete sweep when the upper
            // bracket reaches the last bin. Preserve that boundary convention.
            while (upper < y.size() - 1 && .5f * x[upper] <= angle)
                ++upper;
            if (upper >= y.size() - 1) {
                exhausted = true;
                break;
            }
            const float hi = .5f * x[upper], lo = .5f * x[upper - 1];
            const float step = hi - lo;
            y[upper - 1] += (hi - angle) / step * amount;
            y[upper] += (angle - lo) / step * amount;
        }
    }
    std::vector<double> output;
    output.reserve(y.size());
    for (float v : y) {
        v += minimum;
        if (!std::isfinite(v))
            return Result<std::vector<double>>{invalid(
                "kalpha2_nonfinite_result", "Stripping overflowed binary32.", ErrorCode::numerical_failure)};
        output.push_back(v);
    }
    return Result<std::vector<double>>{std::move(output)};
}
} // namespace qualpf::preprocessing
