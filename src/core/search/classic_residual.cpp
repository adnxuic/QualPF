#include "qualpf/search/classic.h"

#include "qualpf/domain/validation.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace qualpf::search {
namespace {
template <class Position>
std::optional<std::size_t> first_nearest(std::size_t size, Position position, double target, double delta) {
    std::optional<std::size_t> best;
    double distance = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < size; ++i) {
        const double d = std::abs(position(i) - target);
        if (d <= delta && d < distance) {
            best = i;
            distance = d;
        }
    }
    return best;
}
} // namespace

Result<std::vector<ResidualAssociation>> associate_residual(const PeakSet &peaks, const ReferenceCard &card,
                                                            double scale, AngularWidthDegrees delta) {
    using Output = Result<std::vector<ResidualAssociation>>;
    auto issues = validate(peaks);
    if (!issues.empty())
        return Output{issues.front()};
    issues = validate(card);
    if (!issues.empty())
        return Output{issues.front()};
    if (!std::isfinite(scale) || scale < 0 || !std::isfinite(delta.value) || delta.value <= 0)
        return Output{Error{ErrorCode::invalid_input, "association.parameters",
                            "Expected nonnegative scale and positive finite delta"}};
    std::vector<double> scaled;
    for (const auto &r : card.reflections) {
        if (!r.two_theta)
            return Output{Error{ErrorCode::missing_data, "reflection.two_theta",
                                "Explicit angular coordinate required"}};
        if (r.intensity < 0 || !std::isfinite(r.intensity * scale))
            return Output{Error{ErrorCode::invalid_input, "reflection.intensity",
                                "Expected finite nonnegative scaled height"}};
        scaled.push_back(r.intensity * scale);
    }
    for (const auto &peak : peaks.peaks)
        if (peak.intensity < 0)
            return Output{Error{ErrorCode::invalid_input, "peak.intensity", "Expected nonnegative height"}};
    std::vector<ResidualAssociation> out(peaks.peaks.size());
    const auto exp_x = [&](std::size_t i) { return peaks.peaks[i].two_theta.value; };
    const auto card_x = [&](std::size_t j) { return card.reflections[j].two_theta->value; };
    for (std::size_t i = 0; i < peaks.peaks.size(); ++i) {
        const auto j = first_nearest(scaled.size(), card_x, exp_x(i), delta.value);
        if (!j)
            continue;
        const auto competitor = first_nearest(peaks.peaks.size(), exp_x, card_x(*j), delta.value);
        if (!competitor)
            continue;
        const auto competing_card = first_nearest(scaled.size(), card_x, exp_x(*competitor), delta.value);
        if (*competitor == i || competing_card != j) {
            const double quality = (1.0 - std::abs(exp_x(i) - card_x(*j)) / delta.value) *
                                   (1.0 - std::abs(peaks.peaks[i].intensity - scaled[*j]) / 1000.0);
            out[i] = {*j, quality};
        }
    }
    return Output{std::move(out)};
}

Result<ResidualResult> subtract_phase(const PeakSet &peaks, const ReferenceCard &card, double scale,
                                      const SearchParameters &parameters) {
    const auto issues = validate(parameters);
    if (!issues.empty())
        return Result<ResidualResult>{issues.front()};
    if (parameters.weight_intensity > 1)
        return Result<ResidualResult>{
            Error{ErrorCode::invalid_input, "search.weight_intensity", "Expected weight in [0,1]"}};
    // The original height is persistent across successive subtractions; never
    // substitute the current residual for a missing original value.
    for (const auto &p : peaks.peaks)
        if (!p.original_intensity || *p.original_intensity < 0)
            return Result<ResidualResult>{Error{ErrorCode::missing_data, "peak.original_intensity",
                                                "Residual search requires original nonnegative heights"}};
    auto associations = associate_residual(peaks, card, scale, parameters.delta_two_theta);
    if (!associations)
        return Result<ResidualResult>{associations.error()};
    ResidualResult out{peaks, std::move(associations).value(), {}};
    out.peaks.peaks.clear();
    const double noise = 0.2 * (1.0 - parameters.weight_intensity);
    for (std::size_t i = 0; i < peaks.peaks.size(); ++i) {
        auto peak = peaks.peaks[i];
        if (out.associations[i].reflection_index) {
            peak.intensity =
                std::max(0.0, peak.intensity -
                                  card.reflections[*out.associations[i].reflection_index].intensity * scale);
            if (*peak.original_intensity > 0 && peak.intensity / *peak.original_intensity <= noise)
                peak.intensity = 0.0;
        }
        if (peak.intensity > 0)
            out.peaks.peaks.push_back(std::move(peak));
        else
            out.removed.push_back(peak.id);
    }
    return Result<ResidualResult>{std::move(out)};
}
} // namespace qualpf::search
