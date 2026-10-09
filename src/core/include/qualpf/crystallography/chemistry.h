#pragma once

#include "qualpf/crystallography/scattering.h"
#include <array>

namespace qualpf::crystallography {

struct ElementIdentity {
  int atomic_number;
  std::string_view
      symbol; // Neutral element; D retains species identity but Z=1.
};
struct AttenuationPoint {
  double energy_kev;
  double mass_attenuation_cm2_per_g;
};
struct MassAttenuation {
  double energy_kev;
  AttenuationPoint lower;
  AttenuationPoint upper;
  double log_fraction;
  double cm2_per_g;
};

// Checked numerical kernel, also used by the immutable registry. The closed
// interval and zero-endpoint rule are the retained NIST interpolation contract.
[[nodiscard]] Result<MassAttenuation>
interpolate_mass_attenuation(AttenuationPoint lower, AttenuationPoint upper,
                             double energy_kev);

class ChemistryRegistry {
public:
  [[nodiscard]] Result<double> atomic_mass(AtomicSpecies species) const;
  [[nodiscard]] Result<double>
  atomic_mass(std::string_view species_label) const;
  [[nodiscard]] ElementIdentity element(AtomicSpecies species) const noexcept;
  [[nodiscard]] Result<std::array<double, 2>>
  attenuation_energy_range(int atomic_number) const;
  [[nodiscard]] Result<MassAttenuation>
  mass_attenuation(int atomic_number, WavelengthAngstrom wavelength) const;
};

[[nodiscard]] Result<double>
attenuation_energy_kev(WavelengthAngstrom wavelength);

} // namespace qualpf::crystallography
