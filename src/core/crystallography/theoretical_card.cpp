#include "qualpf/crystallography/theoretical_card.h"
#include "qualpf/domain/radiation.h"
#include <algorithm>
#include <cmath>
#include <set>

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
bool close(double a, double b, double tolerance) {
  return std::isfinite(a) && std::isfinite(b) &&
         std::abs(a - b) <= tolerance * std::max(std::abs(a), std::abs(b));
}
bool finite_member(const PowderReflection &r) {
  return std::isfinite(r.fc_squared) && r.fc_squared >= 0 &&
         positive(r.correction.lorentz) &&
         positive(r.correction.polarization) &&
         positive(r.correction.combined) && std::isfinite(r.raw_intensity) &&
         r.raw_intensity >= 0;
}
} // namespace

Result<ReferenceIntensityRatio>
legacy_cu_reference_intensity_ratio(const CrystalDensity &density,
                                    double maximum) {
  if (!positive(density.grams_per_cm3) ||
      !positive(density.volume_angstrom_cubed) ||
      !positive(density.cell_mass_daltons))
    return Result<ReferenceIntensityRatio>(
        invalid("rir.denominator", "invalid_rir_denominator"));
  if (!std::isfinite(maximum) || maximum < 0)
    return Result<ReferenceIntensityRatio>(
        invalid("rir.intensity", "invalid_rir_intensity"));
  if (maximum == 0)
    return Result<ReferenceIntensityRatio>(invalid("rir.value", "zero_rir"));
  const double denominator =
      (density.volume_angstrom_cubed * density.volume_angstrom_cubed) *
      density.grams_per_cm3;
  if (!positive(denominator))
    return Result<ReferenceIntensityRatio>(
        numerical("rir.denominator", "invalid_rir_denominator"));
  constexpr double corundum_rho_v2 = static_cast<double>(259079.140625f);
  constexpr double corundum_maximum = static_cast<double>(340862.156f);
  const double rhov = corundum_rho_v2 / denominator,
               intensity = maximum / corundum_maximum;
  const double rir = rhov * intensity;
  if (!positive(rhov) || !positive(intensity) || !positive(rir))
    return Result<ReferenceIntensityRatio>(
        numerical("rir.value", "nonpositive_or_nonfinite_rir"));
  return Result<ReferenceIntensityRatio>(
      ReferenceIntensityRatio{maximum, denominator, rhov, intensity, rir});
}

Result<DatabaseLineSelection>
select_database_lines(const PowderLineSet &physical) {
  struct Slot {
    const PowderReflection *member;
    const PowderLine *line;
    bool representative;
  };
  std::vector<Slot> slots;
  double all_maximum = 0;
  for (const auto &line : physical.lines()) {
    all_maximum = std::max(all_maximum, line.raw_intensity);
    for (const auto &member : line.members) {
      if (!finite_member(member))
        return Result<DatabaseLineSelection>(
            invalid("card.line.members", "malformed_powder_line_set"));
      slots.push_back(
          {&member, &line, member.identity == line.representative_identity});
    }
  }
  if (slots.empty())
    return Result<DatabaseLineSelection>(invalid("card.lines", "empty_card"));
  std::sort(slots.begin(), slots.end(), [](const auto &a, const auto &b) {
    if (a.member->reflection.two_theta.value !=
        b.member->reflection.two_theta.value)
      return a.member->reflection.two_theta.value <
             b.member->reflection.two_theta.value;
    return a.member->identity < b.member->identity;
  });
  const auto prefix = std::min(slots.size(), database_reflection_slot_limit);
  double maximum = 0;
  for (std::size_t i = 0; i < prefix; ++i)
    if (slots[i].representative)
      maximum = std::max(maximum, slots[i].line->raw_intensity);
  constexpr double native_epsilon = 0x1p-23;
  if (maximum <= native_epsilon)
    return Result<DatabaseLineSelection>(
        invalid("card.lines", "empty_card_after_filtering"));
  const double threshold = (native_epsilon * maximum) / 10;
  DatabaseLineSelection selection{slots.size(), prefix, all_maximum, maximum,
                                  threshold,    {},     {}};
  for (std::size_t i = 0; i < slots.size(); ++i) {
    const auto &slot = slots[i];
    const double raw = slot.representative ? slot.line->raw_intensity : 0;
    const bool meets = raw >= threshold, retained = i < prefix && meets;
    selection.decisions.push_back(
        {slot.member->identity, slot.line->representative_identity, i + 1, raw,
         slot.representative, i < prefix, meets, retained});
    if (retained) {
      const double relative = 1000 * (raw / maximum);
      if (!std::isfinite(relative))
        return Result<DatabaseLineSelection>(
            numerical("card.line.intensity", "nonfinite_card_field"));
      selection.lines.push_back({i + 1, *slot.line, relative});
    }
  }
  std::sort(
      selection.lines.begin(), selection.lines.end(),
      [](const auto &a, const auto &b) {
        if (a.physical_line.raw_intensity != b.physical_line.raw_intensity)
          return a.physical_line.raw_intensity > b.physical_line.raw_intensity;
        return a.angular_slot < b.angular_slot;
      });
  return Result<DatabaseLineSelection>(std::move(selection));
}

