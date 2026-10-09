#include "qualpf/crystallography/theoretical_card.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

using namespace qualpf;
using namespace qualpf::crystallography;
namespace {
std::size_t checks = 0, failed = 0, pipelines = 0, source_reflections = 0;
void check(bool condition, const std::string &name) {
  ++checks;
  if (!condition) {
    ++failed;
    std::cerr << "FAIL " << name << '\n';
  }
}
template <class T> T take(Result<T> result, const char *name) {
  check(bool(result), name);
  if (!result)
    throw std::runtime_error(std::string(name) + ": " + result.error().field +
                             " " + result.error().reason);
  return std::move(result).value();
}
bool near(double a, double b, double relative = 2e-12) {
  return std::isfinite(a) && std::isfinite(b) &&
         std::abs(a - b) <= relative * std::max(std::abs(a), std::abs(b));
}
template <class T>
void error_case(const char *name, const Result<T> &result, const char *field,
                const char *reason) {
  check(!result, std::string("error.") + name);
  if (result)
    return;
  check(result.error().field == field, std::string("error-field.") + name);
  check(result.error().reason == reason, std::string("error-reason.") + name);
  std::cout << "ERROR_TEST " << name << ' ' << result.error().field << ' '
            << result.error().reason << '\n';
}
void same_composition(const CrystalComposition &a, const CrystalComposition &b,
                      bool exact) {
  check(a.entries().size() == b.entries().size(), "composition.entries");
  check(a.formula().display == b.formula().display, "formula.invariance");
  check(a.formula().formula_units_per_cell ==
            b.formula().formula_units_per_cell,
        "Z.invariance");
  check(exact ? a.cell_mass_daltons() == b.cell_mass_daltons()
              : near(a.cell_mass_daltons(), b.cell_mass_daltons()),
        "mass.invariance");
  for (std::size_t i = 0; i < std::min(a.entries().size(), b.entries().size());
       ++i) {
    check(a.entries()[i].species == b.entries()[i].species,
          "species.invariance");
    check(exact ? a.entries()[i].unit_cell_amount ==
                      b.entries()[i].unit_cell_amount
                : near(a.entries()[i].unit_cell_amount,
                       b.entries()[i].unit_cell_amount),
          "amount.invariance");
  }
}
std::vector<ReflectionIdentity> ids(const DatabaseLineSelection &s) {
  std::vector<ReflectionIdentity> out;
  for (const auto &l : s.lines)
    out.push_back(l.physical_line.representative_identity);
  return out;
}
void same_selection(const DatabaseLineSelection &a,
                    const DatabaseLineSelection &b) {
  check(ids(a) == ids(b), "card.identities");
  check(a.prefix_maximum == b.prefix_maximum &&
            a.physical_maximum == b.physical_maximum &&
            a.threshold == b.threshold,
        "card.scalars");
  check(a.decisions.size() == b.decisions.size(), "card.decisions.size");
  for (std::size_t i = 0; i < std::min(a.decisions.size(), b.decisions.size());
       ++i) {
    const auto &x = a.decisions[i], &y = b.decisions[i];
    check(x.identity == y.identity && x.angular_slot == y.angular_slot &&
              x.retained == y.retained && x.raw_intensity == y.raw_intensity &&
              x.line_representative_identity == y.line_representative_identity,
          "card.decision.invariance");
  }
  for (std::size_t i = 0; i < std::min(a.lines.size(), b.lines.size()); ++i) {
    const auto &x = a.lines[i], &y = b.lines[i];
    check(x.card_relative_intensity == y.card_relative_intensity,
          "card.normalization.invariance");
    check(x.physical_line.members.size() == y.physical_line.members.size(),
          "card.members.size");
    for (std::size_t j = 0; j < std::min(x.physical_line.members.size(),
                                         y.physical_line.members.size());
         ++j)
      check(x.physical_line.members[j].identity ==
                y.physical_line.members[j].identity,
            "card.members.identity");
  }
}
AtomicSite site(std::string_view label, std::array<double, 3> xyz = {0, 0, 0},
                double occupancy = 1) {
  return {take(ScatteringRegistry{}.find(label), "species"),
          take(FractionalPosition::create(xyz), "position"),
          take(Occupancy::create(occupancy), "occupancy"),
          take(Displacement::isotropic_b(0), "displacement")};
}
PowderLine scalar_line(std::size_t id, double raw) {
  Reflection r{{static_cast<std::int32_t>(id), 0, 0},
               DSpacingAngstrom{1},
               TwoThetaDegrees{10 + .1 * id},
               1};
  PowderReflection member{
      static_cast<ReflectionIdentity>(id), r, std::abs(raw), {1, 1, 1}, raw};
  return {member.identity, r, raw, 0, {member}};
}
void selection_properties() {
  std::mt19937 random(180018);
  for (const std::size_t n : {1, 2, 499, 500, 501, 650}) {
    for (const bool ties : {false, true}) {
      std::vector<PowderLine> lines;
      for (std::size_t i = 1; i <= n; ++i)
        lines.push_back(scalar_line(i, ties ? 10 : static_cast<double>(i)));
      auto physical = take(normalize_powder_lines(lines), "scalar.normalize");
      auto selected = take(select_database_lines(physical), "scalar.select");
      const auto count = std::min(n, database_reflection_slot_limit);
      check(selected.lines.size() == count, "slot.cap");
      check(selected.input_slot_count == n &&
                selected.prefix_slot_count == count,
            "slot.counts");
      for (std::size_t i = 0; i < count; ++i)
        check(selected.lines[i].angular_slot == (ties ? i + 1 : count - i),
              "deterministic.rank");
      check(selected.lines.front().card_relative_intensity == 1000,
            "card.maximum");
      if (n > 500 && !ties)
        check(selected.physical_maximum == double(n) &&
                  selected.prefix_maximum == 500,
              "different.maxima");
      for (int permutation = 0; permutation < 4; ++permutation) {
        std::shuffle(lines.begin(), lines.end(), random);
        auto reordered =
            take(normalize_powder_lines(lines), "permuted.normalize");
        same_selection(selected, take(select_database_lines(reordered),
                                      "permuted.select"));
      }
      for (double scale : {.125, 2., 1e6}) {
        auto scaled = lines;
        for (auto &l : scaled) {
          l.raw_intensity *= scale;
          l.members[0].raw_intensity *= scale;
        }
        auto ss = take(select_database_lines(take(
                           normalize_powder_lines(scaled), "scale.normalize")),
                       "scale.select");
        check(ids(selected) == ids(ss), "scale.selection");
        for (std::size_t i = 0; i < count; ++i)
          check(near(selected.lines[i].card_relative_intensity,
                     ss.lines[i].card_relative_intensity),
                "scale.normalization");
      }
    }
  }
  constexpr double eps = 0x1p-23;
  auto boundary = take(
      normalize_powder_lines(
          {scalar_line(1, 10), scalar_line(2, std::nextafter(eps, 0.)),
           scalar_line(3, eps),
           scalar_line(
               4, std::nextafter(eps, std::numeric_limits<double>::infinity())),
           scalar_line(5, 0)}),
      "threshold.normalize");
  auto selected = take(select_database_lines(boundary), "threshold.select");
  check(ids(selected) == std::vector<ReflectionIdentity>{1, 4, 3},
        "exact.binary64.threshold");
  check(selected.threshold == eps, "exact.threshold.scalar");
  for (double maximum : {eps, std::nextafter(eps, 0.), 1e-20})
    error_case("maximum_floor",
               select_database_lines(
                   take(normalize_powder_lines({scalar_line(1, maximum)}),
                        "tiny.normalize")),
               "card.lines", "empty_card_after_filtering");
  check(bool(select_database_lines(take(
            normalize_powder_lines({scalar_line(1, std::nextafter(eps, 1.))}),
            "floor-neighbor.normalize"))),
        "floor.neighbor");
  std::vector<PowderLine> overlap;
  for (std::size_t i = 1; i <= 502; ++i)
    overlap.push_back(scalar_line(i, i == 502 ? 100 : 1));
  overlap[499].raw_intensity = 20;
  overlap[499].members[0].raw_intensity = 12;
  overlap[500].members[0].raw_intensity = 8;
  overlap[499].members.push_back(overlap[500].members[0]);
  overlap.erase(overlap.begin() + 500);
  auto combined =
      take(select_database_lines(
               take(normalize_powder_lines(overlap), "overlap.normalize")),
           "overlap.select");
  check(combined.input_slot_count == 502 && combined.lines.size() == 500,
        "overlap.slot.cap");
  check(combined.prefix_maximum == 20 && combined.physical_maximum == 100,
        "overlap.maxima");
  check(combined.lines.front().physical_line.members.size() == 2 &&
            combined.lines.front().physical_line.members.back().identity == 501,
        "overlap.complete.members");
  check(combined.lines.front().physical_line.relative_intensity == 200 &&
            combined.lines.front().card_relative_intensity == 1000,
        "two.normalizations");
  auto malformed = scalar_line(1, 10);
  malformed.members[0].fc_squared = std::numeric_limits<double>::quiet_NaN();
  error_case("malformed_fc2",
             select_database_lines(take(normalize_powder_lines({malformed}),
                                        "malformed.transport")),
             "card.line.members", "malformed_powder_line_set");
  error_case("empty_lines", normalize_powder_lines({}), "powder.lines",
             "empty_line_set");
  error_case("all_zero", normalize_powder_lines({scalar_line(1, 0)}),
             "powder.lines", "all_zero_normalization");
  auto invalid_members = scalar_line(1, 1);
  invalid_members.members.clear();
  error_case("missing_members", normalize_powder_lines({invalid_members}),
             "powder.line.members", "invalid_line_membership");
}
void error_properties() {
  const double inf = std::numeric_limits<double>::infinity(),
               nan = std::numeric_limits<double>::quiet_NaN();
  error_case("negative_occupancy", Occupancy::create(-.1), "site.occupancy",
             "invalid_occupancy");
  error_case("excess_occupancy", Occupancy::create(1.1), "site.occupancy",
             "invalid_occupancy");
  error_case("nan_occupancy", Occupancy::create(nan), "site.occupancy",
             "invalid_occupancy");
  error_case("unknown_mass", ChemistryRegistry{}.atomic_mass("Xx"),
             "composition.atomic_mass", "unsupported_atomic_mass");
  error_case("missing_attenuation",
             ChemistryRegistry{}.mass_attenuation(93, WavelengthAngstrom{1.54}),
             "absorption.atomic_number", "unsupported_absorption_data");
  error_case("invalid_attenuation_Z",
             ChemistryRegistry{}.mass_attenuation(0, WavelengthAngstrom{1.54}),
             "absorption.atomic_number", "unsupported_absorption_data");
  for (double wave : {0., -1., nan, inf})
    error_case("invalid_wavelength",
               attenuation_energy_kev(WavelengthAngstrom{wave}),
               "absorption.wavelength", "invalid_wavelength");
  error_case("energy_overflow",
             attenuation_energy_kev(
                 WavelengthAngstrom{std::numeric_limits<double>::denorm_min()}),
             "absorption.energy", "nonfinite_energy");
  error_case(
      "attenuation_range",
      ChemistryRegistry{}.mass_attenuation(14, WavelengthAngstrom{1e-20}),
      "absorption.energy", "attenuation_out_of_range");
  error_case("bad_bracket", interpolate_mass_attenuation({1, 1}, {1, 2}, 1),
             "absorption.interpolation", "invalid_attenuation_interpolation");
  error_case("bad_coefficient",
             interpolate_mass_attenuation({1, -1}, {2, 2}, 1.5),
             "absorption.interpolation", "invalid_attenuation_interpolation");
  error_case("nan_coefficient",
             interpolate_mass_attenuation({1, nan}, {2, 2}, 1.5),
             "absorption.interpolation", "invalid_attenuation_interpolation");
  auto power =
      take(interpolate_mass_attenuation({1, 10}, {10, 1000}, std::sqrt(10.)),
           "analytic.power-law");
  check(near(power.cm2_per_g, 100), "loglog.analytic");
  check(take(interpolate_mass_attenuation({1, 0}, {2, 4}, 1.5), "zero-endpoint")
                .cm2_per_g == 0,
        "zero.endpoint.rule");
  check(take(interpolate_mass_attenuation({1, 2}, {2, 4}, 1), "first-endpoint")
                .cm2_per_g == 2,
        "first.endpoint");
  check(take(interpolate_mass_attenuation({1, 2}, {2, 4}, 2), "last-endpoint")
                .cm2_per_g == 4,
        "last.endpoint");
  for (double volume : {0., -1., nan, inf})
    error_case("invalid_volume", crystal_density(1, volume), "density.volume",
               "invalid_cell_volume");
  error_case("invalid_mass", crystal_density(-1, 1), "density.cell_mass",
             "invalid_cell_mass");
  error_case("density_overflow", crystal_density(1e308, 1e-20), "density.value",
             "nonfinite_density");
  error_case("density_underflow",
             crystal_density(std::numeric_limits<double>::denorm_min(), 1e308),
             "density.value", "nonfinite_density");
  const CrystalDensity valid{1, 1, 1};
  error_case("zero_RIR", legacy_cu_reference_intensity_ratio(valid, 0),
             "rir.value", "zero_rir");
  error_case("nan_RIR_intensity",
             legacy_cu_reference_intensity_ratio(valid, nan), "rir.intensity",
             "invalid_rir_intensity");
  error_case("invalid_RIR_denominator",
             legacy_cu_reference_intensity_ratio({1, 1, 0}, 1),
             "rir.denominator", "invalid_rir_denominator");
  error_case("RIR_denominator_overflow",
             legacy_cu_reference_intensity_ratio({1, 1e300, 1}, 1),
             "rir.denominator", "invalid_rir_denominator");
  error_case("RIR_underflow",
             legacy_cu_reference_intensity_ratio(
                 valid, std::numeric_limits<double>::denorm_min()),
             "rir.value", "nonpositive_or_nonfinite_rir");
  const auto cell = take(UnitCell::create({{5, 5, 5}, {90, 90, 90}}), "cell");
  const auto group = take(SpaceGroupRegistry{}.by_number(1), "P1");
  const auto zero =
      take(CrystalStructure::create(cell, group, {site("Si", {0, 0, 0}, 0)}),
           "zero-structure");
  error_case("zero_composition", CrystalComposition::calculate(zero),
             "composition.cell_mass", "invalid_composition");
  auto c = take(
      CrystalComposition::calculate(take(
          CrystalStructure::create(cell, group, {site("Si")}), "Si-structure")),
      "Si-composition");
  const auto density =
      take(crystal_density(c.cell_mass_daltons(), 125), "Si-density");
  error_case("neutron_absorption",
             crystal_absorption(
                 c, density,
                 {RadiationKind::neutron, WavelengthAngstrom{1.54}, false}),
             "absorption.radiation", "unsupported_radiation");
  auto altered = density;
  altered.grams_per_cm3 *= 2;
  error_case("inconsistent_density", crystal_absorption(c, altered, {}),
             "absorption.density", "incompatible_density");
  const auto cubic = take(SpaceGroupRegistry{}.by_number(225), "Fm-3m");
  const auto salt =
      take(CrystalComposition::calculate(take(
               CrystalStructure::create(
                   cell, cubic, {site("Na1+"), site("Cl1-", {.5, .5, .5})}),
               "analytic.salt")),
           "analytic.salt.composition");
  check(salt.entries().size() == 2 && salt.entries()[0].unit_cell_amount == 4 &&
            salt.entries()[1].unit_cell_amount == 4,
        "analytic.centered.special.positions");
  check(salt.formula().formula_units_per_cell == 4, "analytic.salt.Z");
  for (int z = 1; z <= 92; ++z) {
    const auto range = take(ChemistryRegistry{}.attenuation_energy_range(z),
                            "element.energy-range");
    check(range[0] > 0 && range[1] > range[0], "registry.range");
    const auto mac = take(
        ChemistryRegistry{}.mass_attenuation(z, WavelengthAngstrom{1.54059307}),
        "element.attenuation");
    check(mac.cm2_per_g > 0 && mac.log_fraction >= 0 && mac.log_fraction <= 1,
          "registry.finite");
  }
}
void composition_properties(const CrystalStructure &structure) {
  const auto c = take(CrystalComposition::calculate(structure), "composition");
  const auto density =
      take(crystal_density(c.cell_mass_daltons(),
                           structure.cell().volume_angstrom_cubed()),
           "density");
  check(density.grams_per_cm3 > 0 && std::isfinite(density.grams_per_cm3),
        "density.finite");
  double mass_sum = 0;
  for (const auto &e : c.entries()) {
    mass_sum += e.cell_mass_daltons;
    check(e.cell_mass_daltons == e.unit_cell_amount * e.atomic_mass_daltons,
          "constituent.mass");
  }
  check(near(mass_sum, c.cell_mass_daltons()), "mass.sum");
  auto sites = std::vector<AtomicSite>(structure.sites().begin(),
                                       structure.sites().end());
  std::reverse(sites.begin(), sites.end());
  same_composition(c,
                   take(CrystalComposition::calculate(take(
                            CrystalStructure::create(structure.cell(),
                                                     structure.group(), sites),
                            "permuted.structure")),
                        "permuted.composition"),
                   true);
  for (auto &s : sites) {
    auto xyz = s.position.coordinates();
    xyz[0] += 2;
    xyz[1] -= 3;
    xyz[2] += 1;
    s.position = take(FractionalPosition::create(xyz), "translated.position");
  }
  same_composition(c,
                   take(CrystalComposition::calculate(take(
                            CrystalStructure::create(structure.cell(),
                                                     structure.group(), sites),
                            "translated.structure")),
                        "translated.composition"),
                   true);
  for (auto &s : sites)
    s.position = take(FractionalPosition::create(
                          take(structure.group().operations().back().apply(
                                   s.position.coordinates()),
                               "symmetry.image")),
                      "equivalent.position");
  same_composition(c,
                   take(CrystalComposition::calculate(take(
                            CrystalStructure::create(structure.cell(),
                                                     structure.group(), sites),
                            "equivalent.structure")),
                        "equivalent.composition"),
                   true);
  auto parameters = structure.cell().parameters();
  for (auto &length : parameters.lengths_angstrom)
    length *= 2;
  auto larger = take(CrystalStructure::create(
                         take(UnitCell::create(parameters), "larger.cell"),
                         structure.group(), sites),
                     "larger.structure");
  auto larger_c =
      take(CrystalComposition::calculate(larger), "larger.composition");
  same_composition(c, larger_c, true);
  check(near(take(crystal_density(larger_c.cell_mass_daltons(),
                                  larger.cell().volume_angstrom_cubed()),
                  "larger.density")
                 .grams_per_cm3,
             density.grams_per_cm3 / 8),
        "volume.inverse.scaling");
  for (auto &s : sites)
    s.occupancy =
        take(Occupancy::create(s.occupancy.value() * .5), "scaled.occupancy");
  auto half = take(
      CrystalComposition::calculate(take(
          CrystalStructure::create(structure.cell(), structure.group(), sites),
          "half.structure")),
      "half.composition");
  check(near(half.cell_mass_daltons(), c.cell_mass_daltons() * .5),
        "occupancy.mass.scaling");
  sites.assign(structure.sites().begin(), structure.sites().end());
  sites.push_back(site("Np", {.17, .29, .43}, 0));
  auto with_zero = take(
      CrystalComposition::calculate(take(
          CrystalStructure::create(structure.cell(), structure.group(), sites),
          "zero-added.structure")),
      "zero-added.composition");
  check(with_zero.cell_mass_daltons() == c.cell_mass_daltons(),
        "occupancy.zero.mass");
  auto absorption = crystal_absorption(c, density, {});
  auto zero_absorption = crystal_absorption(with_zero, density, {});
  check(bool(absorption) == bool(zero_absorption),
        "zero.missing-absorption.ignored");
  if (absorption && zero_absorption)
    check(absorption.value().linear_absorption_per_cm ==
              zero_absorption.value().linear_absorption_per_cm,
          "zero.absorption.invariance");
  same_composition(
      c, take(CrystalComposition::calculate(structure), "repeat.composition"),
      true);
}
void full_pipeline(const CrystalStructure &structure, int id) {
  for (const auto friedel : {FriedelPolicy::merge, FriedelPolicy::separate})
    for (const bool anomalous : {false, true}) {
      PowderIntensityOptions options;
      options.radiation.anomalous = anomalous;
      ReflectionGenerationOptions generation;
      generation.wavelength = options.radiation.wavelength;
      generation.minimum_d = DSpacingAngstrom{.7};
      generation.minimum_two_theta = TwoThetaDegrees{1};
      generation.maximum_two_theta = TwoThetaDegrees{150};
      generation.friedel = friedel;
      auto refs = take(
          generate_reflections(structure.cell(), structure.group(), generation),
          "full.reflections");
      auto lines = take(powder_lines(structure, refs, options), "full.powder");
      auto card = take(
          TheoreticalReferenceCard::create(
              structure, lines,
              {"case-" + std::to_string(id), "chemical", "mineral", "fixture"},
              options.radiation, friedel),
          "full.card");
      ++pipelines;
      source_reflections += refs.reflections().size();
      if (pipelines == 1) {
        error_case("card_invalid_friedel",
                   TheoreticalReferenceCard::create(
                       structure, lines, {}, options.radiation,
                       static_cast<FriedelPolicy>(99)),
                   "card.friedel", "invalid_friedel_policy");
        const auto original = lines.lines().front().members.front();
        const auto singleton = [](const PowderReflection &member) {
          return PowderLine{member.identity,
                            member.reflection,
                            member.raw_intensity,
                            0,
                            {member}};
        };
        const auto attempt = [&](const PowderReflection &member) {
          auto altered = take(normalize_powder_lines({singleton(member)}),
                              "malformed.normalize");
          return TheoreticalReferenceCard::create(structure, altered, {},
                                                  options.radiation, friedel);
        };
        auto altered = original;
        altered.reflection.two_theta.value += .01;
        error_case("card_angle_mismatch", attempt(altered),
                   "card.line.two_theta", "reflection_radiation_mismatch");
        altered = original;
        ++altered.reflection.multiplicity;
        error_case("card_multiplicity_mismatch", attempt(altered),
                   "card.line.group", "reflection_group_mismatch");
        altered = original;
        altered.correction.combined *= 2;
        error_case("card_LP_mismatch", attempt(altered), "card.line.intensity",
                   "incompatible_powder_intensity");
        altered = original;
        altered.fc_squared *= 2;
        error_case("card_Fc2_raw_mismatch", attempt(altered),
                   "card.line.intensity", "incompatible_powder_intensity");
        altered = original;
        altered.identity += 1000000;
        auto duplicate = take(
            normalize_powder_lines({singleton(original), singleton(altered)}),
            "duplicate-hkl.normalize");
        error_case("card_duplicate_hkl",
                   TheoreticalReferenceCard::create(structure, duplicate, {},
                                                    options.radiation, friedel),
                   "card.line.hkl", "duplicate_reflection");
      }
      check(card.metadata().identifier == "case-" + std::to_string(id) &&
                card.metadata().chemical_name == "chemical" &&
                card.metadata().mineral_name == "mineral" &&
                card.metadata().provenance == "fixture",
            "metadata.copy");
      check(card.physical_lines().lines().size() == lines.lines().size(),
            "physical.lines.preserved");
      check(card.rir().value > 0 && std::isfinite(card.rir().value),
            "full.RIR.finite");
      check(card.presentation().input_slot_count == refs.reflections().size(),
            "full.slot.count");
      auto reversed =
          std::vector<PowderLine>(lines.lines().begin(), lines.lines().end());
      std::reverse(reversed.begin(), reversed.end());
      auto changed =
          take(normalize_powder_lines(reversed), "full.permuted.lines");
      auto repeat = take(
          TheoreticalReferenceCard::create(structure, changed, card.metadata(),
                                           options.radiation, friedel),
          "full.repeat");
      same_selection(card.presentation(), repeat.presentation());
      check(card.rir().value == repeat.rir().value, "full.RIR.repeat");
      auto reversed_sites = std::vector<AtomicSite>(structure.sites().begin(),
                                                    structure.sites().end());
      std::reverse(reversed_sites.begin(), reversed_sites.end());
      auto permuted_structure =
          take(CrystalStructure::create(structure.cell(), structure.group(),
                                        reversed_sites),
               "full.permuted-structure");
      auto permuted_lines =
          take(powder_lines(permuted_structure, refs, options),
               "full.permuted-powder");
      auto permuted_card =
          take(TheoreticalReferenceCard::create(permuted_structure,
                                                permuted_lines, card.metadata(),
                                                options.radiation, friedel),
               "full.permuted-card");
      check(card.rir().value == permuted_card.rir().value,
            "full.atom-order.RIR");
      same_selection(card.presentation(), permuted_card.presentation());
      generation.order = ReflectionOrder::hkl;
      auto hkl_refs = take(
          generate_reflections(structure.cell(), structure.group(), generation),
          "full.hkl-reflections");
      auto hkl_lines =
          take(powder_lines(structure, hkl_refs, options), "full.hkl-lines");
      same_selection(card.presentation(),
                     take(TheoreticalReferenceCard::create(
                              structure, hkl_lines, card.metadata(),
                              options.radiation, friedel),
                          "full.hkl-card")
                         .presentation());
      error_case(
          "card_neutron",
          TheoreticalReferenceCard::create(
              structure, lines, {},
              {RadiationKind::neutron, WavelengthAngstrom{1.54059307}, false},
              friedel),
          "card.radiation", "unsupported_rir_radiation");
      error_case("card_custom_wave",
                 TheoreticalReferenceCard::create(
                     structure, lines, {},
                     {RadiationKind::xray, WavelengthAngstrom{.71073}, false},
                     friedel),
                 "card.wavelength", "unsupported_rir_wavelength");
      auto wrong_cell = structure.cell().parameters();
      for (auto &x : wrong_cell.lengths_angstrom)
        x *= 1.01;
      auto wrong =
          take(CrystalStructure::create(
                   take(UnitCell::create(wrong_cell), "wrong.cell"),
                   structure.group(),
                   {structure.sites().begin(), structure.sites().end()}),
               "wrong.structure");
      error_case("card_cell_mismatch",
                 TheoreticalReferenceCard::create(wrong, lines, {},
                                                  options.radiation, friedel),
                 "card.line.d", "reflection_cell_mismatch");
      std::cout << "PIPELINE " << id << ' ' << static_cast<int>(friedel) << ' '
                << anomalous << ' ' << refs.reflections().size() << ' '
                << card.presentation().lines.size() << '\n';
    }
}
double number() {
  std::string s;
  std::cin >> s;
  return std::stod(s);
}
template <std::size_t N> std::array<double, N> numbers() {
  std::array<double, N> x{};
  for (auto &v : x)
    v = number();
  return x;
}
} // namespace
int main() {
  std::cout << std::setprecision(17);
  try {
    error_properties();
    selection_properties();
    int id, number_group, setting, full;
    std::size_t n;
    while (std::cin >> id >> number_group >> setting >> n >> full) {
      const auto parameters = numbers<6>();
      auto cell = take(
          UnitCell::create({{parameters[0], parameters[1], parameters[2]},
                            {parameters[3], parameters[4], parameters[5]}}),
          "fixture.cell");
      auto group = take(SpaceGroupRegistry{}.by_number(number_group, setting),
                        "fixture.group");
      std::vector<AtomicSite> sites, mass_sites;
      for (std::size_t i = 0; i < n; ++i) {
        std::string label;
        int adp;
        std::cin >> label;
        const auto xyz = numbers<3>();
        const double occupancy = number();
        std::cin >> adp;
        const auto tensor = numbers<6>();
        auto s = site(label, xyz, occupancy);
        mass_sites.push_back(s);
        s.displacement =
            adp == 0   ? take(Displacement::isotropic_b(tensor[0]), "fixture.B")
            : adp == 1 ? take(Displacement::isotropic_u(tensor[0]), "fixture.U")
            : adp == 2
                ? take(Displacement::anisotropic_u(tensor), "fixture.Uij")
                : take(Displacement::anisotropic_beta(tensor), "fixture.beta");
        sites.push_back(s);
      }
      composition_properties(
          take(CrystalStructure::create(cell, group, mass_sites),
               "fixture.mass-structure"));
      if (full)
        full_pipeline(take(CrystalStructure::create(cell, group, sites),
                           "fixture.physical-structure"),
                      id);
      std::cout << "FIXTURE " << id << '\n';
    }
  } catch (const std::exception &e) {
    ++failed;
    std::cerr << e.what() << '\n';
  }
  std::cout << "SUMMARY " << checks << ' ' << failed << ' ' << pipelines << ' '
            << source_reflections << '\n';
  return failed ? 1 : 0;
}
