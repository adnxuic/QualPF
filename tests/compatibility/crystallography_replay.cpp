#include "qualpf/crystallography/unit_cell.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string>

using namespace qualpf;
using namespace qualpf::crystallography;

int main() {
    UnitCellParameters p{};
    std::string token;
    // stod deliberately accepts the captured historical NaN/Infinity inputs.
    for (auto& x : p.lengths_angstrom) {
        std::cin >> token;
        x = std::stod(token);
    }
    for (auto& x : p.angles_degrees) {
        std::cin >> token;
        x = std::stod(token);
    }
    Hkl h{};
    std::cin >> h.h >> h.k >> h.l >> token;
    const WavelengthAngstrom wave{std::stod(token)};
    auto error = [](const Error& e) {
        std::cout << "{\"error\":\"" << e.reason << "\"}\n";
        return 0;
    };
    const auto cell = UnitCell::create(p);
    if (!cell)
        return error(cell.error());
    const auto norm = cell.value().reciprocal_vector_norm(h);
    if (!norm)
        return error(norm.error());
    const auto d = cell.value().d_spacing(h);
    if (!d)
        return error(d.error());
    const auto t = two_theta(d.value(), wave);
    if (!t)
        return error(t.error());
    const auto r = cell.value().reciprocal();
    if (!r)
        return error(r.error());
    const auto back = UnitCell::from_reciprocal(r.value());
    if (!back)
        return error(back.error());
    const auto db = d_spacing(t.value(), wave);
    if (!db)
        return error(db.error());
    std::cout << std::setprecision(17) << "{\"error\":null,\"fields\":{";
    bool first = true;
    auto field = [&](const char* name, const auto& values) {
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << '"' << name << "\":[";
        bool comma = false;
        for (double value : values) {
            if (comma)
                std::cout << ',';
            comma = true;
            std::cout << value;
        }
        std::cout << ']';
    };
    auto scalar = [&](const char* name, double value) { field(name, std::array{value}); };
    field("metric", cell.value().metric().angstrom_squared);
    field("reciprocal_metric", cell.value().reciprocal_metric().inverse_angstrom_squared);
    auto parameters = [&](const char* name, const auto& lengths, const auto& angles) {
        field(name, std::array{lengths[0], lengths[1], lengths[2],
                               std::cos(angles[0] * std::numbers::pi / 180.0),
                               std::cos(angles[1] * std::numbers::pi / 180.0),
                               std::cos(angles[2] * std::numbers::pi / 180.0)});
    };
    parameters("reciprocal", r.value().lengths_inverse_angstrom, r.value().angles_degrees);
    parameters("back", back.value().parameters().lengths_angstrom,
               back.value().parameters().angles_degrees);
    scalar("volume", cell.value().volume_angstrom_cubed());
    scalar("reciprocal_volume", cell.value().reciprocal_volume_inverse_angstrom_cubed());
    scalar("norm", norm.value().inverse_angstrom);
    scalar("d", d.value().value);
    scalar("reflection_two_theta", t.value().value);
    scalar("counts_two_theta", t.value().value);
    scalar("counts_d", db.value().value);
    std::cout << "}}\n";
}
