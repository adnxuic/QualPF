#pragma once

#include "qualpf/domain/units.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace qualpf {

enum class RadiationKind { x_ray, neutron, electron };
struct RadiationLine {
    WavelengthAngstrom wavelength;
    double relative_weight = 0.0; // Kept as supplied, never normalized on import.
};
struct Radiation {
    std::optional<RadiationKind> kind; // Unknown legacy integer codes are not guessed.
    std::vector<RadiationLine> lines;
};

struct SampleWindow {
    std::size_t first = 0; // Zero-based offset in Pattern::two_theta.
};
struct SampledSignal {
    std::vector<double> values;
    std::optional<SampleWindow> window; // Unknown is distinct from full-grid alignment.
};
struct ProcessingState {
    std::optional<bool> background_subtracted;
    std::optional<bool> kalpha2_subtracted;
    std::optional<bool> intensity_scaled;
};

// Owning value. "observed" is the supplied working spectrum, not a promise of raw data.
struct Pattern {
    DatasetId id;
    std::string source_id; // Opaque UTF-8 identity; filesystem/encoding work stays outside domain values.
    std::vector<TwoThetaDegrees> two_theta;
    std::vector<double> observed;
    std::optional<std::vector<TwoThetaDegrees>> corrected_two_theta;
    std::optional<std::vector<double>> original_observed;
    std::optional<bool> original_kalpha2_subtracted; // Provenance of original_observed, not inferred from its existence.
    std::optional<SampledSignal> background;
    std::optional<SampledSignal> calculated;
    std::optional<SampledSignal> background_subtracted_signal;
    std::optional<std::vector<double>> weights;
    Radiation radiation;
    std::optional<AngularRange> calculation_range; // Explicit dataset tmin/tmax, not inferred.
    ProcessingState processing;
    IntensityScale intensity_scale = IntensityScale::unspecified;
    SourcePrecision source_precision = SourcePrecision::unknown;
};

struct PatternView {
    std::span<const TwoThetaDegrees> two_theta;
    std::span<const double> observed;
};
// Views are synchronous borrows; mutation/reallocation/destruction invalidates them.
inline PatternView view_of(const Pattern& pattern) noexcept
{
    return {pattern.two_theta, pattern.observed};
}
PatternView view_of(Pattern&&) = delete;
PatternView view_of(const Pattern&&) = delete;

} // namespace qualpf
