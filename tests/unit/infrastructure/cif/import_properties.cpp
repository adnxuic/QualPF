#include "cif_import.h"
#include "cif_parser.h"
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace qualpf::io::cif;
namespace {
int checks = 0;
void check(bool ok, const char *label) {
    ++checks;
    if (!ok)
        throw std::runtime_error(label);
}
const std::string base =
    "data_test\n_cell_length_a 5\n_cell_length_b 5\n_cell_length_c 5\n"
    "_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma 90\n_symmetry_space_group_name_H-M 'P 1'\n";
const std::string columns = "loop_\n_atom_site_label\n_atom_site_type_symbol\n_atom_site_fract_x\n_atom_site_"
                            "fract_y\n_atom_site_fract_z\n";
ImportResult<ImportedCrystal> import(std::string text) {
    const auto parsed = parse(text, "test.cif");
    if (const auto *error = std::get_if<Error>(&parsed))
        throw std::runtime_error(error->reason);
    return import_for_theoretical_card(std::get<Document>(parsed));
}
ImportedCrystal accepted(std::string text) {
    auto result = import(std::move(text));
    if (const auto *error = std::get_if<ImportIssue>(&result))
        throw std::runtime_error(error->code + ": " + error->message);
    return std::move(std::get<ImportedCrystal>(result));
}
bool near(double a, double b) { return std::abs(a - b) < 1e-12; }
} // namespace
int main() {
    try {
        auto missing = accepted(base + columns + "H1 H -.1 1.2 .3\nO1 O .4 .5 .6\n");
        check(missing.sites.size() == 2, "site count");
        check(missing.sites[0].imported_b_iso == 4 && missing.sites[1].imported_b_iso == 1,
              "missing ADP import defaults");
        check(missing.sites[0].default_occupancy && missing.structure.sites()[0].occupancy.value() == 1,
              "missing occupancy");
        check(near(missing.structure.sites()[0].position.coordinates()[0], .9),
              "fractional wrapping uses Core");
        for (auto value : {"0", "-1", "101"}) {
            auto result =
                accepted(base + columns + "_atom_site_B_iso_or_equiv\nO1 O .1 .2 .3 " + value + "\n");
            auto beta = result.structure.sites()[0].displacement.beta(result.structure.cell()).value();
            check(result.sites[0].default_card_adp, "card replaces explicit nonpositive/large B");
            check(near(qualpf::crystallography::equivalent_isotropic_b(beta, result.structure.cell()).value(),
                       3),
                  "oxygen card B is three");
        }
        auto u = accepted(base + columns +
                          "_atom_site_B_iso_or_equiv\n_atom_site_U_iso_or_equiv\nSi1 Si .1 .2 .3 5 .02\n");
        check(near(u.sites[0].imported_b_iso, 8 * std::numbers::pi * std::numbers::pi * .02),
              "U takes precedence over B");
        auto umissing = accepted(
            base + columns + "_atom_site_B_iso_or_equiv\n_atom_site_U_iso_or_equiv\nSi1 Si .1 .2 .3 5 ?\n");
        check(umissing.sites[0].imported_b_iso == 1, "present missing U column suppresses B column");
        auto declared = accepted(
            base + columns + "_atom_site_U_iso_or_equiv\n_atom_site_adp_type\nSi1 Si .1 .2 .3 .02 Biso\n");
        check(declared.sites[0].imported_b_iso == .02, "explicit ADP type controls units");
        for (const char *invalid : {"no", "?"}) {
            const auto result = import(base + columns + "Si1 Si .1 .2 " + invalid + "\n");
            const auto *error = std::get_if<ImportIssue>(&result);
            check(error && error->field == "_atom_site_fract_z" && error->block == "test" &&
                      error->source == "test.cif",
                  "invalid coordinate has source context");
        }
        const auto occupancy = import(base + columns + "_atom_site_occupancy\nSi1 Si .1 .2 .3 1.1\n");
        check(std::get<ImportIssue>(occupancy).core_reason == "invalid_occupancy",
              "Core occupancy rejection preserved");
        auto nearest = accepted(base + columns + "Fe1 Fe9+ .1 .2 .3\n");
        check(nearest.structure.sites()[0].species.label() == "Fe6+",
              "observed Legacy registered-charge mapping");
        auto fallback = accepted(base + columns + "Fe1 Xx .1 .2 .3\n");
        check(fallback.structure.sites()[0].species.label() == "Fe",
              "unknown type falls back to valid atom label");
        auto explicitGroup =
            accepted(base + columns +
                     "Si1 Si .1 .2 .3\nloop_\n_space_group_symop_operation_xyz\n'x,y,z'\n'-x,-y,-z'\n");
        check(explicitGroup.structure.group().number() == 2 && !explicitGroup.notices.empty(),
              "explicit known operators override HM with notice");
        Value operation{"-x+y+1/2,z,y-1/3", {}, ValueKind::single_quoted, {}};
        auto op = parse_operation(operation);
        check(std::holds_alternative<qualpf::crystallography::SymmetryOperation>(op),
              "exact rational operator");
        check(std::get<qualpf::crystallography::SymmetryOperation>(op).translation_twelfths() ==
                  std::array<std::int32_t, 3>{6, 0, 8},
              "rational shifts in twelfths");
        for (const char *s : {"x,y", "x,y,z,1", "x+1/0,y,z", "x+1/5,y,z", "x,y,garbage", "x,y,y"}) {
            operation.text = s;
            check(std::holds_alternative<ImportIssue>(parse_operation(operation)),
                  "malformed/nonrepresentable operator rejected");
        }
        const std::string anis =
            "loop_\n_atom_site_aniso_label\n_atom_site_aniso_U_11\n_atom_site_aniso_U_22\n_atom_site_aniso_U_"
            "33\n_atom_site_aniso_U_12\n_atom_site_aniso_U_13\n_atom_site_aniso_U_23\n";
        auto tensor = accepted(base + columns + "Si1 Si .1 .2 .3\n" + anis + "Si1 .01 .01 .01 0 0 0\n");
        check(near(tensor.sites[0].imported_b_iso, 8 * std::numbers::pi * std::numbers::pi * .01),
              "isotropic U tensor equivalent B");
        const auto badTensor =
            import(base + columns + "Si1 Si .1 .2 .3\n" + anis + "Si1 .01 -.01 .01 0 0 0\n");
        check(std::get<ImportIssue>(badTensor).core_reason == "invalid_anisotropic_tensor",
              "tensor PSD remains Core contract");
        auto anisB = anis;
        for (std::size_t offset = 0; (offset = anisB.find("_U_", offset)) != std::string::npos; ++offset)
            anisB[offset + 1] = 'B';
        // Orthogonal 5 A cell: beta_ij = B_ij/(4*5*5). Independent analytic
        // conversion checks distinguish B from both U and dimensionless beta.
        auto bTensor = accepted(base + columns + "Si1 Si .1 .2 .3\n" + anisB + "Si1 1 2 3 .1 .2 .3\n");
        const std::array<double, 6> expectedBeta{.01, .02, .03, .001, .002, .003};
        for (std::size_t i = 0; i < expectedBeta.size(); ++i)
            check(near(bTensor.sites[0].imported_beta[i], expectedBeta[i]), "CIF B tensor has B units");
        check(near(bTensor.sites[0].imported_b_iso, 2) && !bTensor.sites[0].default_card_adp,
              "CIF B equivalent is trace over three in orthogonal cell");
        const auto invalidB = import(base + columns + "Si1 Si .1 .2 .3\n" + anisB + "Si1 1 -2 3 .1 .2 .3\n");
        check(std::get<ImportIssue>(invalidB).core_reason == "invalid_anisotropic_tensor" &&
                  std::get<ImportIssue>(invalidB).field == "_atom_site_aniso_b",
              "invalid B reports its own field and preserves Core PSD error");
        const auto malformedB = import(base + columns + "Si1 Si .1 .2 .3\n" + anisB + "Si1 1 2 ? .1 .2 .3\n");
        check(std::get<ImportIssue>(malformedB).field == "_atom_site_aniso_b_33",
              "missing B component is not silently defaulted");
        auto anisBeta = anis;
        for (std::size_t offset = 0; (offset = anisBeta.find("_U_", offset)) != std::string::npos;
             offset += 6)
            anisBeta.replace(offset, 3, "_beta_");
        auto betaTensor =
            accepted(base + columns + "Si1 Si .1 .2 .3\n" + anisBeta + "Si1 .01 .02 .03 .001 .002 .003\n");
        for (std::size_t i = 0; i < expectedBeta.size(); ++i)
            check(betaTensor.sites[0].imported_beta[i] == expectedBeta[i],
                  "CIF beta tensor is dimensionless and not reinterpreted as B or U");
        check(near(betaTensor.sites[0].imported_b_iso, 2), "CIF beta equivalent B uses cell metric");
        const auto invalidBeta =
            import(base + columns + "Si1 Si .1 .2 .3\n" + anisBeta + "Si1 .01 -.02 .03 .001 .002 .003\n");
        check(std::get<ImportIssue>(invalidBeta).core_reason == "invalid_anisotropic_tensor" &&
                  std::get<ImportIssue>(invalidBeta).field == "_atom_site_aniso_beta",
              "invalid beta reports its units and Core PSD rejection");
        auto shiftedText = base;
        shiftedText.replace(shiftedText.find("'P 1'"), 5, "'P -1'");
        auto shifted = accepted(
            shiftedText + columns +
            "Si1 Si .123 .234 .345\nloop_\n_space_group_symop_operation_xyz\n'x,y,z'\n'-x+1/2,-y,-z'\n");
        check(shifted.metadata.origin_shift_twenty_fourths == std::array<int, 3>{6, 0, 0},
              "exact origin offset retained");
        check(near(shifted.structure.sites()[0].position.coordinates()[0], .873),
              "origin translation applied to position");
        auto joined = accepted(base + columns +
                               "Si1 Si .1 .2 .3\nO1 O .4 .5 "
                               ".6\nloop_\n_atom_site_label_1\n_atom_site_B_iso_or_equiv\nO1 .02\nSi1 .03\n");
        check(joined.sites[0].imported_b_iso == .03 && joined.sites[1].imported_b_iso == .02,
              "separate isotropic loop joined by label instead of row order");
        auto dummy =
            accepted(base + columns + "_atom_site_calc_flag\nSi1 Si .1 .2 .3 dum\nO1 O .4 .5 .6 d\n");
        check(dummy.structure.sites().size() == 1 && dummy.sites.size() == 1 &&
                  dummy.structure.sites()[0].species.label() == "O" && dummy.sites[0].label == "O1",
              "explicit dummy sites do not become physical atoms or shift provenance");
        check(dummy.notices.size() == 1 && dummy.notices[0].code == "nonphysical_dummy_site" &&
                  dummy.notices[0].field == "_atom_site_calc_flag" && dummy.notices[0].location.line > 1,
              "dummy exclusion is explicit and located");
        const auto onlyDummy = import(base + columns + "_atom_site_calc_flag\nSi1 Si .1 .2 .3 dum\n");
        check(std::get<ImportIssue>(onlyDummy).core_reason == "empty_structure",
              "an all-dummy input cannot publish an empty successful physical structure");
        auto unnamed = accepted(base + "_chemical_name_common zinc_oxide\n_chemical_name_systematic\n;\n ?\n;\n" +
                                columns + "Si1 Si .1 .2 .3\n");
        check(unnamed.metadata.chemical_name == "zinc_oxide",
              "semicolon unknown name uses the native common-name fallback");
        check(unnamed.metadata.reported.at("_chemical_name_systematic").kind == ValueKind::text &&
                  unnamed.metadata.reported.at("_chemical_name_systematic").text.find('?') != std::string::npos,
              "name presentation fallback preserves raw reported text and syntax kind");
        std::cout << "PASS " << checks << " checks\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
