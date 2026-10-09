#pragma once

#include "cif_document.h"
#include "cif_provenance.h"
#include "qualpf/crystallography/structure_factor.h"
#include <map>

namespace qualpf::io::cif {

struct StructureBlockCandidate;

struct ImportIssue {
  std::string code; // stable machine-readable reason
  std::string source;
  std::string block;
  std::string field;
  Location location;
  std::string message;
  std::string core_field;
  std::string core_reason;
  std::vector<StructureBlockCandidate> candidates;
  std::optional<StructureProvenance> provenance = std::nullopt;
};
template <class T> using ImportResult = std::variant<T, ImportIssue>;

struct StructureMetadata {
  std::string source;
  std::string block;
  std::string chemical_name;
  std::string mineral_name;
  // Canonical coordinates = supplied coordinates - this exact origin offset.
  std::array<int, 3> origin_shift_twenty_fourths{};
  // Selected original values only; never substitutes for calculated
  // composition.
  std::map<std::string, Value> reported;
};
struct ImportedSiteRecord {
  std::string label;
  std::array<double, 3> input_fractional;
  double imported_b_iso;
  crystallography::DisplacementTensor imported_beta;
  bool default_occupancy;
  bool default_import_adp;
  bool default_card_adp;
  // Original CIF tokens, including uncertainty and location. input_fractional
  // remains nominal after coordinate/origin conversion. Reconciliation evidence
  // and the resulting coordinate live in the selected-block provenance ledger.
  std::array<Value, 3> source_coordinates;
  std::optional<int> declared_multiplicity;
  std::size_t orbit_multiplicity = 0;
};
struct ImportedCrystal {
  crystallography::CrystalStructure structure;
  StructureMetadata metadata;
  std::vector<ImportedSiteRecord> sites;
  std::vector<ImportIssue> notices;
  StructureProvenance provenance;
};

enum class BlockClassification {
  metadata_only,
  complete_structure,
  partial_structure,
  placeholder_structure,
  invalid_structure
};
[[nodiscard]] std::string_view
classification_name(BlockClassification value) noexcept;
// Qt-free owning DTO for Application/GUI. No UI selection in the parser.
struct StructureBlockCandidate {
  std::string block_id;
  std::string block_name;
  std::array<std::optional<Value>, 6> cell_parameters;
  bool cell_present = false;
  bool cell_valid = false;
  std::map<std::string, Value> space_group_declarations;
  bool explicit_operations = false;
  std::optional<int> registry_setting;
  std::string normalized_identifier;
  std::string registry_choice;
  std::size_t operation_count = 0;
  std::optional<ImportIssue> symmetry_rejection;
  std::size_t atom_site_count = 0;
  std::size_t usable_coordinate_count = 0;
  std::size_t missing_coordinate_count = 0;
  std::size_t placeholder_coordinate_count = 0;
  std::size_t malformed_coordinate_count = 0;
  std::size_t occupancy_present_count = 0;
  bool occupancy_complete = false;
  std::map<std::string, std::size_t> adp_summary;
  BlockClassification classification = BlockClassification::metadata_only;
  bool importable = false;
  std::optional<ImportIssue> rejection;
  std::map<std::string, Value> source_metadata;
  std::optional<StructureProvenance> provenance;
};
enum class BlockSelectionPolicy { require_unique, specified_block };
struct BlockSelection {
  BlockSelectionPolicy policy = BlockSelectionPolicy::require_unique;
  std::string block_id;
};
enum class SourceUeqPolicy { strict, zincite_2300112_v1 };
enum class FormulaZPolicy { strict, source_metadata_v1 };
inline constexpr std::string_view source_ueq_policy_v1 =
    "phase19c4.2300112-source-ueq.v1";
inline constexpr std::string_view formula_z_policy_v1 =
    "phase19c4.formula-z-source-metadata.experimental.v1";
struct ImportPolicies {
  SourceUeqPolicy source_ueq = SourceUeqPolicy::strict;
  FormulaZPolicy formula_z = FormulaZPolicy::strict;
};
[[nodiscard]] std::vector<StructureBlockCandidate>
structure_candidates(const Document &document, ImportPolicies policies = {});
[[nodiscard]] ImportResult<ImportedCrystal>
import_cif(const Document &document, BlockSelection selection,
           ImportPolicies policies = {});

// A reload ticket owns the exact input and explicit selection. Re-import checks
// the source hash and complete provenance ledger; changed semantics fail
// closed.
[[nodiscard]] ImportResult<std::string>
save_selection(const Document &document, const ImportedCrystal &selected);
[[nodiscard]] ImportResult<ImportedCrystal>
reload_selection(std::string_view ticket);

// Exact affine operator parser, with Phase 14's denominator-12 representation.
// A valid non-registry operator set cannot be smuggled into SpaceGroup.
[[nodiscard]] ImportResult<crystallography::SymmetryOperation>
parse_operation(const Value &value, std::string source = {},
                std::string block = {});

// Structure import explicitly for the theoretical-card consumer. Applies the
// observed missing-ADP and card ADP-range policies, retaining both stages
// above. Does not enumerate reflections, calculate factors/cards or write a
// database. Unsupported/malformed data returns no partial structure and never
// retries Legacy.
[[nodiscard]] ImportResult<ImportedCrystal>
import_for_theoretical_card(const Document &document);

} // namespace qualpf::io::cif
