#include "qualpf/crystallography/chemistry.h"
#include "rounded_transcendentals.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace qualpf::crystallography {
namespace {
struct ChemistryRecord {
  std::string_view label;
  int z;
  double mass;
};
struct AttenuationTable {
  std::size_t offset, count;
};
#include "chemistry_data.inc"

Error error(std::string field, std::string reason,
            ErrorCode code = ErrorCode::invalid_input) {
  return {code, std::move(field), {}, std::move(reason)};
}
bool positive(double x) { return std::isfinite(x) && x > 0; }
} // namespace

Result<double> ChemistryRegistry::atomic_mass(AtomicSpecies species) const {
  const auto &r = chemistry_records[species.index()];
  if (r.label != species.label() || r.z != species.atomic_number() ||
      !positive(r.mass))
    return Result<double>(error("composition.atomic_mass",
                                "unsupported_atomic_mass",
                                ErrorCode::unsupported));
  return Result<double>(r.mass);
}
Result<double> ChemistryRegistry::atomic_mass(std::string_view label) const {
  auto s = ScatteringRegistry{}.find(label);
  if (!s)
    return Result<double>(error("composition.atomic_mass",
                                "unsupported_atomic_mass",
                                ErrorCode::unsupported));
  return atomic_mass(s.value());
}
ElementIdentity
ChemistryRegistry::element(AtomicSpecies species) const noexcept {
  // XEN's first 111 records are the neutral elements in atomic-number order.
  const auto &r =
      chemistry_records[static_cast<std::size_t>(species.atomic_number() - 1)];
  return {r.z, r.label};
}
Result<std::array<double, 2>>
ChemistryRegistry::attenuation_energy_range(int z) const {
  if (z < 1 || z > static_cast<int>(std::size(attenuation_tables)))
    return Result<std::array<double, 2>>(error("absorption.atomic_number",
                                               "unsupported_absorption_data",
                                               ErrorCode::unsupported));
  const auto table = attenuation_tables[z - 1];
  return Result<std::array<double, 2>>(std::array<double, 2>{
      attenuation_points[table.offset].energy_kev,
      attenuation_points[table.offset + table.count - 1].energy_kev});
}
Result<double> attenuation_energy_kev(WavelengthAngstrom wavelength) {
  if (!positive(wavelength.value))
    return Result<double>(error("absorption.wavelength", "invalid_wavelength"));
  constexpr double conversion =
      (1e7) * 6.62607015e-34 * 2.998e8 / 1.602176634e-19;
  const double energy = conversion / wavelength.value;
  if (!positive(energy))
    return Result<double>(error("absorption.energy", "nonfinite_energy",
                                ErrorCode::numerical_failure));
  return Result<double>(energy);
}
Result<MassAttenuation> interpolate_mass_attenuation(AttenuationPoint a,
                                                     AttenuationPoint b,
                                                     double energy) {
  if (!positive(a.energy_kev) || !positive(b.energy_kev) ||
      a.energy_kev >= b.energy_kev ||
      !std::isfinite(a.mass_attenuation_cm2_per_g) ||
      a.mass_attenuation_cm2_per_g < 0 ||
      !std::isfinite(b.mass_attenuation_cm2_per_g) ||
      b.mass_attenuation_cm2_per_g < 0 || !positive(energy))
    return Result<MassAttenuation>(
        error("absorption.interpolation", "invalid_attenuation_interpolation"));
  if (energy < a.energy_kev || energy > b.energy_kev)
    return Result<MassAttenuation>(error("absorption.energy",
                                         "attenuation_out_of_range",
                                         ErrorCode::unsupported));
  double fraction = 0, mac = a.mass_attenuation_cm2_per_g;
  if (energy == b.energy_kev) {
    fraction = 1;
    mac = b.mass_attenuation_cm2_per_g;
  } else if (energy != a.energy_kev) {
    fraction = (detail::rounded_log10(energy) - detail::rounded_log10(a.energy_kev)) /
               (detail::rounded_log10(b.energy_kev) - detail::rounded_log10(a.energy_kev));
    if (a.mass_attenuation_cm2_per_g == 0 || b.mass_attenuation_cm2_per_g == 0)
      mac = 0;
    else
      mac = detail::rounded_pow10(
                     detail::rounded_log10(a.mass_attenuation_cm2_per_g) +
                         fraction * (detail::rounded_log10(b.mass_attenuation_cm2_per_g) -
                                     detail::rounded_log10(a.mass_attenuation_cm2_per_g)));
  }
  if (!std::isfinite(fraction) || !std::isfinite(mac) || mac < 0)
    return Result<MassAttenuation>(error("absorption.interpolation",
                                         "nonfinite_attenuation",
                                         ErrorCode::numerical_failure));
  return Result<MassAttenuation>(MassAttenuation{energy, a, b, fraction, mac});
}
Result<MassAttenuation>
ChemistryRegistry::mass_attenuation(int z,
                                    WavelengthAngstrom wavelength) const {
  auto range = attenuation_energy_range(z);
  if (!range)
    return Result<MassAttenuation>(range.error());
  auto energy = attenuation_energy_kev(wavelength);
  if (!energy)
    return Result<MassAttenuation>(energy.error());
  if (energy.value() < range.value()[0] || energy.value() > range.value()[1])
    return Result<MassAttenuation>(error("absorption.energy",
                                         "attenuation_out_of_range",
                                         ErrorCode::unsupported));
  const auto table = attenuation_tables[z - 1];
  const auto *begin = attenuation_points + table.offset,
             *end = begin + table.count;
  auto upper =
      std::upper_bound(begin, end, energy.value(), [](double x, const auto &p) {
        return x < p.energy_kev;
      });
  if (upper == end)
    --upper;
  return interpolate_mass_attenuation(*(upper - 1), *upper, energy.value());
}
} // namespace qualpf::crystallography
