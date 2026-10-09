#include "qualpf/application/database/cif_card_record.h"
#include "cif_classification_data.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace qualpf::application {
namespace {
std::string trim(std::string_view value) {
  constexpr auto whitespace = " \t\r\n\f\v";
  const auto first = value.find_first_not_of(whitespace);
  if (first == std::string_view::npos)
    return {};
  return std::string(
      value.substr(first, value.find_last_not_of(whitespace) - first + 1));
}
bool organic_element(int z) {
  switch (z) {
  case 1:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 15:
  case 16:
  case 17:
  case 35:
  case 53:
    return true;
  default:
    return false;
  }
}
struct ElementCount {
  int atomic_number;
  std::size_t species_index;
  std::string symbol;
  std::int64_t rounded_cell_count;
};
std::string subfile(const std::vector<ElementCount> &elements) {
  bool carbon = false;
  std::int64_t inorganic = 0, total = 0, oxygen = 0, oxygen_inorganic = 0;
  bool found_oxygen = false;
  for (const auto &e : elements) {
    const auto z = e.atomic_number;
    carbon = carbon || z == 6;
    total += e.rounded_cell_count;
    const bool metal = !organic_element(z);
    if (metal)
      inorganic += e.rounded_cell_count;
    // The old helper takes only the first matching oxygen entry. Entries
    // have a deterministic atomic-number/species-index order here.
    if (z == 8 && !found_oxygen) {
      oxygen = e.rounded_cell_count;
      found_oxygen = true;
    }
    if ((metal && e.rounded_cell_count > 0) || z == 15) {
      const int oxidation =
          z >= 1 && z <= 108 ? detail::common_oxidation_number[z - 1] : 0;
      // Fortran NINT is nearest, ties away from zero. This is a small
      // integer table calculation; do not derive it from formula text.
      oxygen_inorganic +=
          static_cast<std::int64_t>(std::round(.7 * oxidation)) *
          e.rounded_cell_count;
    }
  }
  if (!carbon)
    return "inorganic";
  if (inorganic == 0)
    return "organic";
  if (oxygen > 0)
    inorganic += std::min(oxygen_inorganic, oxygen);
  // Counts are nonnegative bounded int32 values; a nonzero inorganic sum
  // implies total>0. Use int64 to avoid Legacy's possible int32 overflow.
  return 100 * inorganic / total <= 20 ? "metallorganic" : "inorganic";
}
std::string crystal_system(int number, const std::array<double, 6> &cell) {
  if (number <= 2)
    return "Triclinic";
  if (number <= 15)
    return "Monoclinic";
  if (number <= 74)
    return "Orthorhombic";
  if (number <= 142)
    return "Tetragonal";
  if (number <= 167) {
    if (std::abs(cell[0] - cell[2]) + std::abs(cell[1] - cell[2]) +
            std::abs(cell[3] - cell[5]) + std::abs(cell[4] - cell[5]) <
        .01)
      return "Trigonal (rhombohedral axes)";
    if (std::abs(cell[0] - cell[1]) + std::abs(cell[3] - 90) +
            std::abs(cell[4] - 90) + std::abs(cell[5] - 120) <
        .01)
      return "Trigonal (hexagonal axes)";
    return "Trigonal";
  }
  if (number <= 194)
    return "Hexagonal";
  return "Cubic";
}
} // namespace

CifRecordResult cif_card_record(const CifCard &calculated,
                                std::int32_t identity) {
  const auto &card = calculated.card();
  const auto &imported = calculated.imported();
  const auto fail = [&](std::string field,
                        std::string reason) -> CifRecordResult {
    return database::WriteError{"card_record_range", imported.metadata.source,
                                imported.metadata.block, std::move(field),
                                std::move(reason)};
  };
  constexpr auto limit = std::numeric_limits<std::int32_t>::max();
  if (imported.structure.sites().size() > static_cast<std::size_t>(limit) ||
      calculated.reflection_count() > static_cast<std::size_t>(limit) ||
      card.composition().formula().formula_units_per_cell >
          static_cast<std::uint64_t>(limit))
    return fail("counts",
                "Card counts exceed the database integer representation");
  database::CifRecord record;
  record.identity = identity;
  record.source = imported.metadata.source;
  record.block = imported.metadata.block;
  record.source_sha256 = imported.provenance.source_sha256;
  record.structure_identity = imported.provenance.identity();
  record.provenance = io::cif::provenance_ledger(imported.provenance);
  record.name = trim(card.metadata().chemical_name);
  record.mineral = trim(card.metadata().mineral_name);
  record.formula = card.composition().formula().display;
  record.space_group = trim(imported.structure.group().hermann_mauguin());
  // The native CIF product omits zero-occupancy sites from its reported atom
  // count. Keep them in import provenance, but report only contributing sites.
  record.atom_count = static_cast<std::int32_t>(std::count_if(
      imported.structure.sites().begin(), imported.structure.sites().end(),
      [](const auto &site) { return site.occupancy.value() > 0; }));
  record.reflection_count =
      static_cast<std::int32_t>(calculated.reflection_count());
  record.formula_units = static_cast<std::int32_t>(
      card.composition().formula().formula_units_per_cell);
  std::copy(card.cell().lengths_angstrom.begin(),
            card.cell().lengths_angstrom.end(), record.cell.begin());
  std::copy(card.cell().angles_degrees.begin(),
            card.cell().angles_degrees.end(), record.cell.begin() + 3);
  record.crystal_system =
      crystal_system(imported.structure.group().number(), record.cell);
  record.volume = card.density().volume_angstrom_cubed;
  record.density = card.density().grams_per_cm3;
  record.linear_absorption = card.absorption().linear_absorption_per_cm;
  record.rir = card.rir().value;
  std::vector<ElementCount> elements;
  for (const auto &entry : card.composition().entries()) {
    if (entry.unit_cell_amount == 0)
      continue;
    const double count = std::round(entry.unit_cell_amount);
    if (!std::isfinite(count) || count < 0 || count > limit)
      return fail(
          "composition.amount",
          "Rounded cell content exceeds the classification integer bound");
    elements.push_back({entry.element.atomic_number, entry.species.index(),
                        std::string(entry.element.symbol),
                        static_cast<std::int64_t>(count)});
  }
  std::sort(elements.begin(), elements.end(), [](const auto &a, const auto &b) {
    return std::tie(a.atomic_number, a.species_index) <
           std::tie(b.atomic_number, b.species_index);
  });
  for (const auto &entry : elements)
    record.elements.push_back(entry.symbol);
  record.subfile = subfile(elements);
  record.lines.reserve(card.presentation().lines.size());
  for (const auto &line : card.presentation().lines) {
    const auto &reflection = line.physical_line.representative;
    if (reflection.multiplicity > static_cast<std::size_t>(limit))
      return fail("lines.multiplicity",
                  "Multiplicity exceeds the database integer representation");
    record.lines.push_back(
        {{reflection.hkl.h, reflection.hkl.k, reflection.hkl.l},
         static_cast<std::int32_t>(reflection.multiplicity),
         reflection.d.value,
         line.card_relative_intensity});
  }
  return record;
}
} // namespace qualpf::application
