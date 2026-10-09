#pragma once

#include "qualpf/domain/units.h"

#include <optional>
#include <string>
#include <vector>

namespace qualpf {

struct MillerIndices { int h = 0; int k = 0; int l = 0; };
struct Reflection {
    ReflectionId id; // Stable within this card; never a global counter.
    std::optional<MillerIndices> hkl;
    std::optional<int> multiplicity;
    std::optional<DSpacingAngstrom> d_spacing;
    std::optional<TwoThetaDegrees> two_theta;
    double intensity = 0.0;
    std::optional<double> lorentz_polarization;
    std::optional<double> structure_factor_squared;
};
struct ReferenceCard {
    std::string database_id; // Qualifies card_id across repositories.
    std::string card_id;
    std::string chemical_name;
    std::string chemical_formula;
    std::string mineral_name;
    std::string space_group;
    std::string quality;
    std::optional<double> rir; // Missing is not zero; no RIR computation here.
    std::vector<Reflection> reflections;
    std::optional<WavelengthAngstrom> effective_wavelength;
    IntensityScale intensity_scale = IntensityScale::unspecified;
    SourcePrecision source_precision = SourcePrecision::unknown;
};

} // namespace qualpf
