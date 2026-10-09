#include "powder_cif_importer.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>
using namespace qualpf::io::powder_cif;
static_assert(!std::is_default_constructible_v<PowderCifPreparation>);
int main(int argc, char **argv) {
  if (argc != 2)
    return 2;
  PowderCifPreparation preparation = [](const char *filename) {
    std::ifstream file(filename, std::ios::binary);
    std::string raw((std::istreambuf_iterator<char>(file)), {});
    return std::get<PowderCifPreparation>(
        PowderCifImporter::prepare(std::move(raw), filename));
  }(argv[1]);
  // Input/local source buffers are destroyed. The preparation owns all source.
  if (preparation.selected().importability !=
      CandidateState::requires_wavelength)
    return 3;
  auto retained = preparation.source().artifact;
  auto block = preparation.selected().block_index;
  auto hash = retained->sha256;
  // Application Cancel drops a private attempt; it cannot produce a Pattern.
  {
    auto cancelled = preparation;
    if (cancelled.snapshot())
      return 4;
  }
  if (preparation.snapshot() || retained->sha256 != hash)
    return 5;
  for (auto invalid :
       {0u, 0x80000000u, 0xbf800000u, 0x7f800000u, 0x7fc00000u}) {
    auto result = PowderCifImporter::resolve(preparation, {invalid, {}});
    auto failure = std::get_if<ImportError>(&result);
    if (!failure || failure->error.reason != "invalid_wavelength" ||
        failure->block_index != block || failure->source != retained)
      return 6;
  }
  auto result = PowderCifImporter::resolve(
      preparation, {0x3f99999a, UserSuppliedRadiation::Kind::neutron});
  if (std::holds_alternative<ImportError>(result))
    return 7;
  auto snapshot = std::get<PowderCifImportSnapshot>(std::move(result));
  if (snapshot.source != retained ||
      snapshot.selected_block.block_index != block ||
      snapshot.radiation.origin != RadiationOrigin::user_supplied ||
      snapshot.radiation.kind != qualpf::RadiationKind::neutron ||
      preparation.snapshot() || preparation.source().artifact->sha256 != hash)
    return 8;
  auto imported = PowderCifImporter::to_pattern(std::move(snapshot), {77});
  if (std::holds_alternative<ImportError>(imported))
    return 9;
  auto out = std::get<ImportedPowderPattern>(std::move(imported));
  if (out.pattern.id.value != 77 || out.provenance.source != retained ||
      out.pattern.radiation.lines.size() != 1 ||
      out.pattern.radiation.lines[0].relative_weight != 1.0)
    return 10;
  std::cout
      << "PASS source ownership, Cancel/private discard, five invalid explicit "
         "wavelengths, same-block neutron caller, canonical ID/ratio\n";
  return 0;
}
