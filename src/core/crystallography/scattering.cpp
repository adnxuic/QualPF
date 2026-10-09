#include "qualpf/crystallography/scattering.h"
#include "rounded_transcendentals.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <string>

namespace qualpf::crystallography {
namespace {
struct SpeciesRecord {
  std::string_view label;
  int z, charge;
  std::array<double, 4> a, b;
  double c, neutron;
};
struct NistPoint {
  double energy, f1, f2;
};
struct NistRecord {
  std::size_t offset, count;
  double frel;
};
#include "scattering_data.inc"

Error error(ErrorCode code, const char *field, const char *reason) {
  return {code, field, reason, reason};
}
bool label_equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  auto lower = [](char c) {
    return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
  };
  return std::equal(a.begin(), a.end(), b.begin(),
                    [&](char x, char y) { return lower(x) == lower(y); });
}
} // namespace

std::string_view AtomicSpecies::label() const noexcept {
  return species_records[index_].label;
}
int AtomicSpecies::atomic_number() const noexcept {
  return species_records[index_].z;
}
int AtomicSpecies::charge() const noexcept {
  return species_records[index_].charge;
}
std::size_t ScatteringRegistry::size() const noexcept {
  return std::size(species_records);
}
Result<AtomicSpecies> ScatteringRegistry::by_index(std::size_t index) const {
  if (index >= size())
    return Result<AtomicSpecies>(
        error(ErrorCode::invalid_input, "species", "unknown_species"));
  return Result<AtomicSpecies>(AtomicSpecies(index));
}
Result<AtomicSpecies> ScatteringRegistry::find(std::string_view label) const {
  for (std::size_t i = 0; i < size(); ++i)
    if (label_equal(label, species_records[i].label))
      return by_index(i);
  return Result<AtomicSpecies>(
      error(ErrorCode::invalid_input, "species", "unknown_species"));
}
Result<ScatteringVariable> ScatteringVariable::from_squared(double value) {
  if (!std::isfinite(value) || value < 0)
    return Result<ScatteringVariable>(error(ErrorCode::invalid_input,
                                            "scattering.s_squared",
                                            "invalid_scattering_variable"));
  return Result<ScatteringVariable>(ScatteringVariable(value));
}
Result<ScatteringVariable> ScatteringVariable::from_s(double value) {
  if (!std::isfinite(value) || value < 0)
    return Result<ScatteringVariable>(error(ErrorCode::invalid_input,
                                            "scattering.s",
                                            "invalid_scattering_variable"));
  return from_squared(value * value);
}
Result<ScatteringVariable> ScatteringVariable::from_d(DSpacingAngstrom d) {
  if (!std::isfinite(d.value) || d.value <= 0)
    return Result<ScatteringVariable>(
        error(ErrorCode::invalid_input, "reflection.d", "invalid_d_spacing"));
  return from_s(0.5 / d.value);
}
Result<bool> validate_radiation(ScatteringRadiation r) {
  if (r.kind != RadiationKind::xray && r.kind != RadiationKind::neutron &&
      r.kind != RadiationKind::electron)
    return Result<bool>(error(ErrorCode::unsupported, "radiation.kind",
                              "unsupported_radiation"));
  if (!std::isfinite(r.wavelength.value) || r.wavelength.value <= 0)
    return Result<bool>(error(ErrorCode::invalid_input, "radiation.wavelength",
                              "invalid_wavelength"));
  if (r.anomalous && r.kind != RadiationKind::xray)
    return Result<bool>(error(ErrorCode::unsupported, "radiation.anomalous",
                              "unsupported_anomalous_radiation"));
  return Result<bool>(true);
}
Result<AnomalousCorrection>
anomalous_correction(AtomicSpecies species, WavelengthAngstrom wavelength) {
  auto valid = validate_radiation({RadiationKind::xray, wavelength, true});
  if (!valid)
    return Result<AnomalousCorrection>(valid.error());
  int z = species.atomic_number();
  if (z < 1 || z > 92)
    return Result<AnomalousCorrection>(error(ErrorCode::unsupported,
                                             "species.anomalous",
                                             "unsupported_anomalous_species"));
  constexpr double conversion =
      1e7 * 6.62607015e-34 * 2.998e8 / 1.602176634e-19;
  const double energy = conversion / wavelength.value;
  const auto &record = nist_records[z - 1];
  const auto *first = nist_points + record.offset;
  const auto *last = first + record.count;
  if (!std::isfinite(energy) || energy < first->energy ||
      energy > (last - 1)->energy)
    return Result<AnomalousCorrection>(error(ErrorCode::unsupported,
                                             "radiation.wavelength",
                                             "wavelength_outside_table"));
  auto upper = std::lower_bound(first, last, energy,
                                [](auto p, double e) { return p.energy < e; });
  double f1 = upper->f1, f2 = upper->f2;
  if (upper != first && upper->energy != energy) {
    const auto &a = *(upper - 1);
    const auto &b = *upper;
    const double frac = (energy - a.energy) / (b.energy - a.energy);
    f1 = a.f1 + frac * (b.f1 - a.f1);
    if (a.f2 == 0 || b.f2 == 0)
      f2 = 0; // explicit retained zero-endpoint rule
    else {
      const double logfrac = (detail::rounded_log10(energy) - detail::rounded_log10(a.energy)) /
                             (detail::rounded_log10(b.energy) - detail::rounded_log10(a.energy));
      f2 = detail::rounded_pow10(detail::rounded_log10(a.f2) +
                             logfrac * (detail::rounded_log10(b.f2) - detail::rounded_log10(a.f2)));
    }
  }
  f1 = f1 - z + record.frel;
  if (!std::isfinite(f1) || !std::isfinite(f2))
    return Result<AnomalousCorrection>(error(ErrorCode::numerical_failure,
                                             "scattering.anomalous",
                                             "nonfinite_scattering_factor"));
  return Result<AnomalousCorrection>(AnomalousCorrection{f1, f2});
}
Result<std::complex<double>>
scattering_amplitude(AtomicSpecies species, ScatteringVariable variable,
                     ScatteringRadiation radiation) {
  auto valid = validate_radiation(radiation);
  if (!valid)
    return Result<std::complex<double>>(valid.error());
  const auto &s = species_records[species.index()];
  const double s2 = variable.squared_inverse_angstrom();
  double f = 0, imag = 0;
  bool available = true;
  if (radiation.kind == RadiationKind::xray) {
    available = s.c != 0 || std::any_of(s.a.begin(), s.a.end(),
                                        [](double a) { return a != 0; });
    f = s.c;
    for (std::size_t i = 0; i < 4; ++i)
      if (s.a[i] != 0)
        f += s.a[i] * detail::rounded_exp(-s.b[i] * s2);
  } else if (radiation.kind == RadiationKind::neutron) {
    available = s.neutron != 0;
    f = s.neutron;
  } else {
    available = s.z <= 103;
    if (available) {
      const auto &p = lobato_records[s.z - 1];
      for (std::size_t i = 0; i < 5; ++i) {
        // Algebraically identical Lobato form; avoids squaring overflow.
        const double reciprocal = 1. / (1. + p[i + 5] * s2);
        f += p[i] * reciprocal * (1. + reciprocal);
      }
    }
  }
  if (!available)
    return Result<std::complex<double>>(error(ErrorCode::unsupported,
                                              "species.scattering",
                                              "unsupported_scattering_entry"));
  if (radiation.anomalous) {
    auto correction = anomalous_correction(species, radiation.wavelength);
    if (!correction)
      return Result<std::complex<double>>(correction.error());
    f += correction.value().real;
    imag = correction.value().imaginary;
  }
  if (!std::isfinite(f) || !std::isfinite(imag))
    return Result<std::complex<double>>(error(ErrorCode::numerical_failure,
                                              "scattering.amplitude",
                                              "nonfinite_scattering_factor"));
  return Result<std::complex<double>>(std::complex<double>{f, imag});
}
} // namespace qualpf::crystallography
