#pragma once

#include "qualpf/search/classic.h"

#include <initializer_list>

namespace qualpf::testing {
struct ClassicFixture {
    Pattern pattern;
    PeakSet peaks;
    ReferenceCard card;
    SearchParameters parameters;
    ExecutionContext context;
    ClassicFixture() {
        pattern.id = DatasetId{1};
        pattern.two_theta = {TwoThetaDegrees{0},  TwoThetaDegrees{10}, TwoThetaDegrees{20},
                             TwoThetaDegrees{30}, TwoThetaDegrees{40}, TwoThetaDegrees{50},
                             TwoThetaDegrees{60}};
        pattern.observed = {0, 100, 200, 100, 0, 0, 0};
        pattern.calculation_range = AngularRange{TwoThetaDegrees{0}, TwoThetaDegrees{60}};
        peaks.dataset = pattern.id;
        card.database_id = "synthetic";
        card.card_id = "A";
        set_peaks({{10, 100}, {20, 200}, {30, 100}});
        set_card({{10, 10}, {20, 20}, {30, 10}});
        parameters.delta_two_theta = AngularWidthDegrees{0.25};
        parameters.weight_position = parameters.weight_intensity = parameters.weight_phases = 0.5;
        parameters.maximum_entries = 20;
        parameters.include_deleted_cards = true;
        parameters.strongest_count = 3;
        context.numerical_policy = NumericalPolicy::legacy_mixed_precision;
    }
    void set_peaks(std::initializer_list<std::pair<double, double>> values) {
        peaks.peaks.clear();
        for (const auto [x, y] : values) {
            Peak peak;
            peak.id = PeakId{peaks.peaks.size() + 1};
            peak.two_theta = TwoThetaDegrees{x};
            peak.intensity = y;
            peak.original_intensity = y;
            peak.d_spacing = DSpacingAngstrom{100.0 / (1.0 + x)};
            peak.delta_d = DSpacingAngstrom{0.01};
            peak.fwhm = AngularWidthDegrees{0.1};
            peak.integrated_intensity = y * 0.1;
            peaks.peaks.push_back(peak);
        }
    }
    void set_card(std::initializer_list<std::pair<double, double>> values) {
        card.reflections.clear();
        for (const auto [x, y] : values) {
            Reflection r;
            r.id = ReflectionId{card.reflections.size() + 1};
            r.two_theta = TwoThetaDegrees{x};
            r.intensity = y;
            card.reflections.push_back(r);
        }
    }
    ScoreInput input() const & { return {pattern, peaks, card, parameters}; }
    ScoreInput input() const && = delete;
};
} // namespace qualpf::testing
