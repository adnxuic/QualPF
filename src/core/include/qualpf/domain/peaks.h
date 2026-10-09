#pragma once

#include "qualpf/domain/units.h"

#include <optional>
#include <vector>

namespace qualpf {

struct Peak {
    PeakId id; // Stable within a dataset; not a GUI row number.
    TwoThetaDegrees two_theta;
    std::optional<DSpacingAngstrom> d_spacing;
    std::optional<DSpacingAngstrom> delta_d;
    double intensity = 0.0; // Current/residual height in the declared scale.
    std::optional<double> original_intensity;
    std::optional<AngularWidthDegrees> fwhm;
    std::optional<double> integrated_intensity; // Height-scale * degrees when known.
};
struct PeakSet {
    DatasetId dataset;
    std::vector<Peak> peaks; // Preserve source order; validation never sorts or merges.
    std::optional<WavelengthAngstrom> effective_wavelength;
    IntensityScale intensity_scale = IntensityScale::unspecified;
    SourcePrecision source_precision = SourcePrecision::unknown;
};

} // namespace qualpf
