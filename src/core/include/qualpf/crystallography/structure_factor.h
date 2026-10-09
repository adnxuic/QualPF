#pragma once

#include "qualpf/crystallography/reflection.h"
#include "qualpf/crystallography/scattering.h"
#include <array>
#include <complex>
#include <span>
#include <vector>

namespace qualpf::crystallography {

class FractionalPosition {
public:
  [[nodiscard]] static Result<FractionalPosition>
  create(std::array<double, 3> coordinates);
  [[nodiscard]] const std::array<double, 3> &coordinates() const noexcept {
    return coordinates_;
  }

private:
  explicit FractionalPosition(std::array<double, 3> coordinates)
      : coordinates_(coordinates) {}
  std::array<double, 3> coordinates_;
};
class Occupancy {
public:
  [[nodiscard]] static Result<Occupancy> create(double value);
  [[nodiscard]] double value() const noexcept { return value_; }

private:
  explicit Occupancy(double value) : value_(value) {}
  double value_;
};

// Order 11,22,33,12,13,23. U/B are crystallographic (Angstrom squared),
// NOT Cartesian tensors. beta is dimensionless in exp(-h beta h^T).
using DisplacementTensor = std::array<double, 6>;
class Displacement {
public:
  [[nodiscard]] static Result<Displacement>
  isotropic_b(double angstrom_squared);
  [[nodiscard]] static Result<Displacement>
  isotropic_u(double angstrom_squared);
  [[nodiscard]] static Result<Displacement>
  anisotropic_u(DisplacementTensor angstrom_squared);
  [[nodiscard]] static Result<Displacement>
  anisotropic_b(DisplacementTensor angstrom_squared);
  [[nodiscard]] static Result<Displacement>
  anisotropic_beta(DisplacementTensor dimensionless);
  [[nodiscard]] Result<DisplacementTensor> beta(const UnitCell &cell) const;

private:
  enum class Kind { isotropic_b, crystallographic_u, beta };
  Displacement(Kind kind, DisplacementTensor values)
      : kind_(kind), values_(values) {}
  Kind kind_;
  DisplacementTensor values_;
};

[[nodiscard]] Result<DisplacementTensor>
crystallographic_u_from_beta(DisplacementTensor beta, const UnitCell &cell);
[[nodiscard]] Result<DisplacementTensor>
crystallographic_b_from_beta(DisplacementTensor beta, const UnitCell &cell);
[[nodiscard]] Result<double> equivalent_isotropic_b(DisplacementTensor beta,
                                                    const UnitCell &cell);

struct AtomicSite {
  AtomicSpecies species;
  FractionalPosition position;
  Occupancy occupancy;
  Displacement displacement;
};
struct ExpandedAtomicSite {
  AtomicSpecies species;
  FractionalPosition position;
  Occupancy occupancy;
  DisplacementTensor beta;
};

// Returns unique positions within this one ASU site. Different ASU sites are
// never merged, including mixed species/occupancies on a common position.
// Coincidence is componentwise modulo one within 1e-10 fractional units.
[[nodiscard]] Result<std::vector<ExpandedAtomicSite>>
expand_site(const AtomicSite &site, const UnitCell &cell,
            const SpaceGroup &group);

class CrystalStructure {
public:
  [[nodiscard]] static Result<CrystalStructure>
  create(UnitCell cell, SpaceGroup group, std::vector<AtomicSite> sites);
  [[nodiscard]] const UnitCell &cell() const noexcept { return cell_; }
  [[nodiscard]] const SpaceGroup &group() const noexcept { return group_; }
  [[nodiscard]] std::span<const AtomicSite> sites() const noexcept {
    return sites_;
  }
  [[nodiscard]] std::span<const ExpandedAtomicSite>
  expanded_sites() const noexcept {
    return expanded_;
  }

private:
  CrystalStructure(UnitCell cell, SpaceGroup group,
                   std::vector<AtomicSite> sites,
                   std::vector<ExpandedAtomicSite> expanded)
      : cell_(std::move(cell)), group_(group), sites_(std::move(sites)),
        expanded_(std::move(expanded)) {}
  UnitCell cell_;
  SpaceGroup group_;
  std::vector<AtomicSite> sites_;
  std::vector<ExpandedAtomicSite> expanded_;
};

struct StructureFactor {
  std::complex<double> amplitude; // exp(+2*pi*i*h dot x)
  double fc_squared;              // no multiplicity, LP, scale or profile
};
// Hkl overload evaluates the reciprocal-space coefficient, including indices
// beyond the wavelength's accessible Ewald sphere. Reflection overload also
// validates physical angle/d geometry. Zero hkl is not a diffraction
// reflection.
[[nodiscard]] Result<StructureFactor>
structure_factor(const CrystalStructure &structure, Hkl hkl,
                 ScatteringRadiation radiation);
[[nodiscard]] Result<StructureFactor>
structure_factor(const CrystalStructure &structure,
                 const Reflection &reflection, ScatteringRadiation radiation);
// A ReflectionSet does not carry cell/group ownership in Phase 15. This method
// validates every member's d, angle, class, absence and canonical
// representative against the structure, as well as the set's wavelength. Empty
// sets have no ownership evidence; their wavelength is still checked.
[[nodiscard]] Result<std::vector<StructureFactor>>
structure_factors(const CrystalStructure &structure,
                  const ReflectionSet &reflections,
                  ScatteringRadiation radiation);

} // namespace qualpf::crystallography
