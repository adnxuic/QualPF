#include "qualpf/crystallography/theoretical_card.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

using namespace qualpf;
using namespace qualpf::crystallography;
namespace {
double number() {
  std::string s;
  std::cin >> s;
  return std::stod(s);
}
template <std::size_t N> std::array<double, N> numbers() {
  std::array<double, N> values{};
  for (auto &v : values)
    v = number();
  return values;
}
void failure(const char *stage, const Error &error) {
  std::cout << "ERROR " << stage << ' ' << error.field << ' ' << error.reason
            << '\n';
}
std::optional<CrystalDensity> composition(const CrystalStructure &structure,
                                          ScatteringRadiation radiation) {
  auto c = CrystalComposition::calculate(structure);
  if (!c) {
    failure("composition", c.error());
    return {};
  }
  auto d = crystal_density(c.value().cell_mass_daltons(),
                           structure.cell().volume_angstrom_cubed());
  if (!d) {
    failure("density", d.error());
    return {};
  }
  const auto &f = c.value().formula();
  std::cout << "COMPOSITION " << f.formula_units_per_cell << ' '
            << c.value().cell_mass_daltons() << ' ' << f.mass_daltons << ' '
            << d.value().volume_angstrom_cubed << ' ' << d.value().grams_per_cm3
            << '\n';
  std::cout << "FORMULA " << f.display << '\n';
  for (const auto &e : c.value().entries())
    std::cout << "ENTRY " << e.species.index() + 1 << ' '
              << e.element.atomic_number << ' ' << e.species.label() << ' '
              << e.unit_cell_amount << ' ' << e.formula_amount << ' '
              << e.atomic_mass_daltons << ' ' << e.cell_mass_daltons << '\n';
  std::size_t i = 0;
  for (const auto &s : structure.sites()) {
    auto images = expand_site(s, structure.cell(), structure.group());
    if (!images) {
      failure("site", images.error());
      continue;
    }
    std::cout << "SITE " << ++i << ' ' << s.occupancy.value() << ' '
              << images.value().size() << ' '
              << s.occupancy.value() *
                     static_cast<double>(images.value().size())
              << '\n';
  }
  auto absorption = crystal_absorption(c.value(), d.value(), radiation);
  if (!absorption)
    failure("absorption", absorption.error());
  else {
    const auto &a = absorption.value();
    std::cout << "ABSORPTION " << a.energy_kev << ' '
              << a.mass_attenuation_cm2_per_g << ' '
              << a.linear_absorption_per_cm << '\n';
    for (const auto &s : a.species) {
      const auto &m = s.attenuation;
      std::cout << "MAC " << s.species.index() + 1 << ' ' << m.lower.energy_kev
                << ' ' << m.lower.mass_attenuation_cm2_per_g << ' '
                << m.upper.energy_kev << ' '
                << m.upper.mass_attenuation_cm2_per_g << ' ' << m.log_fraction
                << ' ' << m.cm2_per_g << ' ' << s.weighted_attenuation << '\n';
    }
  }
  return d.value();
}
} // namespace
int main() {
  std::cout << std::setprecision(17);
  int cid, group_number, setting;
  std::size_t na, n, ng;
  while (std::cin >> cid >> group_number >> setting >> na) {
    ScatteringRadiation radiation;
    radiation.wavelength = WavelengthAngstrom{number()};
    std::cin >> n >> ng;
    const auto cell_values = numbers<6>();
    auto cell =
        UnitCell::create({{cell_values[0], cell_values[1], cell_values[2]},
                          {cell_values[3], cell_values[4], cell_values[5]}});
    auto group = SpaceGroupRegistry{}.by_number(group_number, setting);
    std::vector<AtomicSite> sites;
    std::optional<Error> invalid;
    for (std::size_t i = 0; i < na; ++i) {
      std::string label;
      std::cin >> label;
      const auto xyz = numbers<3>();
      const double occ = number();
      auto species = ScatteringRegistry{}.find(label);
      auto pos = FractionalPosition::create(xyz);
      auto occupancy = Occupancy::create(occ);
      if (!species)
        invalid = species.error();
      else if (!pos)
        invalid = pos.error();
      else if (!occupancy)
        invalid = occupancy.error();
      else
        sites.push_back({species.value(), pos.value(), occupancy.value(),
                         Displacement::isotropic_b(0).value()});
    }
    std::vector<PowderReflection> slots;
    std::vector<double> slot_intensities;
    for (std::size_t i = 0; i < n; ++i) {
      ReflectionIdentity id;
      Reflection ref{{0, 0, 0}, DSpacingAngstrom{0}, TwoThetaDegrees{0}, 0};
      std::cin >> id >> ref.hkl.h >> ref.hkl.k >> ref.hkl.l >> ref.multiplicity;
      ref.d = DSpacingAngstrom{number()};
      ref.two_theta = TwoThetaDegrees{number()};
      const double fc2 = number(), lp = number(), raw = number();
      slots.push_back({id, ref, fc2, {1, 1, lp}, raw});
      slot_intensities.push_back(raw);
    }
    std::vector<PowderLine> lines;
    for (std::size_t i = 0; i < ng; ++i) {
      std::size_t representative, count;
      std::cin >> representative >> count;
      if (representative == 0 || representative > slots.size() || count == 0)
        return 2;
      std::vector<PowderReflection> members;
      for (std::size_t j = 0; j < count; ++j) {
        std::size_t index;
        std::cin >> index;
        if (index == 0 || index > slots.size())
          return 2;
        auto member = slots[index - 1];
        // Transport the native grouped slot total. Native binary32 summation
        // cannot be reconstructed as an exact binary64 sum of Fc2 components.
        // This isolates selection; full physical pipelines run in properties.
        member.raw_intensity =
            index == representative ? slot_intensities[index - 1] : 0;
        members.push_back(member);
      }
      lines.push_back({slots[representative - 1].identity,
                       slots[representative - 1].reflection,
                       slot_intensities[representative - 1], 0,
                       std::move(members)});
    }
    std::cout << "CASE " << cid << '\n';
    std::optional<CrystalDensity> density;
    if (!cell)
      failure("composition", cell.error());
    else if (!group)
      failure("composition", group.error());
    else if (invalid)
      failure("composition", *invalid);
    else {
      auto structure = CrystalStructure::create(cell.value(), group.value(),
                                                std::move(sites));
      if (!structure)
        failure("composition", structure.error());
      else
        density = composition(structure.value(), radiation);
    }
    auto normalized = normalize_powder_lines(std::move(lines));
    if (!normalized)
      failure("lines", normalized.error());
    else {
      double maximum = 0;
      for (const auto &l : normalized.value().lines())
        maximum = std::max(maximum, l.raw_intensity);
      if (density) {
        auto rir = legacy_cu_reference_intensity_ratio(*density, maximum);
        if (!rir)
          failure("rir", rir.error());
        else {
          const auto &r = rir.value();
          std::cout << "RIR " << r.maximum_raw_intensity << ' '
                    << r.density_volume_squared << ' ' << r.density_volume_ratio
                    << ' ' << r.intensity_ratio << ' ' << r.value << '\n';
        }
      }
      auto selection = select_database_lines(normalized.value());
      if (!selection)
        failure("selection", selection.error());
      else {
        const auto &s = selection.value();
        std::cout << "SELECT " << s.input_slot_count << ' '
                  << s.prefix_slot_count << ' ' << s.lines.size() << ' '
                  << s.physical_maximum << ' ' << s.prefix_maximum << ' '
                  << s.threshold << '\n';
        for (const auto &d : s.decisions)
          std::cout << "DECISION " << d.identity << ' ' << d.angular_slot << ' '
                    << d.line_representative_identity << ' ' << d.raw_intensity
                    << ' ' << d.representative << ' ' << d.within_prefix << ' '
                    << d.meets_threshold << ' ' << d.retained << '\n';
        for (const auto &l : s.lines) {
          std::cout << "CARD " << l.physical_line.representative_identity << ' '
                    << l.angular_slot << ' ' << l.physical_line.raw_intensity
                    << ' ' << l.card_relative_intensity << ' '
                    << l.physical_line.relative_intensity << ' '
                    << l.physical_line.members.size();
          for (const auto &m : l.physical_line.members)
            std::cout << ' ' << m.identity;
          std::cout << '\n';
        }
      }
    }
    std::cout << "END\n";
  }
  return std::cin.eof() ? 0 : 1;
}
