#include "qualpf/search/rir.h"
#include "qualpf/domain/radiation.h"
#include <algorithm>
#include <cmath>
namespace qualpf::search {
std::vector<double> quantify_rir(std::span<const RirInput> phases) {
    std::vector<double> rap(phases.size(), 0.0);
    bool valid = true;
    for (std::size_t i = 0; i < phases.size(); ++i) {
        if (!phases[i].calculated) {
            valid = false;
            break;
        }
        if (!phases[i].rir || *phases[i].rir <= 0.0) {
            valid = false;
            break;
        }
        double maxI = 0.0;
        for (double value : phases[i].intensity)
            maxI = std::max(maxI, value * phases[i].scale);
        rap[i] = maxI / *phases[i].rir;
    }
    double sum = 0.0;
    for (double r : rap)
        sum += r;
    std::vector<double> quant(phases.size(), 0.0);
    if (valid && sum > 0.0)
        for (std::size_t i = 0; i < phases.size(); ++i)
            quant[i] = 100.0 * rap[i] / sum;
    return valid && sum > 0.0 ? quant : std::vector<double>{};
}
Result<double> rir_reference_maximum(std::span<const double> intensity) {
    double maximum = 0;
    for (double value : intensity) {
        if (!std::isfinite(value) || value < 0)
            return Result<double>{Error{ErrorCode::invalid_input, "quantification.reference",
                "Reference intensities must be finite and nonnegative", "invalid_reference_intensity"}};
        maximum = std::max(maximum, value);
    }
    return Result<double>{maximum}; // Empty/zero cards are legal search templates.
}
Result<std::vector<double>> quantify_profile_rir(std::span<const ProfileRirInput> phases) {
    std::vector<double> amounts;
    amounts.reserve(phases.size());
    double sum = 0;
    for (std::size_t i = 0; i < phases.size(); ++i) {
        const auto &p = phases[i];
        const auto invalid = [&](const char *reason, const char *message) {
            return Result<std::vector<double>>{Error{ErrorCode::invalid_input,
                "quantification.phase[" + std::to_string(i) + "]", message, reason}};
        };
        if (!p.rir || !std::isfinite(*p.rir) || *p.rir <= 0)
            return invalid("invalid_rir", "A finite positive RIR is required for every selected phase");
        if (!std::isfinite(p.reference_maximum) || p.reference_maximum <= 0)
            return invalid("invalid_reference_maximum", "A finite positive full-card maximum is required");
        if (!std::isfinite(p.scale) || p.scale < 0)
            return invalid("invalid_scale", "Joint baseline scales must be finite and nonnegative");
        const double amount = (p.scale * p.reference_maximum) / *p.rir;
        if (!std::isfinite(amount) || (p.scale > 0 && amount == 0))
            return invalid("nonfinite_amount", "RIR conversion overflowed or underflowed");
        amounts.push_back(amount);
        sum += amount;
    }
    if (!std::isfinite(sum) || sum <= 0)
        return Result<std::vector<double>>{Error{ErrorCode::invalid_input, "quantification.sum",
            "The sum of RIR-corrected amounts must be finite and positive", "invalid_amount_sum"}};
    for (double &amount : amounts) {
        const double percentage = 100.0 * amount / sum;
        if (!std::isfinite(percentage) || (amount > 0 && percentage == 0))
            return Result<std::vector<double>>{Error{ErrorCode::numerical_failure, "quantification.percentages",
                "Percentage normalization overflowed or underflowed", "nonfinite_percentage"}};
        amount = percentage;
    }
    return Result<std::vector<double>>{std::move(amounts)};
}
bool profile_rir_supports_radiation(const Radiation &r) {
    if ((r.kind && r.kind != RadiationKind::x_ray) || r.lines.empty()) return false;
    bool positive = false;
    for (const auto &line : r.lines) {
        if (!std::isfinite(line.relative_weight) || line.relative_weight < 0) return false;
        if (line.relative_weight == 0) continue;
        positive = true;
        const double wave = line.wavelength.value;
        if (!std::isfinite(wave) ||
            (std::abs(wave - radiation::CU_KA1_WAVE) > 1e-4 &&
             std::abs(wave - radiation::CU_KA2_WAVE) > 1e-4 &&
             std::abs(wave - radiation::CU_KA_WAVE) > 1e-4)) return false;
    }
    return positive;
}
} // namespace qualpf::search
