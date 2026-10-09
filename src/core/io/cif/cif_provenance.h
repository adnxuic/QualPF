#pragma once
#include "cif_document.h"
#include <array>
#include <map>

namespace qualpf::io::cif {
// Original physical quantity; unknown never means zero or a card default.
enum class SourceDisplacement {
  unknown,
  isotropic_b,
  isotropic_u,
  anisotropic_b,
  anisotropic_u,
  anisotropic_beta
};
[[nodiscard]] std::string_view
displacement_name(SourceDisplacement value) noexcept;
struct CoordinateEvidence {
  std::string nominal;
  std::string standard_uncertainty; // empty means absent, never zero by default
  int decimal_places = 0;
  int exponent = 0;
  std::string quantum;
  std::string interval_lower;
  std::string interval_upper;
};
struct SiteSymmetryEvidence {
  std::string policy_version;
  std::string state;
  std::string reason;
  std::string constraint_source;
  std::string naming_source;
  std::optional<int> constraint_multiplicity;
  std::string normalized_wyckoff;
  std::array<CoordinateEvidence, 3> coordinates;
  std::array<std::string, 3> nominal_fractional;
  std::array<std::string, 3> reconciled_fractional;
  std::array<std::string, 3> delta;
  std::size_t nominal_orbit = 0;
  std::size_t reconciled_orbit = 0;
  std::vector<std::size_t> nominal_stabilizer;
  std::vector<std::size_t> reconciled_stabilizer;
  std::size_t supported_candidates = 0;
};
struct SiteProvenance {
  std::size_t coordinate_row = 0; // one based in the selected block's loop
  std::string label;
  std::string
      occupancy_source; // explicit, unknown_default_one, absent_default_one
  std::optional<Value> occupancy;
  SourceDisplacement displacement = SourceDisplacement::unknown;
  std::map<std::string, Value> displacement_values;
  std::optional<std::size_t> isotropic_row;
  std::optional<std::size_t> anisotropic_row;
  std::array<Value, 3> coordinates;
  std::map<std::string, Value> site_symmetry_declarations;
  SiteSymmetryEvidence site_symmetry;
};
// Source evidence remains above; this is a separately selected calculation
// model. Neither its value nor its failure replaces the original tensor.
struct DisplacementChoice {
  std::string policy_version;
  std::string site;
  std::string source_field;
  std::string unit;
  Value source_value;
  std::string strict_field;
  std::string strict_reason;
  std::string strict_message;
  std::string certificate;
};
enum class FormulaZState {
  consistent,
  metadata_discrepancy,
  inconsistent,
  insufficient_metadata
};
[[nodiscard]] std::string_view
formula_z_state_name(FormulaZState state) noexcept;
struct FormulaZConsistency {
  std::string policy_version;
  FormulaZState state = FormulaZState::insufficient_metadata;
  std::optional<Value> source_formula;
  std::optional<Value> source_z;
  // Atomic-number totals, never inputs used to alter occupancy or Phase 18 Z.
  std::map<int, double> site_cell_composition;
  std::map<int, double> formula_times_z;
  std::string notice;
};
struct StructureProvenance {
  std::string schema = "qualpf.cif.structure-provenance.v2";
  std::string source;
  std::string source_sha256;
  std::string selected_block; // exact data_ suffix; lookup is case insensitive
  std::string block_id; // ASCII-folded CIF identity; independent of block order
  std::string source_profile;
  std::map<std::string, Value> space_group_declarations;
  std::map<std::string, Value> selected_metadata;
  std::string symmetry_source;
  std::string normalized_identifier;
  int registry_setting = -1;
  std::string registry_choice;
  std::string registry_transform;
  std::size_t operation_count = 0;
  std::vector<SiteProvenance> sites;
  std::vector<std::string> normalization; // exactly {"none"} when absent
  std::vector<std::string> notices;
  // Phase 19A theoretical-card materialization, separate from measured ADP.
  std::string calculation_policy = "phase19a_theoretical_card_adp";
  // Empty means the unchanged v2 strict contract and byte-identical ledger.
  std::string source_ueq_policy;
  std::string formula_z_policy;
  std::string strict_attempt_ledger;
  std::vector<DisplacementChoice> displacement_choices;
  std::optional<FormulaZConsistency> formula_z;
  [[nodiscard]] std::string identity() const;
};
[[nodiscard]] std::string sha256(std::string_view bytes);
// Deterministic, length-framed UTF-8 ledger. Includes exact source tokens/SU.
[[nodiscard]] std::string provenance_ledger(const StructureProvenance &value);
} // namespace qualpf::io::cif
