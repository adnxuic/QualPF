#include "qualpf/application/database/cif_card_service.h"
#include "cif_parser.h"
#include "cif_site_symmetry.h"
#include <iostream>
#include <stdexcept>

namespace cif = qualpf::io::cif;
namespace {
int checks = 0;
void check(bool v, const char *why) {
  ++checks;
  if (!v)
    throw std::runtime_error(why);
}
const std::string base =
    "data_a\n_cell_length_a 3.25\n_cell_length_b 3.25\n_cell_length_c 5.2\n"
    "_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma "
    "120\n_space_group_IT_number 186\n";
const std::string tags =
    "loop_\n_atom_site_label\n_atom_site_type_symbol\n_atom_site_fract_x\n_"
    "atom_site_fract_y\n_atom_site_fract_z\n";
cif::ImportResult<cif::ImportedCrystal> run(const std::string &text) {
  auto doc = cif::parse(text, "site-evidence.cif");
  check(std::holds_alternative<cif::Document>(doc), "fixture syntax");
  return cif::import_cif(std::get<cif::Document>(doc), {});
}
const cif::ImportedCrystal &
ok(const cif::ImportResult<cif::ImportedCrystal> &v) {
  if (auto e = std::get_if<cif::ImportIssue>(&v))
    throw std::runtime_error(e->code + ":" + e->message);
  return std::get<cif::ImportedCrystal>(v);
}
void rejects(std::string text, const char *code) {
  auto r = run(text);
  auto e = std::get_if<cif::ImportIssue>(&r);
  check(e && e->code == code, "stable atomic site rejection");
  check(e->provenance && e->provenance->selected_block == "a",
        "rejection preserves block evidence");
}
} // namespace
int main() {
  try {
    auto r =
        run(base + tags +
            "_atom_site_symmetry_multiplicity\nZn1 Zn .3333 .6667 .123(4) 2\n");
    const auto &s = ok(r);
    const auto &e = s.provenance.sites[0].site_symmetry;
    check(e.state == "UNIQUE_RECONCILABLE" && e.nominal_orbit == 6 &&
              e.reconciled_orbit == 2,
          "rounded fixed coordinates");
    check(e.reconciled_fractional ==
              std::array<std::string, 3>{"1/3", "2/3", "123/1000"},
          "free measured z retained");
    check(e.delta[0] == "1/30000" && e.delta[1] == "-1/30000" &&
              e.delta[2] == "0",
          "exact signed deltas");
    check(e.coordinates[2].standard_uncertainty == "1/250",
          "SU has last-digit scale");
    check(s.provenance.sites[0].coordinates[0].raw == ".3333" &&
              s.sites[0].input_fractional[0] == .3333,
          "raw and nominal never overwritten");
    auto exact =
        run(base + tags +
            "_atom_site_symmetry_multiplicity\nZn1 Zn 0 0 .123(4) 2\n");
    check(ok(exact).provenance.sites[0].site_symmetry.state == "EXACT",
          "exact special position unchanged");
    rejects(
        base + tags +
            "_atom_site_symmetry_multiplicity\nZn1 Zn .3333(1) .6667 .123 2\n",
        "site_symmetry_ambiguous");
    rejects(base + tags +
                "_atom_site_symmetry_multiplicity\nZn1 Zn .3331 .6667 .123 2\n",
            "site_symmetry_inconsistent");
    rejects(base + tags +
                "_atom_site_symmetry_multiplicity\nZn1 Zn .3333 .6667 .123 4\n",
            "site_symmetry_inconsistent");
    rejects(base + tags +
                "_atom_site_symmetry_multiplicity\n_atom_site_Wyckoff_"
                "symbol\nZn1 Zn .3333 .6667 .123 2 a\n",
            "site_symmetry_inconsistent");
    auto general = run(base + tags + "Zn1 Zn .3333 .6667 .123\n");
    check(ok(general).provenance.sites[0].site_symmetry.state == "EXACT" &&
              ok(general).sites[0].orbit_multiplicity == 6,
          "precision alone cannot snap");
    auto derived =
        run(base + "_chemical_formula_sum 'Zn'\n_cell_formula_units_Z 2\n" +
            tags + "Zn1 Zn .3333 .6667 .123\n");
    check(ok(derived).provenance.sites[0].site_symmetry.constraint_source ==
              "formula_Z_single_site_element_balance",
          "derived count distinguished from declaration");
    check(!ok(derived).sites[0].declared_multiplicity,
          "no invented source multiplicity");
    auto two =
        run(base + "_chemical_formula_sum 'Zn6'\n_cell_formula_units_Z 2\n" +
            tags + "Zn1 Zn .3333 .6667 .123\nZn2 Zn .3333 .6667 .456\n");
    check(ok(two).sites[0].orbit_multiplicity == 6 &&
              ok(two).sites[1].orbit_multiplicity == 6,
          "multiple same-element rows forbid site-count inference");
    auto shifted = run(
        base + tags +
        "_atom_site_symmetry_multiplicity\nZn1 Zn 10.3333 -9.3333 .123 2\n");
    check(ok(shifted)
                  .provenance.sites[0]
                  .site_symmetry.reconciled_fractional[0] == "31/3",
          "lattice-translated intervals");
    // Exact inversion centres at both ends of the decimal half-step are not
    // an authorized choice of rounding tie convention.
    const std::string inversion =
        "data_a\n_cell_length_a 5\n_cell_length_b 5\n_cell_length_c "
        "5\n_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma "
        "90\n_space_group_IT_number 2\n";
    rejects(inversion + tags +
                "_atom_site_symmetry_multiplicity\nZn1 Zn 0.25 0 0 1\n",
            "site_symmetry_inconsistent");
    rejects(inversion + tags +
                "_atom_site_symmetry_multiplicity\nZn1 Zn 9223372036854775807 "
                ".25 .25 1\n",
            "site_symmetry_ambiguous");
    rejects(base + tags +
                "_atom_site_Wyckoff_symbol\n_atom_site.Wyckoff_symbol\nZn1 Zn "
                "0 0 .123 a b\n",
            "site_symmetry_ambiguous");
    auto doc = cif::parse(
        base + tags +
            "_atom_site_symmetry_multiplicity\nZn1 Zn .3333 .6667 .123(4) 2\n",
        "site-evidence.cif");
    auto ticket = cif::save_selection(std::get<cif::Document>(doc), s);
    check(std::holds_alternative<std::string>(ticket), "new evidence saves");
    auto restored = cif::reload_selection(std::get<std::string>(ticket));
    check(cif::provenance_ledger(ok(restored).provenance) ==
              cif::provenance_ledger(s.provenance),
          "reconciliation ledger round trip");
    std::cout << "PASS " << checks << " checks\n";
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
