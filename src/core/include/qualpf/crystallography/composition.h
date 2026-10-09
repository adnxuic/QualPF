#pragma once

#include "qualpf/crystallography/chemistry.h"
#include "qualpf/crystallography/structure_factor.h"
#include <cstdint>
#include <string>

namespace qualpf::crystallography {

struct SiteComposition {
  AtomicSpecies species;
  std::array<double, 3> orbit_representative;
  double occupancy;
  std::size_t multiplicity;
  double unit_cell_amount;
};
struct CompositionEntry {
  AtomicSpecies species; // Ions are distinct, including in numeric formula.
  ElementIdentity element;
  double unit_cell_amount;
  double formula_amount;
  double atomic_mass_daltons;
  double cell_mass_daltons;
};
struct ChemicalFormula {
  std::uint64_t
      formula_units_per_cell; // Legacy Z display-normalization heuristic.
  std::string display; // Charge hidden, no merging of equal visible tokens.
  double mass_daltons;
};

class CrystalComposition {
public:
  [[nodiscard]] static Result<CrystalComposition>
  calculate(const CrystalStructure &structure);
  [[nodiscard]] std::span<const SiteComposition> sites() const noexcept {
    return sites_;
  }
  [[nodiscard]] std::span<const CompositionEntry> entries() const noexcept {
    return entries_;
  }
  [[nodiscard]] const ChemicalFormula &formula() const noexcept {
    return formula_;
  }
  [[nodiscard]] double cell_mass_daltons() const noexcept { return cell_mass_; }

private:
  CrystalComposition(std::vector<SiteComposition> sites,
                     std::vector<CompositionEntry> entries,
                     ChemicalFormula formula, double cell_mass)
      : sites_(std::move(sites)), entries_(std::move(entries)),
        formula_(std::move(formula)), cell_mass_(cell_mass) {}
  std::vector<SiteComposition> sites_;
  std::vector<CompositionEntry> entries_;
  ChemicalFormula formula_;
  double cell_mass_;
};

struct CrystalDensity {
  double cell_mass_daltons;
  double volume_angstrom_cubed;
  double grams_per_cm3;
};
[[nodiscard]] Result<CrystalDensity>
crystal_density(double cell_mass_daltons, double volume_angstrom_cubed);

struct SpeciesAbsorption {
  AtomicSpecies species;
  double cell_mass_daltons;
  MassAttenuation attenuation;
  double weighted_attenuation;
};
struct CrystalAbsorption {
  double energy_kev;
  double mass_attenuation_cm2_per_g;
  double linear_absorption_per_cm;
  std::vector<SpeciesAbsorption> species; // Only positive mass contributions.
};
[[nodiscard]] Result<CrystalAbsorption>
crystal_absorption(const CrystalComposition &composition,
                   const CrystalDensity &density,
                   ScatteringRadiation radiation);

} // namespace qualpf::crystallography
