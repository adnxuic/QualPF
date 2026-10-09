#pragma once

#include "qualpf/crystallography/composition.h"
#include "qualpf/crystallography/powder_intensity.h"

namespace qualpf::crystallography {

inline constexpr std::size_t database_reflection_slot_limit = 500;
struct ReferenceIntensityRatio {
  double
      maximum_raw_intensity; // All merged physical lines, before the slot cap.
  double density_volume_squared;
  double density_volume_ratio;
  double intensity_ratio;
  double value;
};
// Scalar compatibility calibration. Caller supplies Cu-Kalpha1 raw intensity;
// the complete card factory additionally checks radiation and member geometry.
[[nodiscard]] Result<ReferenceIntensityRatio>
legacy_cu_reference_intensity_ratio(const CrystalDensity &density,
                                    double maximum_raw_intensity);

struct CardSlotDecision {
  ReflectionIdentity identity;
  ReflectionIdentity line_representative_identity;
  std::size_t
      angular_slot;     // One-based, before truncation and intensity sorting.
  double raw_intensity; // Zero for a non-representative overlap member.
  bool representative;
  bool within_prefix;
  bool meets_threshold;
  bool retained;
};
struct TheoreticalCardLine {
  std::size_t angular_slot;
  PowderLine physical_line; // Includes complete, untruncated source membership.
  double card_relative_intensity; // Max of the first 500 slots is 1000.
};
struct DatabaseLineSelection {
  std::size_t input_slot_count;
  std::size_t prefix_slot_count;
  double physical_maximum;
  double prefix_maximum;
  double threshold;
  std::vector<CardSlotDecision> decisions; // Physical angular slot order.
  std::vector<TheoreticalCardLine> lines;  // Descending raw, then angular slot.
};
// No structure ownership claim; accepts only an already validated Phase 17
// value and preserves its angular ordering. No global top-500 selection.
[[nodiscard]] Result<DatabaseLineSelection>
select_database_lines(const PowderLineSet &physical);

struct TheoreticalCardMetadata {
  std::string identifier;
  std::string chemical_name;
  std::string mineral_name;
  std::string provenance;
};
class TheoreticalReferenceCard {
public:
  [[nodiscard]] static Result<TheoreticalReferenceCard>
  create(const CrystalStructure &structure, const PowderLineSet &physical,
         TheoreticalCardMetadata metadata, ScatteringRadiation radiation = {},
         FriedelPolicy friedel = FriedelPolicy::merge);
  [[nodiscard]] const TheoreticalCardMetadata &metadata() const noexcept {
    return metadata_;
  }
  [[nodiscard]] const CrystalComposition &composition() const noexcept {
    return composition_;
  }
  [[nodiscard]] const CrystalDensity &density() const noexcept {
    return density_;
  }
  [[nodiscard]] const CrystalAbsorption &absorption() const noexcept {
    return absorption_;
  }
  [[nodiscard]] const ReferenceIntensityRatio &rir() const noexcept {
    return rir_;
  }
  [[nodiscard]] const PowderLineSet &physical_lines() const noexcept {
    return physical_;
  }
  [[nodiscard]] const DatabaseLineSelection &presentation() const noexcept {
    return presentation_;
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
  TheoreticalReferenceCard(TheoreticalCardMetadata metadata,
                           CrystalComposition composition,
                           CrystalDensity density, CrystalAbsorption absorption,
                           ReferenceIntensityRatio rir, PowderLineSet physical,
                           DatabaseLineSelection presentation,
                           UnitCellParameters cell, int group_id,
                           ScatteringRadiation radiation, FriedelPolicy friedel)
      : metadata_(std::move(metadata)), composition_(std::move(composition)),
        density_(density), absorption_(std::move(absorption)), rir_(rir),
        physical_(std::move(physical)), presentation_(std::move(presentation)),
        cell_(cell), group_id_(group_id), radiation_(radiation),
        friedel_(friedel) {}
  TheoreticalCardMetadata metadata_;
  CrystalComposition composition_;
  CrystalDensity density_;
  CrystalAbsorption absorption_;
  ReferenceIntensityRatio rir_;
  PowderLineSet physical_;
  DatabaseLineSelection presentation_;
  UnitCellParameters cell_;
  int group_id_;
  ScatteringRadiation radiation_;
  FriedelPolicy friedel_;
};

} // namespace qualpf::crystallography
