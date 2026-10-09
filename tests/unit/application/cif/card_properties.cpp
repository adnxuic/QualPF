#include "qualpf/application/database/cif_card_record.h"
#include "qualpf/application/database/cif_card_service.h"
#include "qualpf/domain/radiation.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace qualpf::application;
namespace {
int checks = 0;
void check(bool ok, const char *message) {
  ++checks;
  if (!ok)
    throw std::runtime_error(message);
}
std::string input(std::string length = "5", std::string occupancy = "1") {
  return "data_copper\n_cell_length_a " + length + "\n_cell_length_b " +
         length + "\n_cell_length_c " + length +
         "\n_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma 90\n"
         "_symmetry_space_group_name_H-M 'P 1'\n_chemical_name_common 'Test "
         "copper'\n"
         "loop_\n_atom_site_label\n_atom_site_type_symbol\n_atom_site_fract_x\n"
         "_atom_site_fract_y\n_atom_site_fract_z\n_atom_site_occupancy\n"
         "Cu1 Cu .123 .234 .345 " +
         occupancy + "\n";
}
const CifCard &complete(const CifCardResult &result) {
  if (const auto *error = std::get_if<CifCardIssue>(&result))
    throw std::runtime_error(error->code + ": " + error->field + ": " +
                             error->message);
  return std::get<CifCard>(result);
}
const CifCardIssue &rejected(const CifCardResult &result, CifCardStage stage) {
  const auto *error = std::get_if<CifCardIssue>(&result);
  check(error && error->stage == stage,
        "failure reports originating stage without partial card");
  check(error->source == "bad.cif", "failure source retained");
  check(!error->code.empty() && !error->message.empty(),
        "failure has stable code and explanation");
  return *error;
}
} // namespace
int main() {
  try {
    // Input temporaries die here: result must own its provenance and physics.
    const auto first =
        CifCardService::from_bytes(input(), std::string("first.cif"));
    const auto &result = complete(first);
    const auto &card = result.card();
    check(result.imported().metadata.source == "first.cif" &&
              card.metadata().provenance == "first.cif",
          "complete result owns matching provenance");
    check(card.metadata().identifier ==
                  result.imported().provenance.identity() &&
              card.metadata().chemical_name == "Test copper",
          "selected block metadata reaches card");
    check(card.radiation().wavelength.value ==
                  qualpf::radiation::default_wavelength.value &&
              card.friedel_policy() ==
                  qualpf::crystallography::FriedelPolicy::merge,
          "canonical product radiation and Friedel policy");
    check(card.group_setting_id() ==
              result.imported().structure.group().setting().id,
          "card retains imported setting");
    check(std::abs(card.density().volume_angstrom_cubed - 125) < 1e-12 &&
              card.density().grams_per_cm3 > 0 && card.rir().value > 0,
          "complete positive physical card");
    check(card.composition().formula().display == "Cu", "computed composition");
    std::size_t members = 0;
    for (const auto &line : card.physical_lines().lines()) {
      members += line.members.size();
      check(line.representative.two_theta.value >= 1 &&
                line.representative.two_theta.value <= 150,
            "physical range remains product range");
    }
    check(result.reflection_count() == members,
          "reflection count is complete before card truncation");
    check(!card.presentation().lines.empty() &&
              card.presentation().lines.size() <= 500,
          "successful card has bounded presentation");

    const auto syntax =
        CifCardService::from_bytes("data_bad\n_tag 'unclosed", "bad.cif");
    const auto &syntax_error = rejected(syntax, CifCardStage::syntax);
    check(syntax_error.code == "unterminated_quote" &&
              syntax_error.location.line == 2,
          "syntax details preserved");
    const auto semantic =
        CifCardService::from_bytes(input("5", "-1"), "bad.cif");
    const auto &semantic_error = rejected(semantic, CifCardStage::import);
    check(semantic_error.block == "copper" &&
              semantic_error.field == "_atom_site_occupancy" &&
              semantic_error.core_reason == "invalid_occupancy",
          "semantic field and core reason preserved");
    const auto huge = CifCardService::from_bytes(input("1000"), "bad.cif");
    const auto &huge_error = rejected(huge, CifCardStage::reflections);
    check(huge_error.block == "copper" && !huge_error.core_field.empty() &&
              !huge_error.core_reason.empty() &&
              huge_error.core_code.has_value(),
          "calculation rejection preserves block and complete Core diagnostic");
    const auto dark = CifCardService::from_bytes(input("5", "0"), "bad.cif");
    const auto &dark_error = rejected(dark, CifCardStage::powder);
    check(!dark_error.core_reason.empty(),
          "zero scattering yields error, not empty successful card");
    qualpf::io::cif::Limits tiny;
    tiny.bytes = 5;
    const auto limited = CifCardService::from_bytes(input(), "bad.cif", tiny);
    check(rejected(limited, CifCardStage::syntax).code == "resource_limit",
          "syntax resource limit");

    const auto after = CifCardService::from_bytes(input(), "after.cif");
    const auto &second = complete(after).card();
    check(
        second.metadata().provenance == "after.cif" &&
            card.metadata().provenance == "first.cif",
        "success and failures cannot leak source metadata into other results");
    check(second.rir().value == card.rir().value &&
              second.presentation().lines.size() ==
                  card.presentation().lines.size(),
          "failure sequence leaves later calculation unchanged");
    for (std::size_t i = 0; i < card.presentation().lines.size(); ++i) {
      const auto &a = card.presentation().lines[i];
      const auto &b = second.presentation().lines[i];
      check(a.angular_slot == b.angular_slot &&
                a.card_relative_intensity == b.card_relative_intensity,
            "independent calculations retain exact card order and intensity");
    }
    const auto with_zero =
        CifCardService::from_bytes(input() + "C2 C .4 .5 .6 0\n", "zero.cif");
    const auto &zero_card = complete(with_zero);
    auto adapted = cif_card_record(zero_card, 1900199);
    const auto &record = std::get<qualpf::database::CifRecord>(adapted);
    check(record.identity == 1900199 && record.source == "zero.cif" &&
              record.block == "copper",
          "record owns explicit identity and source context");
    check(record.atom_count == 1 &&
              zero_card.imported().structure.sites().size() == 2,
          "zero occupancy preserved in provenance and omitted from product "
          "atom count");
    check(record.elements == std::vector<std::string>{"Cu"} &&
              record.subfile == "inorganic",
          "zero occupancy carbon cannot change product chemistry or "
          "classification");
    check(record.rir == zero_card.card().rir().value &&
              record.density == zero_card.card().density().grams_per_cm3,
          "record keeps full validated scalar precision");
    check(record.lines.size() == zero_card.card().presentation().lines.size(),
          "adapter does not reselect or truncate lines");
    for (std::size_t i = 0; i < record.lines.size(); ++i) {
      const auto &selected = zero_card.card().presentation().lines[i];
      check(record.lines[i].d ==
                    selected.physical_line.representative.d.value &&
                record.lines[i].relative_intensity ==
                    selected.card_relative_intensity,
            "adapter preserves each selected line and its order without "
            "binary32 rounding");
    }
    std::cout << checks << " CIF card service checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
