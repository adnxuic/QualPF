#include "infrastructure/io/pattern/file_reader.h"
#include <iomanip>
#include <iostream>
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const auto result =
        qualpf::io::read_pattern(std::filesystem::path(reinterpret_cast<const char8_t *>(argv[1])));
    if (!result) {
        std::cout << "rejected\n" << result.error().field << '\n';
        return 0;
    }
    const auto &r = result.value();
    const auto &p = r.pattern;
    std::cout << std::setprecision(17) << "accepted\n"
              << p.two_theta.size() << ' ' << p.radiation.lines.size() << " 0\n"
              << p.calculation_range->minimum.value << ' ' << p.calculation_range->maximum.value << ' '
              << r.nominal_step << '\n'
              << "T F F T T 0\n"
              << p.source_id << '\n';
    for (auto w : p.radiation.lines)
        std::cout << w.wavelength.value << ' ' << w.relative_weight << '\n';
    for (std::size_t i = 0; i < p.observed.size(); ++i)
        std::cout << p.two_theta[i].value << ' ' << p.observed[i] << ' ' << (*p.original_observed)[i] << ' '
                  << (*p.corrected_two_theta)[i].value << '\n';
}
