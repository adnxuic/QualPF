#include "cif_parser.h"
#include "infrastructure/io/pattern/file_reader.h"
#include "qualpf/io/pattern_reader.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
static void check(bool value) {
    if (!value)
        throw std::runtime_error("Memory/file IO contract");
}
int main(int argc, char **argv) try {
    check(argc == 2);
    const auto directory = std::filesystem::u8path(argv[1]);
    std::filesystem::create_directories(directory);
    const auto path = directory / std::filesystem::path(u8"样本.xy");
    const auto source = path.u8string();
    const std::string identity(source.begin(), source.end());
    const std::string bytes = "1 2\n2 4\n3 1\n";
    {
        std::ofstream file(path);
        file << bytes;
        check(bool(file));
    }
    std::istringstream input(bytes);
    auto memory = qualpf::io::read_pattern(input, ".xy", identity);
    auto file = qualpf::io::read_pattern(path);
    check(bool(memory) && bool(file));
    const auto &a = memory.value().pattern, &b = file.value().pattern;
    check(a.source_id == identity && a.source_id == b.source_id && a.two_theta == b.two_theta &&
          a.observed == b.observed && a.original_observed == b.original_observed &&
          a.radiation.lines.size() == b.radiation.lines.size() &&
          a.radiation.lines.front().wavelength == b.radiation.lines.front().wavelength &&
          memory.value().file_wavelength == file.value().file_wavelength &&
          memory.value().nominal_step == file.value().nominal_step);
    std::filesystem::remove(path);
    auto missing = qualpf::io::read_pattern(path);
    check(!missing && missing.error().code == qualpf::ErrorCode::missing_data &&
          missing.error().field == "pattern.file" && missing.error().reason == "pattern_read_failed" &&
          missing.error().message == "Cannot open pattern file");
    std::istringstream unsupported(bytes);
    auto bad = qualpf::io::read_pattern(unsupported, ".cif", "memory");
    check(!bad && bad.error().code == qualpf::ErrorCode::unsupported);
    auto cif = qualpf::io::cif::parse("data_memory\n_cell_length_a 2.1\n", "memory.cif");
    check(std::holds_alternative<qualpf::io::cif::Document>(cif));
    auto malformed = qualpf::io::cif::parse("data_memory\nloop_\n_a\n_b\n1\n", "memory.cif");
    check(std::holds_alternative<qualpf::io::cif::Error>(malformed));
    check(std::get<qualpf::io::cif::Error>(malformed).source == "memory.cif" &&
          std::get<qualpf::io::cif::Error>(malformed).location.line > 0);
    std::cout << "PASS: Qt-free memory Pattern/CIF parsing, UTF-8 file projection and stable IO errors\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
