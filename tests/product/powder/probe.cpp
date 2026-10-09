#include "binary32.h"
#include "powder_cif_importer.h"
#include "sha256.h"
#include <bit>
#include <cfenv>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

using namespace qualpf::io::powder_cif;
std::string hex(std::string_view text) {
  constexpr char digit[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : text) {
    out += digit[c >> 4];
    out += digit[c & 15];
  }
  return out.empty() ? "-" : out;
}
void field(const LocatedField &value) {
  std::cout << "LOC " << hex(value.tag) << ' ' << value.location.offset << ' '
            << value.location.line << ' ' << value.location.column << ' '
            << value.span.begin << ' ' << value.span.end << ' ';
  if (value.row)
    std::cout << *value.row;
  else
    std::cout << '-';
  std::cout << '\n';
}
void failure(const ImportError &error) {
  std::cout << "ERROR " << error.error.reason << ' ' << error.error.field << ' '
            << hex(error.block_id) << ' ';
  if (error.block_index)
    std::cout << *error.block_index;
  else
    std::cout << '-';
  std::cout << ' ' << error.source->sha256 << ' ';
  if (error.axis_count)
    std::cout << *error.axis_count;
  else
    std::cout << '-';
  std::cout << ' ';
  if (error.intensity_count)
    std::cout << *error.intensity_count;
  else
    std::cout << '-';
  std::cout << '\n';
  for (const auto &location : error.locations)
    field(location);
}
void values(std::string_view key, const auto &data, auto getter,
            int width = 8) {
  std::cout << key;
  for (const auto &value : data)
    std::cout << ' ' << std::hex << std::setw(width) << std::setfill('0')
              << getter(value);
  std::cout << std::dec << '\n';
}
void emit(const PowderCifImportSnapshot &snapshot) {
  std::cout << "SNAPSHOT " << snapshot.reversed << ' ';
  if (snapshot.radiation.origin)
    std::cout << int(*snapshot.radiation.origin);
  else
    std::cout << '-';
  std::cout << ' ';
  if (snapshot.radiation.wavelength_bits)
    std::cout << std::hex << *snapshot.radiation.wavelength_bits << std::dec;
  else
    std::cout << '-';
  std::cout << '\n';
  auto same = [](auto n) { return n; };
  values("X32", snapshot.x, same);
  values("RAW32", snapshot.raw_y, same);
  values("Y32", snapshot.scaled_y, same);
  auto imported = PowderCifImporter::to_pattern(snapshot, {1});
  if (auto error = std::get_if<ImportError>(&imported)) {
    failure(*error);
    return;
  }
  const auto &pattern = std::get<ImportedPowderPattern>(imported).pattern;
  std::cout << "SOURCEID " << hex(pattern.source_id) << '\n';
  std::cout << "RADIATIONKIND ";
  if (pattern.radiation.kind)
    std::cout << int(*pattern.radiation.kind);
  else
    std::cout << '-';
  std::cout << '\n';
  std::cout << "PATTERN " << pattern.id.value << ' '
            << int(pattern.intensity_scale) << ' '
            << int(pattern.source_precision) << ' '
            << pattern.processing.background_subtracted.value() << ' '
            << pattern.processing.kalpha2_subtracted.value() << ' '
            << pattern.processing.intensity_scaled.value() << ' '
            << pattern.original_kalpha2_subtracted.value() << ' '
            << bool(pattern.background) << ' ' << bool(pattern.calculated)
            << ' ' << bool(pattern.background_subtracted_signal) << ' '
            << bool(pattern.weights) << '\n';
  values(
      "X64", pattern.two_theta,
      [](auto v) { return std::bit_cast<std::uint64_t>(v.value); }, 16);
  values(
      "X064", *pattern.corrected_two_theta,
      [](auto v) { return std::bit_cast<std::uint64_t>(v.value); }, 16);
  values(
      "RAW64", *pattern.original_observed,
      [](auto v) { return std::bit_cast<std::uint64_t>(v); }, 16);
  values(
      "Y64", pattern.observed,
      [](auto v) { return std::bit_cast<std::uint64_t>(v); }, 16);
  if (pattern.calculation_range)
    std::cout << "RANGE " << std::hex
              << std::bit_cast<std::uint64_t>(
                     pattern.calculation_range->minimum.value)
              << ' '
              << std::bit_cast<std::uint64_t>(
                     pattern.calculation_range->maximum.value)
              << std::dec << '\n';
  for (auto line : pattern.radiation.lines)
    std::cout << "WAVE " << std::hex
              << std::bit_cast<std::uint64_t>(line.wavelength.value) << ' '
              << std::bit_cast<std::uint64_t>(line.relative_weight) << std::dec
              << '\n';
  for (const auto &notice : snapshot.notices)
    std::cout << "NOTICE " << notice.reason << ' ' << hex(notice.tag) << ' '
              << notice.block_index << ' ' << notice.span.begin << ' '
              << notice.span.end << '\n';
}
int main(int argc, char **argv) {
  if (argc == 2 && std::string_view(argv[1]) == "hash") {
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    std::string input((std::istreambuf_iterator<char>(std::cin)), {});
    std::cout << source_sha256(input) << '\n';
    return 0;
  }
  if (argc < 2)
    return 2;
  if (argc > 3) {
    const std::string mode = argv[3];
    std::fesetround(mode == "down"   ? FE_DOWNWARD
                    : mode == "up"   ? FE_UPWARD
                    : mode == "zero" ? FE_TOWARDZERO
                                     : FE_TONEAREST);
  }
  std::ifstream stream(argv[1], std::ios::binary);
  if (!stream)
    return 2;
  std::string raw((std::istreambuf_iterator<char>(stream)), {});
  auto result = PowderCifImporter::prepare(raw, argv[1]);
  if (auto error = std::get_if<ImportError>(&result)) {
    failure(*error);
    return 0;
  }
  const auto &preparation = std::get<PowderCifPreparation>(result);
  std::cout << "SOURCE " << preparation.source().artifact->sha256 << ' '
            << raw.size() << ' '
            << preparation.source().source_fragment_without_data_block << '\n';
  const auto &doc = preparation.source().document;
  // Every source token is checked against original bytes by the driver.
  for (std::size_t b = 0; b < doc.blocks.size(); ++b) {
    std::cout << "BLOCK " << b << ' ' << hex(doc.blocks[b].name) << ' '
              << doc.blocks[b].location.offset << '\n';
    auto value = [&](const auto &tag, const auto &v, auto row) {
      std::cout << "TOKEN " << b << ' ' << hex(tag) << ' ' << v.location.offset
                << ' ' << v.location.line << ' ' << v.location.column << ' '
                << row << ' ' << int(v.kind) << ' ' << hex(v.raw) << ' '
                << hex(v.text) << '\n';
    };
    for (const auto &item : doc.blocks[b].items)
      value(item.tag.name, item.value, 0);
    for (const auto &loop : doc.blocks[b].loops)
      for (std::size_t c = 0; c < loop.tags.size(); ++c)
        for (std::size_t r = 0; r < loop.rows(); ++r)
          value(loop.tags[c].name, loop.values[r * loop.tags.size() + c], r);
  }
  for (const auto &block : preparation.candidates()) {
    std::cout << "CANDIDATE " << block.block_index << ' ' << hex(block.block_id)
              << ' ' << block.source_span.begin << ' ' << block.source_span.end
              << ' ' << int(block.importability) << ' ' << block.row_count
              << ' '
              << hex(block.selected_axis    ? block.selected_axis->tag
                     : block.generated_axis ? "generated"
                                            : "")
              << ' '
              << hex(block.selected_intensity ? block.selected_intensity->tag
                                              : "")
              << ' ' << int(block.wavelength_state) << '\n';
    for (const auto *group : {&block.available_axis_candidates,
                              &block.available_intensity_candidates})
      for (const auto &c : *group) {
        std::cout << "COLUMN " << block.block_index << ' ' << hex(c.tag) << ' '
                  << c.status << ' ' << c.values.size() << '\n';
        values("DECODE32", c.bits, [](auto n) { return n; });
      }
  }
  std::cout << "SELECTED " << preparation.selected().block_index << '\n';
  if (argc > 2 && std::string_view(argv[2]) != "none") {
    auto resolution = PowderCifImporter::resolve(
        preparation,
        {static_cast<std::uint32_t>(std::stoul(argv[2], nullptr, 16)),
         UserSuppliedRadiation::Kind::x_ray});
    if (auto error = std::get_if<ImportError>(&resolution)) {
      failure(*error);
      return 0;
    }
    emit(std::get<PowderCifImportSnapshot>(resolution));
  } else if (preparation.snapshot())
    emit(*preparation.snapshot());
  else
    std::cout << "REQUIRES requires_wavelength\n";
  return 0;
}
