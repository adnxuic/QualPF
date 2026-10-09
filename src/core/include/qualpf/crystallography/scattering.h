#pragma once

#include "qualpf/domain/result.h"
#include "qualpf/domain/units.h"
#include <complex>
#include <cstddef>
#include <string_view>

namespace qualpf::crystallography {

class AtomicSpecies {
public:
  [[nodiscard]] std::string_view label() const noexcept;
  [[nodiscard]] int atomic_number() const noexcept;
  [[nodiscard]] int charge() const noexcept;
  [[nodiscard]] std::size_t index() const noexcept { return index_; }
  bool operator==(const AtomicSpecies &) const = default;

private:
  friend class ScatteringRegistry;
  explicit AtomicSpecies(std::size_t index) : index_(index) {}
  std::size_t index_;
};

// Immutable resource entries; exact ionic spelling (e.g. Fe3+), case
// insensitive. No nearest-charge fallback. Availability is radiation-specific.
class ScatteringRegistry {
public:
  [[nodiscard]] std::size_t size() const noexcept;
  [[nodiscard]] Result<AtomicSpecies> by_index(std::size_t index) const;
  [[nodiscard]] Result<AtomicSpecies> find(std::string_view label) const;
};

enum class RadiationKind { xray, neutron, electron };
struct ScatteringRadiation {
  RadiationKind kind = RadiationKind::xray;
  WavelengthAngstrom wavelength{1.54059307};
  bool anomalous = false; // X-ray NIST only
};

// s=sin(theta)/lambda=1/(2d), never q=2*pi/d. No third Bragg calculation.
class ScatteringVariable {
public:
  [[nodiscard]] static Result<ScatteringVariable>
  from_s(double inverse_angstrom);
  [[nodiscard]] static Result<ScatteringVariable>
  from_squared(double inverse_angstrom_squared);
  [[nodiscard]] static Result<ScatteringVariable> from_d(DSpacingAngstrom d);
  [[nodiscard]] double squared_inverse_angstrom() const noexcept {
    return squared_;
  }

private:
  explicit ScatteringVariable(double squared) : squared_(squared) {}
  double squared_;
};

struct AnomalousCorrection {
  double real, imaginary;
}; // fprime, fdoubleprime
[[nodiscard]] Result<AnomalousCorrection>
anomalous_correction(AtomicSpecies species, WavelengthAngstrom wavelength);
// X-ray units electrons; neutron units 10^-12 cm; electron retained Lobato
// units.
[[nodiscard]] Result<std::complex<double>>
scattering_amplitude(AtomicSpecies species, ScatteringVariable variable,
                     ScatteringRadiation radiation);
[[nodiscard]] Result<bool> validate_radiation(ScatteringRadiation radiation);

} // namespace qualpf::crystallography
