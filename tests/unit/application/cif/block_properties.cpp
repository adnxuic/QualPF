#include "qualpf/application/database/cif_card_record.h"
#include "cif_parser.h"
#include <iostream>
#include <stdexcept>

namespace c = qualpf::io::cif;
using namespace qualpf::application;
namespace {
int checks = 0;
void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
c::Document document(const std::string &text) {
  auto r = c::parse(text, "source.cif");
  check(std::holds_alternative<c::Document>(r), "syntax");
  return std::get<c::Document>(r);
}
const c::ImportedCrystal &
success(const c::ImportResult<c::ImportedCrystal> &r) {
  if (const auto *e = std::get_if<c::ImportIssue>(&r))
    throw std::runtime_error(e->code + ":" + e->message);
  return std::get<c::ImportedCrystal>(r);
}
std::string block(std::string name,
                  std::string rows = "Si1 Si .1 .2 .3 1 .01\n",
                  std::string group = "P 1") {
  return "data_" + name + "\n_chemical_name_common '" + name +
         "'\n_cell_length_a 5\n_cell_length_b 6\n_cell_length_c 7\n"
         "_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma "
         "90\n_space_group_name_H-M_alt '" +
         group +
         "'\n"
         "loop_\n_atom_site_label\n_atom_site_type_symbol\n_atom_site_fract_"
         "x\n_atom_site_fract_y\n_atom_site_fract_z\n"
         "_atom_site_occupancy\n_atom_site_U_iso_or_equiv\n" +
         rows;
}
void rejected(const c::ImportResult<c::ImportedCrystal> &r,
              std::string_view code) {
  const auto *e = std::get_if<c::ImportIssue>(&r);
  check(e && e->code == code, "stable rejection");
}
} // namespace
int main() {
  try {
    check(
        c::sha256("") ==
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
        "SHA256 empty vector");
    check(
        c::sha256("abc") ==
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "SHA256 abc vector");
    check(
        c::sha256(std::string(1000000, 'a')) ==
            "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
        "SHA256 multiblock vector");
    const auto a = block("arbitrary"),
               b = block("published_cell", "O1 O .7 .8 .9 1 .02\n");
    auto d = document("data_sm_global\n_chemical_name_common CONTAMINATION\n" +
                      a + b + block("niggli", "? ? ? ? ? ? ?\n"));
    auto candidates = c::structure_candidates(d);
    check(candidates.size() == 4, "all blocks enumerated");
    check(candidates[0].classification ==
                  c::BlockClassification::metadata_only &&
              !candidates[0].importable,
          "metadata ownership");
    check(candidates[3].classification ==
                  c::BlockClassification::placeholder_structure &&
              !candidates[3].importable,
          "placeholder never zero");
    rejected(c::import_cif(d, {}), "multiple_structure_blocks");
    auto multiple = c::import_cif(d, {});
    check(std::get<c::ImportIssue>(multiple).candidates.size() == 4,
          "ambiguity carries full DTOs");
    auto selected = c::import_cif(
        d, {c::BlockSelectionPolicy::specified_block, "PUBLISHED_CELL"});
    const auto &s = success(selected);
    check(s.metadata.chemical_name == "published_cell" &&
              s.structure.sites()[0].species.label() == "O",
          "selected ownership");
    check(s.provenance.sites[0].displacement ==
              c::SourceDisplacement::isotropic_u,
          "strong source U type");
    check(s.provenance.normalization == std::vector<std::string>{"none"},
          "explicit no normalization");
    check(s.provenance.source_sha256 == c::sha256(d.original_bytes),
          "source bytes bound");
    rejected(
        c::import_cif(d, {c::BlockSelectionPolicy::specified_block, "niggli"}),
        "missing_coordinates");
    rejected(
        c::import_cif(d, {c::BlockSelectionPolicy::specified_block, "absent"}),
        "unknown_block");
    rejected(c::import_cif(d, {c::BlockSelectionPolicy::require_unique,
                               "published_cell"}),
             "invalid_block_selection");
    auto reversed = document(b + a);
    auto reversedSelection = c::import_cif(
        reversed, {c::BlockSelectionPolicy::specified_block, "published_cell"});
    check(success(reversedSelection).structure.sites()[0].species.label() ==
              "O",
          "ordering never changes explicit selection");
    auto unique = document(block("published", "? ? ? ? ? ? ?\n") + a);
    check(success(c::import_cif(unique, {})).metadata.block == "arbitrary",
          "require_unique ignores names, counts importability");
    auto missing = document(block("missing", "Si1 Si .1 .2 .3 1 ?\n"));
    auto m = c::import_cif(missing, {});
    check(success(m).provenance.sites[0].displacement ==
              c::SourceDisplacement::unknown,
          "missing ADP remains unknown");
    check(success(m).provenance.notices.size() == 1,
          "missing ADP warning distinct from card policy");
    auto ticket = c::save_selection(missing, success(m));
    check(std::holds_alternative<std::string>(ticket), "save selection");
    auto reload = c::reload_selection(std::get<std::string>(ticket));
    check(c::provenance_ledger(success(reload).provenance) ==
              c::provenance_ledger(success(m).provenance),
          "unknown and notices roundtrip");
    auto selectedTicket = c::save_selection(d, s);
    auto reloaded = c::reload_selection(std::get<std::string>(selectedTicket));
    check(success(reloaded).metadata.block == s.metadata.block &&
              success(reloaded).metadata.chemical_name ==
                  s.metadata.chemical_name,
          "multi-block selection reload");
    auto broken = std::get<std::string>(ticket);
    broken.back() = 'X';
    rejected(c::reload_selection(broken), "source_hash_mismatch");
    rejected(
        c::reload_selection("QUALPF_CIF_SELECTION_V1\n999999999999999999999:x"),
        "invalid_selection_ticket");
    auto stale = std::get<std::string>(ticket);
    auto pos = stale.find("phase19a_theoretical_card_adp");
    stale[pos] = 'X';
    rejected(c::reload_selection(stale), "selection_provenance_mismatch");
    const auto cardResult = CifCardService::from_imported(s);
    check(std::holds_alternative<CifCard>(cardResult), "card pipeline");
    auto dto = cif_card_record(std::get<CifCard>(cardResult), 1900001);
    const auto &record = std::get<qualpf::database::CifRecord>(dto);
    check(record.structure_identity == s.provenance.identity() &&
              record.provenance == c::provenance_ledger(s.provenance),
          "downstream DTO retains ledger");
    auto conflict = document(a + "_space_group_IT_number 2\n");
    rejected(c::import_cif(conflict, {}), "inconsistent_symmetry_definition");
    // Separate thermal loop is deliberately reverse ordered.
    std::string separate =
        block("joined", "O1 O .1 .2 .3 1 .01\nSi1 Si .2 .3 .4 1 .02\n");
    auto tag = separate.find("_atom_site_U_iso_or_equiv\n");
    separate.erase(tag, 25);
    auto row = separate.find("O1 O .1 .2 .3 1 .01");
    separate.replace(
        row, std::string("O1 O .1 .2 .3 1 .01\nSi1 Si .2 .3 .4 1 .02\n").size(),
        "O1 O .1 .2 .3 1\nSi1 Si .2 .3 .4 1\n");
    const auto thermal =
        "loop_\n_atom_site_label_1\n_atom_site_B_iso_or_equiv\n";
    auto joined = c::import_cif(
        document(separate + thermal + "Si1 .22(2)\nO1 .11(1)\n"), {});
    check(success(joined).sites[0].imported_b_iso == .11 &&
              success(joined).provenance.sites[0].isotropic_row == 2,
          "label join not row join");
    check(success(joined)
                  .provenance.sites[0]
                  .displacement_values.at("_atom_site_b_iso_or_equiv")
                  .raw == ".11(1)",
          "ADP SU preserved");
    rejected(
        c::import_cif(document(separate + thermal + "Si1 .22\nSi1 .11\n"), {}),
        "ambiguous_atom_loop_label");
    rejected(c::import_cif(
                 document(separate + thermal + "Si1 .22\nUnknown .11\n"), {}),
             "unmatched_atom_loop_label");
    rejected(c::import_cif(document(separate + thermal + "Si1 .22\n"), {}),
             "atom_loop_mismatch");
    rejected(
        c::import_cif(document(separate + thermal + "? .22\nO1 .11\n"), {}),
        "ambiguous_atom_loop_label");
    rejected(
        c::import_cif(
            document(block("duplicate",
                           "Si1 Si .1 .2 .3 1 .01\nSi1 Si .2 .3 .4 1 .02\n")),
            {}),
        "duplicate_atom_label");
    std::cout << "PASS " << checks << " checks\n";
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << " checks: " << e.what() << '\n';
    return 1;
  }
}
