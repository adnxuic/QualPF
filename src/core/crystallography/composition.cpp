#include "qualpf/crystallography/composition.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <map>
#include <numeric>
#include <sstream>
#include <tuple>

namespace qualpf::crystallography {
namespace {
Error invalid(std::string field, std::string reason) {
  return {ErrorCode::invalid_input, std::move(field), {}, std::move(reason)};
}
Error numerical(std::string field, std::string reason) {
  return {
      ErrorCode::numerical_failure, std::move(field), {}, std::move(reason)};
}
bool positive(double x) { return std::isfinite(x) && x > 0; }
struct Sum {
  double value = 0, correction = 0;
  void add(double x) {
    const double next = value + x;
    correction += std::abs(value) >= std::abs(x) ? (value - next) + x
                                                 : (x - next) + value;
    value = next;
  }
  double total() const { return value + correction; }
};
std::string formula_number(double amount) {
  const double integer = std::floor(amount + .5);
  if (integer > 0 && std::abs(integer - amount) <= .05) {
    if (integer == 1)
      return {};
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(0) << integer;
    return out.str();
  }
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::fixed << std::setprecision(2) << amount;
  auto s = out.str();
  while (!s.empty() && s.back() == '0')
    s.pop_back();
  // Legacy s_trim_zeros removes only zeros: a tiny positive amount displays
  // as "0.". The unrounded numeric amount remains in CompositionEntry.
  return s;
}
std::string hill_key(const CompositionEntry &entry) {
  if (entry.element.symbol == "C")
    return "1";
  if (entry.element.symbol == "H" && entry.species.label() != "D")
    return "2";
  return std::string(entry.species.label());
}
} // namespace

Result<CrystalComposition>
CrystalComposition::calculate(const CrystalStructure &structure) {
  ChemistryRegistry registry;
  std::vector<SiteComposition> sites;
  for (const auto &site : structure.sites()) {
    auto images = expand_site(site, structure.cell(), structure.group());
    if (!images)
      return Result<CrystalComposition>(images.error());
    auto representative = images.value().front().position.coordinates();
    for (const auto &image : images.value())
      representative = std::min(representative, image.position.coordinates());
    const double amount =
        static_cast<double>(images.value().size()) * site.occupancy.value();
    if (!std::isfinite(amount))
      return Result<CrystalComposition>(
          numerical("composition.amount", "nonfinite_composition"));
    sites.push_back({site.species, representative, site.occupancy.value(),
                     images.value().size(), amount});
  }
  std::sort(sites.begin(), sites.end(), [](const auto &a, const auto &b) {
    return std::tuple(a.orbit_representative, a.species.index(), a.occupancy,
                      a.multiplicity) < std::tuple(b.orbit_representative,
                                                   b.species.index(),
                                                   b.occupancy, b.multiplicity);
  });
  // The original Z heuristic counts coincident partially occupied sites under
  // their first species. Canonical ordering makes that choice deterministic.
  std::vector<bool> used(sites.size(), false);
  std::map<std::size_t, Sum> heuristic;
  for (std::size_t i = 0; i < sites.size(); ++i) {
    if (used[i])
      continue;
    used[i] = true;
    Sum occupancy;
    occupancy.add(sites[i].occupancy);
    if (sites[i].occupancy < 1) {
      for (std::size_t j = i + 1; j < sites.size(); ++j) {
        if (used[j])
          continue;
        const auto &a = sites[i].orbit_representative,
                   &b = sites[j].orbit_representative;
        if (std::hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]) < .01) {
          occupancy.add(sites[j].occupancy);
          used[j] = true;
        }
      }
    }
    heuristic[sites[i].species.index()].add(
        occupancy.total() * static_cast<double>(sites[i].multiplicity));
  }
  std::uint64_t z = 0;
  for (const auto &[identity, sum] : heuristic) {
    (void)identity;
    const double amount = sum.total(), integer = std::floor(amount + .5);
    if (!std::isfinite(amount) ||
        integer >=
            static_cast<double>(std::numeric_limits<std::uint64_t>::max()))
      return Result<CrystalComposition>(
          numerical("composition.formula_units", "invalid_formula_units"));
    if (integer > 0 && std::abs(integer - amount) <= .1)
      z = std::gcd(z, static_cast<std::uint64_t>(integer));
  }
  if (z == 0)
    z = 1;
  std::map<std::size_t, Sum> amounts;
  for (const auto &site : sites)
    amounts[site.species.index()].add(site.unit_cell_amount);
  std::vector<CompositionEntry> entries;
  Sum total;
  for (const auto &[index, sum] : amounts) {
    // Index was obtained from an immutable AtomicSpecies above.
    const auto species = ScatteringRegistry{}.by_index(index).value();
    auto mass = registry.atomic_mass(species);
    if (!mass)
      return Result<CrystalComposition>(mass.error());
    const double amount = sum.total(), cell_mass = amount * mass.value();
    if (!std::isfinite(amount) || amount < 0 || !std::isfinite(cell_mass))
      return Result<CrystalComposition>(
          numerical("composition.cell_mass", "invalid_cell_mass"));
    entries.push_back({species, registry.element(species), amount,
                       amount / static_cast<double>(z), mass.value(),
                       cell_mass});
    total.add(cell_mass);
  }
  const double cell_mass = total.total(),
               formula_mass = cell_mass / static_cast<double>(z);
  if (!positive(cell_mass))
    return Result<CrystalComposition>(
        invalid("composition.cell_mass", "invalid_composition"));
  if (!positive(formula_mass))
    return Result<CrystalComposition>(
        numerical("composition.formula_mass", "invalid_formula_mass"));
  auto display_entries = entries;
  std::sort(display_entries.begin(), display_entries.end(),
            [](const auto &a, const auto &b) {
              return std::tuple(hill_key(a), a.species.index()) <
                     std::tuple(hill_key(b), b.species.index());
            });
  std::string display;
  for (const auto &entry : display_entries) {
    if (entry.formula_amount == 0)
      continue;
    if (!display.empty())
      display += ' ';
    // D is a distinct isotope label; other charge suffixes are display-only.
    display += entry.species.label() == "D" ? "D" : entry.element.symbol;
    display += formula_number(entry.formula_amount);
  }
  return Result<CrystalComposition>(CrystalComposition(
      std::move(sites), std::move(entries),
      ChemicalFormula{z, std::move(display), formula_mass}, cell_mass));
}
Result<CrystalDensity> crystal_density(double mass, double volume) {
  if (!positive(mass))
    return Result<CrystalDensity>(
        invalid("density.cell_mass", "invalid_cell_mass"));
  if (!positive(volume))
    return Result<CrystalDensity>(
        invalid("density.volume", "invalid_cell_volume"));
  const double density = (1.660543780 * mass) / volume;
  if (!positive(density))
    return Result<CrystalDensity>(
        numerical("density.value", "nonfinite_density"));
  return Result<CrystalDensity>(CrystalDensity{mass, volume, density});
}
Result<CrystalAbsorption>
crystal_absorption(const CrystalComposition &composition,
                   const CrystalDensity &density,
                   ScatteringRadiation radiation) {
  if (radiation.kind != RadiationKind::xray)
    return Result<CrystalAbsorption>(Error{ErrorCode::unsupported,
                                           "absorption.radiation",
                                           {},
                                           "unsupported_radiation"});
  auto energy = attenuation_energy_kev(radiation.wavelength);
  if (!energy)
    return Result<CrystalAbsorption>(energy.error());
  auto checked =
      crystal_density(density.cell_mass_daltons, density.volume_angstrom_cubed);
  if (!checked)
    return Result<CrystalAbsorption>(checked.error());
  if (density.cell_mass_daltons != composition.cell_mass_daltons() ||
      density.grams_per_cm3 != checked.value().grams_per_cm3)
    return Result<CrystalAbsorption>(
        invalid("absorption.density", "incompatible_density"));
  ChemistryRegistry registry;
  std::vector<SpeciesAbsorption> species;
  Sum weighted;
  for (const auto &entry : composition.entries()) {
    if (entry.cell_mass_daltons == 0)
      continue;
    auto mac = registry.mass_attenuation(entry.element.atomic_number,
                                         radiation.wavelength);
    if (!mac)
      return Result<CrystalAbsorption>(mac.error());
    const double contribution = entry.cell_mass_daltons * mac.value().cm2_per_g;
    if (!std::isfinite(contribution))
      return Result<CrystalAbsorption>(
          numerical("absorption.weighted_sum", "nonfinite_absorption"));
    weighted.add(contribution);
    species.push_back(
        {entry.species, entry.cell_mass_daltons, mac.value(), contribution});
  }
  const double mixture = weighted.total() / composition.cell_mass_daltons();
  const double linear = density.grams_per_cm3 * mixture;
  if (!positive(mixture) || !positive(linear))
    return Result<CrystalAbsorption>(
        numerical("absorption.value", "nonpositive_or_nonfinite_absorption"));
  return Result<CrystalAbsorption>(
      CrystalAbsorption{energy.value(), mixture, linear, std::move(species)});
}
} // namespace qualpf::crystallography
