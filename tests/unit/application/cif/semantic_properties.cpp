#include "qualpf/application/database/cif_card_service.h"
#include <iostream>
#include <stdexcept>

using namespace qualpf::application;
namespace {
int checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
const std::string base =
    "data_a\n_cell_length_a 5\n_cell_length_b 5\n_cell_length_c 5\n"
    "_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma 90\n"
    "_space_group_name_H-M_alt 'P -1'\n";
const std::string atoms =
    "loop_\n_atom_site_label\n_atom_site_type_symbol\n_atom_site_fract_x\n"
    "_atom_site_fract_y\n_atom_site_fract_z\n";
CifCardResult run(const std::string &text) {
  return CifCardService::from_bytes(text, "semantic.cif");
}
void rejected(const std::string &text, const char *code) {
  auto result = run(text);
  const auto *error = std::get_if<CifCardIssue>(&result);
  check(error && error->stage == CifCardStage::import,
        "atomic import failure, no downstream card");
  check(error->code == code, "stable semantic rejection code");
  check(error->source == "semantic.cif" && !error->block.empty() &&
            !error->field.empty() && error->location.line > 0 &&
            !error->message.empty(),
        "located diagnostic");
}
const CifCard &success(const CifCardResult &result) {
  const auto *card = std::get_if<CifCard>(&result);
  if (!card)
    throw std::runtime_error(std::get<CifCardIssue>(result).message);
  return *card;
}
} // namespace
int main() {
  try {
    for (const auto *tag : {"_atom_site_symmetry_multiplicity",
                            "_atom_site_site_symmetry_multiplicity",
                            "_atom_site.site_symmetry_multiplicity",
                            "_atom_site.symmetry_multiplicity"}) {
      auto result = run(base + atoms + tag + "\nSi1 Si .123(4) .234 .345 2\n");
      const auto &site = success(result).imported().sites[0];
      check(site.declared_multiplicity == 2 && site.orbit_multiplicity == 2,
            "multiplicity matches orbit");
      check(site.source_coordinates[0].raw == ".123(4)",
            "nominal and uncertainty token retained");
      rejected(base + atoms + tag + "\nSi1 Si .123 .234 .345 1\n",
               "site_symmetry_inconsistent");
    }
    rejected(
        base + atoms +
            "_atom_site_symmetry_multiplicity\nSi1 Si .123 .234 .345 1.5\n",
        "invalid_site_multiplicity");
    // SU is not a hard interval and cannot authorize moving a measured value.
    rejected(base + atoms +
                 "_atom_site_symmetry_multiplicity\nSi1 Si .0001(2) 0 0 1\n",
             "site_symmetry_ambiguous");
    auto exact = run(base + atoms +
                     "_atom_site_symmetry_multiplicity\nSi1 Si 0 0 0 1\n");
    check(success(exact).imported().sites[0].orbit_multiplicity == 1,
          "exact special position retained");
    auto formula =
        base + "_chemical_formula_sum 'Si'\n_cell_formula_units_Z 2\n";
    auto good = run(formula + atoms + "Si1 Si .123 .234 .345\n");
    check(success(good).card().density().grams_per_cm3 > 0,
          "formula times Z agrees with occupancy totals");
    rejected(formula + atoms + "Si1 Si 0 0 0\n", "site_symmetry_inconsistent");
    auto partial =
        run(base + "_chemical_formula_sum 'Si0.5'\n_cell_formula_units_Z 2\n" +
            atoms + "_atom_site_occupancy\nSi1 Si .123 .234 .345 .5\n");
    check(success(partial).imported().structure.sites()[0].occupancy.value() ==
              .5,
          "partial occupancy participates in cell balance");
    rejected(base + "_chemical_formula_sum '(Si)'\n_cell_formula_units_Z 2\n" +
                 atoms + "Si1 Si .1 .2 .3\n",
             "unsupported_formula_sum");
    rejected(base + "_chemical_formula_sum 'Si'\n_cell_formula_units_Z 0\n" +
                 atoms + "Si1 Si .1 .2 .3\n",
             "invalid_formula_units");
    rejected(base +
                 "_chemical_formula_sum 'Si10'\n_cell_formula_units_Z 1e308\n" +
                 atoms + "Si1 Si .1 .2 .3\n",
             "invalid_formula_sum");
    auto owned = run("data_metadata\n_chemical_name_common unrelated\n" + base +
                     "_chemical_name_common owned\n" + atoms +
                     "Si1 Si .1 .2 .3\n"
                     "data_footer\n_chemical_name_common ignored\n");
    check(success(owned).imported().metadata.chemical_name == "owned",
          "only atom block owns metadata");
    auto second = base;
    second.replace(second.find("data_a"), 6, "data_b");
    rejected(base + atoms + "Si1 Si .1 .2 .3\n" + second + atoms +
                 "Si1 Si .1 .2 .3\n",
             "multiple_structure_blocks");
    // Phase 19B require_unique counts importable structures, retaining the
    // malformed block in the candidate DTO for independent inspection.
    auto unique = run("data_bad\nloop_\n_atom_site_fract_y\n0.2\n" + base +
                      atoms + "Si1 Si .1 .2 .3\n");
    check(success(unique).imported().metadata.block == "a",
          "unique importable block contract");
    auto dummy = run(
        base + "_chemical_formula_sum 'Si'\n_cell_formula_units_Z 2\n" + atoms +
        "_atom_site_calc_flag\nGhost Xx ? ? ? dum\nSi1 Si .1 .2 .3 d\n");
    check(success(dummy).imported().sites.size() == 1 &&
              success(dummy).imported().notices.size() == 1,
          "dummy with meaningless species/coordinates excluded before physical "
          "validation");
    auto missingDummyLabel =
        run(base + atoms +
            "_atom_site_calc_flag\n? Xx ? ? ? dum\nSi1 Si .1 .2 .3 d\n");
    check(
        success(missingDummyLabel).imported().sites.size() == 1,
        "dummy with missing label remains excluded before physical validation");
    std::cout << "PASS " << checks << " checks\n";
  } catch (const std::exception &error) {
    std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
