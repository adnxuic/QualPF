#include "qualpf/export/binary32_coordinates.h"
#include <array>
#include <cfenv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
template <class T> T read(std::istream &input) {
    T value{};
    input.read(reinterpret_cast<char *>(&value), sizeof value);
    if (!input)
        throw std::runtime_error("Invalid independent coordinate fixture");
    return value;
}
struct Fixture {
    double x, wavelength;
    std::array<std::uint32_t, 7> expected;
};
std::array<std::uint32_t, 7> stages(const qualpf::exporting::Binary32ExportCoordinate &v) {
    return {v.two_theta_bits, v.wavelength_bits,  v.radians_bits, v.half_angle_bits,
            v.sine_bits,      v.denominator_bits, v.d_bits};
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("Fixture and trace target required");
        std::ifstream input(argv[1], std::ios::binary);
        if (read<std::uint32_t>(input) != 0x24312027)
            throw std::runtime_error("Wrong coordinate fixture magic");
        const auto count = read<std::uint32_t>(input);
        if (!count || count > 1000000)
            throw std::runtime_error("Invalid coordinate count");
        std::vector<Fixture> fixtures;
        for (std::uint32_t i = 0; i < count; ++i) {
            Fixture f{read<double>(input), read<double>(input), {}};
            for (auto &bit : f.expected)
                bit = read<std::uint32_t>(input);
            fixtures.push_back(f);
        }
        std::ofstream output(argv[2], std::ios::binary);
        unsigned checks = 0, retries = 0;
        const auto original = std::fegetround();
        for (const auto mode : {FE_TONEAREST, FE_UPWARD, FE_DOWNWARD, FE_TOWARDZERO}) {
            if (std::fesetround(mode))
                throw std::runtime_error("Required rounding-mode control unavailable");
            for (const auto &f : fixtures) {
                const auto result = qualpf::exporting::evaluate_export_binary32_d(
                    qualpf::TwoThetaDegrees{f.x}, qualpf::WavelengthAngstrom{f.wavelength});
                if (!result || stages(result.value()) != f.expected)
                    throw std::runtime_error("Independent binary32 stage mismatch at " + std::to_string(checks));
                ++checks;
                retries += result.value().sine_precision_bits > 256;
                if (mode == FE_TONEAREST) {
                    const auto trace = stages(result.value());
                    output.write(reinterpret_cast<const char *>(trace.data()), sizeof trace);
                }
            }
        }
        std::fesetround(original);
        if (!output)
            throw std::runtime_error("Trace write failed");
        std::cout << "PASS " << fixtures.size() << " independent coordinate fixtures; " << checks
                  << " exact traces across four rounding modes; " << retries << " precision retries\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