Result<TheoreticalReferenceCard> TheoreticalReferenceCard::create(
    const CrystalStructure &structure, const PowderLineSet &physical,
    TheoreticalCardMetadata metadata, ScatteringRadiation radiation,
    FriedelPolicy friedel) {
  if (radiation.kind != RadiationKind::xray)
    return Result<TheoreticalReferenceCard>(Error{ErrorCode::unsupported,
                                                  "card.radiation",
                                                  {},
                                                  "unsupported_rir_radiation"});
  if (radiation.wavelength.value != qualpf::radiation::CU_KA1_WAVE)
    return Result<TheoreticalReferenceCard>(
        Error{ErrorCode::unsupported,
              "card.wavelength",
              {},
              "unsupported_rir_wavelength"});
  if (friedel != FriedelPolicy::merge && friedel != FriedelPolicy::separate)
    return Result<TheoreticalReferenceCard>(
        invalid("card.friedel", "invalid_friedel_policy"));
  auto selection = select_database_lines(physical);
  if (!selection)
    return Result<TheoreticalReferenceCard>(selection.error());
  std::set<Hkl, HklLess> hkls;
  for (const auto &line : physical.lines())
    for (const auto &member : line.members) {
      const auto &r = member.reflection;
      if (!hkls.insert(r.hkl).second)
        return Result<TheoreticalReferenceCard>(
            invalid("card.line.hkl", "duplicate_reflection"));
      auto d = structure.cell().d_spacing(r.hkl);
      if (!d)
        return Result<TheoreticalReferenceCard>(d.error());
      if (!close(d.value().value, r.d.value, 1e-10))
        return Result<TheoreticalReferenceCard>(
            invalid("card.line.d", "reflection_cell_mismatch"));
      auto angle = two_theta(d.value(), radiation.wavelength);
      if (!angle)
        return Result<TheoreticalReferenceCard>(angle.error());
      if (!close(angle.value().value, r.two_theta.value, 1e-9))
        return Result<TheoreticalReferenceCard>(
            invalid("card.line.two_theta", "reflection_radiation_mismatch"));
      auto cls = reflection_class(r.hkl, structure.group(), friedel);
      if (!cls)
        return Result<TheoreticalReferenceCard>(cls.error());
      auto absent = systematically_absent(r.hkl, structure.group());
      if (!absent)
        return Result<TheoreticalReferenceCard>(absent.error());
      if (absent.value() || cls.value().multiplicity() != r.multiplicity ||
          !equal_hkl(cls.value().canonical(), r.hkl))
        return Result<TheoreticalReferenceCard>(
            invalid("card.line.group", "reflection_group_mismatch"));
      PowderIntensityOptions options;
      options.radiation = radiation;
      auto expected =
          powder_reflection(member.identity, r, member.fc_squared, options);
      if (!expected)
        return Result<TheoreticalReferenceCard>(expected.error());
      if (member.raw_intensity != expected.value().raw_intensity ||
          member.correction.lorentz != expected.value().correction.lorentz ||
          member.correction.polarization !=
              expected.value().correction.polarization ||
          member.correction.combined != expected.value().correction.combined)
        return Result<TheoreticalReferenceCard>(
            invalid("card.line.intensity", "incompatible_powder_intensity"));
    }
  auto composition = CrystalComposition::calculate(structure);
  if (!composition)
    return Result<TheoreticalReferenceCard>(composition.error());
  auto density = crystal_density(composition.value().cell_mass_daltons(),
                                 structure.cell().volume_angstrom_cubed());
  if (!density)
    return Result<TheoreticalReferenceCard>(density.error());
  auto absorption =
      crystal_absorption(composition.value(), density.value(), radiation);
  if (!absorption)
    return Result<TheoreticalReferenceCard>(absorption.error());
  auto rir = legacy_cu_reference_intensity_ratio(
      density.value(), selection.value().physical_maximum);
  if (!rir)
    return Result<TheoreticalReferenceCard>(rir.error());
  return Result<TheoreticalReferenceCard>(TheoreticalReferenceCard(
      std::move(metadata), std::move(composition.value()), density.value(),
      std::move(absorption.value()), rir.value(), physical,
      std::move(selection.value()), structure.cell().parameters(),
      structure.group().setting().id, radiation, friedel));
}
} // namespace qualpf::crystallography
