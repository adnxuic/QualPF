#pragma once

#include "qualpf/crystallography/structure_factor.h"
#include <cstdint>
#include <span>
#include <vector>

namespace qualpf::crystallography {

// Stable within a set, independent of its current storage order.
using ReflectionIdentity = std::uint64_t;
struct PowderCorrection {
  double lorentz;
  double polarization; // 1 + 0.8*cos(2theta)^2, or 1 for neutron/synchrotron
  double combined;
};
struct PowderOverlapOptions {
  double tolerance_degrees = 0.00005;
};
struct PowderIntensityOptions {
  ScatteringRadiation radiation;
  // Retained Legacy sync flag: polarization term becomes one.
  bool synchrotron = false;
  PowderOverlapOptions overlap;
};
struct PowderReflection {
  ReflectionIdentity identity;
  Reflection reflection;
  double fc_squared;
  PowderCorrection correction;
  double raw_intensity;
};
struct PowderLine {
  ReflectionIdentity representative_identity;
  Reflection representative; // first member's hkl/d/angle/multiplicity
  double raw_intensity;
  double relative_intensity; // maximum = 1000; no DB rounding/filtering
  std::vector<PowderReflection> members;
};
class PowderLineSet {
public:
  [[nodiscard]] std::span<const PowderLine> lines() const noexcept {
    return lines_;
  }

private:
  friend Result<PowderLineSet> normalize_powder_lines(std::vector<PowderLine>);
  explicit PowderLineSet(std::vector<PowderLine> lines)
      : lines_(std::move(lines)) {}
  std::vector<PowderLine> lines_;
};

struct PowderStructureFactor {
  ReflectionIdentity identity;
  Reflection reflection;
  StructureFactor factor;
};
// A value relation created only by Phase 16 computation on a validated Phase 15
// set. No caller-supplied parallel Fc2 vector, pointer identity or mutable
// cache.
class PowderStructureFactors {
public:
  [[nodiscard]] static Result<PowderStructureFactors>
  calculate(const CrystalStructure &structure, const ReflectionSet &reflections,
            ScatteringRadiation radiation);
  [[nodiscard]] std::span<const PowderStructureFactor> values() const noexcept {
    return values_;
  }
  [[nodiscard]] const UnitCellParameters &cell() const noexcept {
    return cell_;
  }
  [[nodiscard]] int group_setting_id() const noexcept { return group_id_; }
  [[nodiscard]] ScatteringRadiation radiation() const noexcept {
    return radiation_;
  }
  [[nodiscard]] FriedelPolicy friedel_policy() const noexcept {
    return friedel_;
  }

private:
  PowderStructureFactors(UnitCellParameters cell, int group_id,
                         ScatteringRadiation radiation, FriedelPolicy friedel,
                         std::vector<PowderStructureFactor> values)
      : cell_(cell), group_id_(group_id), radiation_(radiation),
        friedel_(friedel), values_(std::move(values)) {}
  UnitCellParameters cell_;
  int group_id_;
  ScatteringRadiation radiation_;
  FriedelPolicy friedel_;
  std::vector<PowderStructureFactor>
      values_; // hkl order, identity assigned here
};

// Scalar numerical primitives. They do not assert a structure/cell/group owner.
// Electron intentionally uses Legacy's X-ray-like LP branch (see contracts).
[[nodiscard]] Result<PowderCorrection>
powder_correction(TwoThetaDegrees angle, const PowderIntensityOptions &options);
[[nodiscard]] Result<PowderReflection>
powder_reflection(ReflectionIdentity identity, Reflection reflection,
                  double fc_squared, const PowderIntensityOptions &options);
// Adjacent transitive groups after (angle, stable identity) ordering. No
// centroid.
[[nodiscard]] Result<std::vector<PowderLine>>
merge_powder_reflections(std::span<const PowderReflection> reflections,
                         PowderOverlapOptions options = {});
[[nodiscard]] Result<PowderLineSet>
normalize_powder_lines(std::vector<PowderLine> lines);

// Pairing is by full reflection identity/geometry, never vector position.
// Radiation (including anomalous flag), wavelength and Friedel policy must
// match.
[[nodiscard]] Result<PowderLineSet>
powder_lines(const ReflectionSet &reflections,
             const PowderStructureFactors &factors,
             const PowderIntensityOptions &options);
[[nodiscard]] Result<PowderLineSet>
powder_lines(const CrystalStructure &structure,
             const ReflectionSet &reflections,
             const PowderIntensityOptions &options);

} // namespace qualpf::crystallography
