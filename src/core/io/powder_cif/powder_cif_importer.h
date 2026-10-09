#pragma once

#include "cif_document.h"
#include "qualpf/domain/pattern.h"
#include "qualpf/domain/result.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace qualpf::io::powder_cif {
struct SourceSpan {
  std::size_t begin = 0;
  std::size_t end = 0; // Exclusive, original source bytes.
};
struct SourceArtifact {
  std::string bytes;
  std::string sha256;
  std::string source_id;
};
struct LocatedField {
  std::string tag;
  cif::Location location;
  SourceSpan span;
  std::optional<std::size_t> row;
};
struct ImportError {
  qualpf::Error error;
  std::shared_ptr<const SourceArtifact> source;
  std::string block_id;
  std::optional<std::size_t> block_index;
  std::vector<LocatedField> locations;
  std::optional<std::size_t> axis_count;
  std::optional<std::size_t> intensity_count;
};
template <class T> using ImportResult = std::variant<T, ImportError>;

struct ParsedPowderSource {
  std::shared_ptr<const SourceArtifact> artifact;
  cif::Document document;
  bool source_fragment_without_data_block = false;
};
class PowderCifSyntaxAdapter {
public:
  static ImportResult<ParsedPowderSource> parse(std::string bytes,
                                                std::string source_id);
};

enum class WavelengthState {
  SOURCE_WAVELENGTH,
  WAVELENGTH_REQUIRED,
  WAVELENGTH_OPTIONAL_OR_ABSENT
};
enum class RadiationOrigin { source_file, user_supplied, application_default };
enum class CandidateState {
  no_pattern,
  incomplete,
  invalid,
  requires_wavelength,
  valid
};
struct ColumnCandidate {
  std::string tag;
  LocatedField field;
  std::vector<cif::Value> values;
  std::vector<std::uint32_t> bits;
  std::string status;
};
struct ScalarField {
  std::string tag;
  std::optional<LocatedField> field;
  std::optional<cif::Value> first;
  std::optional<std::uint32_t> bits;
  std::string status = "absent";
};
struct Notice {
  std::string reason;
  std::string tag;
  SourceSpan span;
  std::size_t block_index;
};
struct GeneratedAxis {
  ScalarField minimum;
  ScalarField increment;
  std::size_t count = 0; // Selected intensity rows; never a declared count/max.
};
struct PowderPatternBlockCandidate {
  std::string block_id;
  std::size_t block_index = 0;
  SourceSpan source_span;
  std::vector<ColumnCandidate> available_axis_candidates;
  std::vector<ColumnCandidate> available_intensity_candidates;
  std::optional<ColumnCandidate> selected_axis;
  std::optional<ColumnCandidate> selected_intensity;
  ScalarField wavelength;
  ScalarField range_minimum;
  ScalarField range_increment;
  std::optional<GeneratedAxis> generated_axis;
  std::size_t row_count = 0;
  std::string completeness = "no_pattern";
  CandidateState importability = CandidateState::no_pattern;
  WavelengthState wavelength_state =
      WavelengthState::WAVELENGTH_OPTIONAL_OR_ABSENT;
  std::vector<ImportError> errors;
  std::vector<Notice> notices;
};
struct RadiationProvenance {
  std::optional<RadiationOrigin> origin;
  std::optional<LocatedField> source_field;
  std::optional<std::uint32_t> wavelength_bits;
  std::optional<RadiationKind> kind;
};
struct UserSuppliedRadiation {
  // Powder's frozen bridge distinguishes X-ray/neutron only. Do not inherit
  // the structural domain's electron interpretation or parse source tokens.
  enum class Kind { x_ray, neutron };
  std::uint32_t
      wavelength_bits; // Explicit already transported binary32, no default.
  std::optional<Kind> kind;
};
struct PowderCifImportSnapshot {
  std::shared_ptr<const SourceArtifact> source;
  bool source_fragment_without_data_block = false;
  PowderPatternBlockCandidate selected_block;
  RadiationProvenance radiation;
  std::vector<std::uint32_t> x;
  std::vector<std::uint32_t> raw_y;
  std::vector<std::uint32_t> scaled_y;
  bool reversed = false;
  std::vector<Notice> notices;
};
// No writable selected-source state escapes while awaiting Application input.
class PowderCifPreparation {
public:
  PowderCifPreparation(const PowderCifPreparation &) = default;
  PowderCifPreparation(PowderCifPreparation &&) = default;
  PowderCifPreparation &operator=(const PowderCifPreparation &) = default;
  PowderCifPreparation &operator=(PowderCifPreparation &&) = default;
  const ParsedPowderSource &source() const { return source_; }
  const std::vector<PowderPatternBlockCandidate> &candidates() const {
    return candidates_;
  }
  const PowderPatternBlockCandidate &selected() const {
    return candidates_.at(selected_);
  }
  const std::optional<PowderCifImportSnapshot> &snapshot() const {
    return snapshot_;
  }

private:
  friend class PowderCifImporter;
  PowderCifPreparation() = default;
  ParsedPowderSource source_;
  std::vector<PowderPatternBlockCandidate> candidates_;
  std::size_t selected_ = 0;
  std::optional<PowderCifImportSnapshot> snapshot_;
};
struct ImportedPowderPattern {
  Pattern pattern;
  // Owning provenance stays associated with this Pattern. Project integration
  // must persist it before cutover; the existing product schema is unchanged.
  PowderCifImportSnapshot provenance;
};
class PowderCifImporter {
public:
  static ImportResult<PowderCifPreparation> prepare(std::string bytes,
                                                    std::string source_id);
  static ImportResult<PowderCifImportSnapshot>
  resolve(const PowderCifPreparation &, UserSuppliedRadiation);
  static ImportResult<ImportedPowderPattern> to_pattern(PowderCifImportSnapshot,
                                                        DatasetId);
};
} // namespace qualpf::io::powder_cif
