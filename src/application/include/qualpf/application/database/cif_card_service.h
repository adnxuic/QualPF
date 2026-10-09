#pragma once

#include "cif_import.h"
#include "qualpf/crystallography/theoretical_card.h"

namespace qualpf::application {

enum class CifCardStage { syntax, import, reflections, factors, powder, card };
[[nodiscard]] std::string_view cif_card_stage_name(CifCardStage stage) noexcept;

struct CifCardIssue {
  CifCardStage stage;
  std::string code;
  std::string source;
  std::string block;
  std::string field;
  io::cif::Location location;
  std::string message;
  std::string core_field;
  std::string core_reason;
  std::optional<ErrorCode> core_code;
  std::vector<io::cif::StructureBlockCandidate> candidates;
  std::optional<io::cif::StructureProvenance> provenance = std::nullopt;
};

// Complete owning result: the card and its imported structure were calculated
// together. No public constructor accepts an unrelated structure/card pair.
class CifCard {
public:
  [[nodiscard]] const io::cif::ImportedCrystal &imported() const noexcept {
    return imported_;
  }
  [[nodiscard]] const crystallography::TheoreticalReferenceCard &
  card() const noexcept {
    return card_;
  }
  [[nodiscard]] std::size_t reflection_count() const noexcept {
    return reflection_count_;
  }

private:
  friend class CifCardService;
  CifCard(io::cif::ImportedCrystal imported,
          crystallography::TheoreticalReferenceCard card,
          std::size_t reflection_count)
      : imported_(std::move(imported)), card_(std::move(card)),
        reflection_count_(reflection_count) {}
  io::cif::ImportedCrystal imported_;
  crystallography::TheoreticalReferenceCard card_;
  std::size_t reflection_count_;
};
using CifCardResult = std::variant<CifCard, CifCardIssue>;

// Stateless application orchestration. The only supported product policy is
// canonical Cu Kalpha1, merged Friedel pairs, 1..150 degrees, minimum d=0.1 A,
// and the frozen Phase 18 slot/normalization policy. CIF radiation metadata
// does not override it. There is no SQLite, Qt, native state or fallback here.
class CifCardService {
public:
  [[nodiscard]] static CifCardResult
  from_bytes(std::string_view bytes, std::string source,
             io::cif::Limits limits = {},
             io::cif::BlockSelection selection = {},
             io::cif::ImportPolicies policies = {});
  [[nodiscard]] static CifCardResult
  from_document(const io::cif::Document &document,
                io::cif::BlockSelection selection,
                io::cif::ImportPolicies policies = {});
  [[nodiscard]] static CifCardResult
  from_imported(io::cif::ImportedCrystal imported);
};

} // namespace qualpf::application
