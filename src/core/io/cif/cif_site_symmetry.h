#pragma once
#include "cif_provenance.h"
#include "qualpf/crystallography/structure_factor.h"

namespace qualpf::io::cif {
inline constexpr std::string_view site_symmetry_policy =
    "phase19c1.rounded-fixed-coordinate.v1";

// A failed result carries the same evidence as a successful interpretation.
struct SiteSymmetryResult {
  SiteSymmetryEvidence evidence;
  std::array<double, 3> position;
  std::string error;
};
[[nodiscard]] SiteSymmetryResult
reconcile_site(const crystallography::SpaceGroup &group,
               const SiteProvenance &source, std::array<double, 3> nominal,
               std::array<int, 3> origin_twenty_fourths, bool cartesian,
               std::optional<int> declared_multiplicity,
               std::optional<int> formula_multiplicity, bool springer_profile);

// Exact count inference is deliberately restricted to one ASU row per element.
// Malformed/unsupported formula declarations remain the final validator's job.
[[nodiscard]] std::optional<int>
single_site_formula_multiplicity(const Block &block, int atomic_number,
                                 const std::optional<Value> &occupancy);
} // namespace qualpf::io::cif
