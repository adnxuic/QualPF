#include "qualpf/crystallography/unit_cell.h"

#include <algorithm>
#include <sstream>
#include <cstdint>
#include <cfenv>
#include <bit>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numbers>
#include <random>
#include <string>
#include <vector>

using namespace qualpf;
using namespace qualpf::crystallography;

namespace {
struct Checks {
    std::map<std::string, int> passed;
    std::vector<std::string> failures;
    void test(const char* group, bool ok) {
        if (ok)
            ++passed[group];
        else
            failures.emplace_back(group);
    }
    void close(const char* group, double a, double b, double rel = 2e-12, double abs = 0) {
        test(group, std::isfinite(a) && std::isfinite(b) &&
                        std::abs(a - b) <= abs + rel * std::max(std::abs(a), std::abs(b)));
    }
};
void invariants(Checks& c, UnitCellParameters p, Hkl h, double tolerance = 2e-12) {
    const auto result = UnitCell::create(p);
    c.test("valid_cell", bool(result));
    if (!result)
        return;
    const auto& cell = result.value();
    const auto& g = cell.metric().angstrom_squared;
    const auto& r = cell.reciprocal_metric().inverse_angstrom_squared;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            c.test("metric_symmetry", g[3 * i + j] == g[3 * j + i] && r[3 * i + j] == r[3 * j + i]);
            // Dimensionless inverse identity avoids confusing length anisotropy
            // with angular conditioning. Products still use the exported tensors.
            long double value = 0;
            for (int k = 0; k < 3; ++k)
                value += (static_cast<long double>(g[3 * i + k]) / p.lengths_angstrom[i] /
                          p.lengths_angstrom[k]) *
                         (static_cast<long double>(r[3 * k + j]) * p.lengths_angstrom[k] *
                          p.lengths_angstrom[j]);
            c.close("inverse_metric", double(value), i == j ? 1.0 : 0.0, tolerance, tolerance);
        }
    }
    c.close("inverse_volume",
            cell.volume_angstrom_cubed() * cell.reciprocal_volume_inverse_angstrom_cubed(), 1.0);
    const auto reciprocal = cell.reciprocal();
    c.test("reciprocal_conversion", bool(reciprocal));
    if (reciprocal) {
        const auto back = UnitCell::from_reciprocal(reciprocal.value());
        c.test("reciprocal_roundtrip", bool(back));
        if (back)
            for (int i = 0; i < 3; ++i) {
                c.close("roundtrip_lengths", back.value().parameters().lengths_angstrom[i],
                        p.lengths_angstrom[i], tolerance);
                c.close("roundtrip_angles", back.value().parameters().angles_degrees[i],
                        p.angles_degrees[i], tolerance, tolerance);
            }
    }
    const auto d = cell.d_spacing(h), negative = cell.d_spacing({-h.h, -h.k, -h.l});
    c.test("positive_finite_d", d && d.value().value > 0 && std::isfinite(d.value().value));
    c.test("friedel_geometry", d && negative && d.value() == negative.value());
    if (d) {
        const WavelengthAngstrom wave{d.value().value * 0.73};
        const auto angle = two_theta(d.value(), wave);
        c.test("bragg_forward", bool(angle));
        if (angle) {
            const auto back_d = d_spacing(angle.value(), wave);
            c.test("bragg_backward", bool(back_d));
            if (back_d)
                c.close("bragg_roundtrip", back_d.value().value, d.value().value);
            const auto theta = bragg_angle(d.value(), wave);
            c.test("theta_is_half_two_theta",
                   theta && theta.value().value * 2 == angle.value().value);
        }
    }
}

void exact_formed_angle_cells(Checks& c, const char* path) {
    std::ifstream input(path);
    c.test("formed_angle_reference_open", bool(input));
    std::string line;
    int rows = 0;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream fields(line);
        std::array<std::uint64_t, 25> values{};
        bool complete = true;
        for (auto& value : values)
            complete = complete && bool(fields >> std::hex >> value);
        std::string extra;
        c.test("formed_angle_reference_row", complete && !(fields >> extra));
        if (!complete)
            continue;
        UnitCellParameters p{};
        for (int i = 0; i < 3; ++i) {
            p.lengths_angstrom[i] = std::bit_cast<double>(values[i]);
            p.angles_degrees[i] = std::bit_cast<double>(values[3 + i]);
        }
        const auto result = UnitCell::create(p);
        c.test("formed_angle_exact_construct", bool(result));
        if (result) {
            const auto& cell = result.value();
            for (int i = 0; i < 9; ++i) {
                c.test("formed_angle_exact_metric",
                       std::bit_cast<std::uint64_t>(cell.metric().angstrom_squared[i]) == values[6 + i]);
                c.test("formed_angle_exact_reciprocal",
                       std::bit_cast<std::uint64_t>(cell.reciprocal_metric().inverse_angstrom_squared[i]) == values[15 + i]);
            }
            c.test("formed_angle_exact_volume",
                   std::bit_cast<std::uint64_t>(cell.volume_angstrom_cubed()) == values[24]);
        }
        ++rows;
    }
    c.test("formed_angle_reference_count", rows == 16);
}

void formed_angle_environment(Checks& c) {
    const int original = std::fegetround();
    for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
        c.test("formed_angle_rounding_setup", std::fesetround(mode) == 0);
        for (auto angles : {std::array{90., 90., 90.}, std::array{90., 108., 90.},
                            std::array{80., 90., 100.}}) {
            const auto result = UnitCell::create({{5, 6, 7}, angles});
            c.test("formed_angle_environment_valid", result &&
                   std::isfinite(result.value().volume_angstrom_cubed()) &&
                   result.value().volume_angstrom_cubed() > 0);
            c.test("formed_angle_environment_preserved", std::fegetround() == mode);
        }
        for (auto angles : {std::array{120., 120., 120.}, std::array{1., 1., 179.},
                            std::array{60., 60., 120.},
                            std::array{90., 90., std::nextafter(180., 0.)},
                            std::array{90., 90., std::numeric_limits<double>::denorm_min()}}) {
            const auto result = UnitCell::create({{5, 6, 7}, angles});
            c.test("formed_angle_bounded_error", !result &&
                   result.error().field == "cell.angles" &&
                   result.error().reason == "degenerate_cell");
            c.test("formed_angle_environment_preserved", std::fegetround() == mode);
        }
    }
    c.test("formed_angle_rounding_restore", std::fesetround(original) == 0);
}

} // namespace

int main(int argc, char** argv) {
    Checks c;
    const int entry_rounding = std::fegetround();
    c.test("formed_angle_exact_rounding", std::fesetround(FE_TONEAREST) == 0);
    if (argc > 2)
        exact_formed_angle_cells(c, argv[2]);
    else
        c.test("formed_angle_reference_argument", false);
    formed_angle_environment(c);
    c.test("formed_angle_test_rounding", std::fesetround(FE_TONEAREST) == 0);
    // Independent analytic solutions, including high indices without integer squaring.
    for (double a : {1e-90, 0.01, 5.0, 1e90}) {
        const auto cell = UnitCell::create({{a, a, a}, {90, 90, 90}});
        c.test("cubic_construct", bool(cell));
        if (!cell)
            continue;
        c.close("cubic_volume", cell.value().volume_angstrom_cubed(), a * a * a);
        for (Hkl h : {Hkl{1, 0, 0}, Hkl{0, -1, 2}, Hkl{1001, -997, 991},
                      Hkl{2147483647, 0, -2147483647}, Hkl{-2147483647 - 1, 1, 0}}) {
            const auto d = cell.value().d_spacing(h);
            c.test("cubic_d_result", bool(d));
            if (d)
                c.close("cubic_analytic_d", d.value().value,
                        a / std::hypot(double(h.h), double(h.k), double(h.l)));
        }
    }
    for (const auto lengths : {std::array{3.0, 7.0, 11.0}, std::array{.01, 10.0, 10000.0},
                               std::array{1e150, 1e-150, 1.0}}) {
        const auto cell = UnitCell::create({lengths, {90, 90, 90}});
        c.test("orthogonal_construct", bool(cell));
        if (!cell)
            continue;
        c.close("orthogonal_volume", cell.value().volume_angstrom_cubed(),
                lengths[0] * lengths[1] * lengths[2]);
        const auto d = cell.value().d_spacing({2, -3, 5});
        c.test("orthogonal_d_result", bool(d));
        if (d)
            c.close("orthogonal_analytic_d", d.value().value,
                    1 / std::hypot(2 / lengths[0], 3 / lengths[1], 5 / lengths[2]));
        invariants(c, {lengths, {90, 90, 90}}, {2, -3, 5});
    }
    const auto hex = UnitCell::create({{4, 4, 7}, {90, 90, 120}});
    c.test("hexagonal_construct", bool(hex));
    if (hex)
        c.close("hexagonal_volume", hex.value().volume_angstrom_cubed(),
                16 * 7 * std::sqrt(3.0) / 2);
    const auto mono = UnitCell::create({{4, 6, 9}, {90, 108, 90}});
    c.test("monoclinic_construct", bool(mono));
    if (mono)
        c.close("monoclinic_volume", mono.value().volume_angstrom_cubed(),
                4 * 6 * 9 * std::sin(108 * std::numbers::pi / 180));
    for (const auto angles :
         {std::array{90., 90., 1.}, std::array{90., 90., 179.}, std::array{60., 60., 119.}})
        invariants(c, {{5, 6, 7}, angles}, {2, -3, 5}, 2e-10);

    // Seed and mapping fixed across standard libraries; random() distributions
    // are deliberately avoided because their implementations differ.
    std::mt19937 engine(130013);
    auto uniform = [&]() { return double(engine()) / 4294967296.0; };
    for (int n = 0; n < 512; ++n) {
        UnitCellParameters p{};
        for (int i = 0; i < 3; ++i) {
            p.lengths_angstrom[i] = std::pow(10.0, uniform() * 12 - 6);
            p.angles_degrees[i] = 70 + 40 * uniform();
        }
        Hkl h{int(engine() % 2001) - 1000, int(engine() % 2001) - 1000,
              int(engine() % 2001) - 1000};
        if (h.h == 0 && h.k == 0 && h.l == 0)
            h.h = 1;
        invariants(c, p, h);
        // Independent scalar determinant expansion, well-conditioned here.
        const auto cell = UnitCell::create(p);
        const double ca = std::cos(p.angles_degrees[0] * std::numbers::pi / 180);
        const double cb = std::cos(p.angles_degrees[1] * std::numbers::pi / 180);
        const double cg = std::cos(p.angles_degrees[2] * std::numbers::pi / 180);
        if (cell)
            c.close("triclinic_volume", cell.value().volume_angstrom_cubed(),
                    p.lengths_angstrom[0] * p.lengths_angstrom[1] * p.lengths_angstrom[2] *
                        std::sqrt(1 + 2 * ca * cb * cg - ca * ca - cb * cb - cg * cg));
    }
    const double nan = std::numeric_limits<double>::quiet_NaN(),
                 inf = std::numeric_limits<double>::infinity();
    for (int axis = 0; axis < 3; ++axis) {
        for (double x : {0., -1., nan, inf, -inf}) {
            UnitCellParameters p{{5, 5, 5}, {90, 90, 90}};
            p.lengths_angstrom[axis] = x;
            auto r = UnitCell::create(p);
            c.test("invalid_lengths", !r && r.error().reason == "invalid_cell_length");
        }
        for (double x : {0., -1., 180., 181., nan, inf, -inf}) {
            UnitCellParameters p{{5, 5, 5}, {90, 90, 90}};
            p.angles_degrees[axis] = x;
            auto r = UnitCell::create(p);
            c.test("invalid_angles", !r && r.error().reason == "invalid_cell_angle");
        }
    }
    for (const auto a : {std::array{60., 60., 120.}, std::array{30., 30., 100.},
                         std::array{120., 120., 120.}, std::array{90., 90., 1e-6}}) {
        auto r = UnitCell::create({{5, 5, 5}, a});
        c.test("singular_geometry", !r && r.error().reason == "degenerate_cell");
    }
    for (double a : {1e150, 1e-150, 1e200, 1e-200}) {
        auto r = UnitCell::create({{a, a, a}, {90, 90, 90}});
        c.test("cell_numeric_range", !r && r.error().reason == "cell_range");
    }
    auto valid = UnitCell::create({{5, 5, 5}, {90, 90, 90}});
    const auto zero = valid.value().d_spacing({0, 0, 0});
    c.test("zero_hkl", !zero && zero.error().reason == "zero_hkl");
    for (double x : {0., -1., nan, inf, -inf}) {
        auto t = two_theta(DSpacingAngstrom{5}, WavelengthAngstrom{x});
        auto d = d_spacing(TwoThetaDegrees{60}, WavelengthAngstrom{x});
        c.test("invalid_wavelength", !t && !d && t.error().reason == "invalid_wavelength" &&
                                         d.error().reason == "invalid_wavelength");
        auto bad_d = two_theta(DSpacingAngstrom{x}, WavelengthAngstrom{1});
        c.test("invalid_spacing", !bad_d && bad_d.error().reason == "invalid_d_spacing");
    }
    for (double x : {0., -1., 180.1, nan, inf, -inf}) {
        auto d = d_spacing(TwoThetaDegrees{x}, WavelengthAngstrom{1});
        c.test("invalid_two_theta", !d && d.error().reason == "invalid_two_theta");
    }
    const auto overflow =
        two_theta(DSpacingAngstrom{1}, WavelengthAngstrom{std::nextafter(2., 3.)});
    c.test("no_asin_clamping", !overflow && overflow.error().reason == "bragg_domain");
    const auto endpoint = two_theta(DSpacingAngstrom{5}, WavelengthAngstrom{10});
    c.test("backscatter_endpoint", endpoint && endpoint.value().value == 180);
    const double tiny = std::numeric_limits<double>::denorm_min();
    const auto subnormal = two_theta(DSpacingAngstrom{tiny * 2}, WavelengthAngstrom{tiny * 3});
    c.test("subnormal_bragg", bool(subnormal));
    if (subnormal)
        c.close("subnormal_ratio", subnormal.value().value,
                2 * std::asin(.75) * 180 / std::numbers::pi);
    const auto huge = two_theta(DSpacingAngstrom{1e308}, WavelengthAngstrom{1e308});
    c.test("huge_bragg", huge && std::abs(huge.value().value - 60) < 1e-12);
    const auto tiny_angle = d_spacing(TwoThetaDegrees{tiny}, WavelengthAngstrom{1});
    c.test("unrepresentable_bragg", !tiny_angle && tiny_angle.error().reason == "bragg_range");
    const auto tiny_ratio = two_theta(DSpacingAngstrom{1e308}, WavelengthAngstrom{tiny});
    c.test("unrepresentable_ratio", !tiny_ratio && tiny_ratio.error().reason == "bragg_range");

    c.test("formed_angle_entry_rounding_restore", std::fesetround(entry_rounding) == 0);
    std::ofstream file;
    if (argc > 1)
        file.open(argv[1]);
    std::ostream& out = file.is_open() ? file : std::cout;
    out << "{\"seed\":130013,\"random_cells\":512,\"passed_by_property\":{";
    bool comma = false;
    for (const auto& [name, count] : c.passed) {
        if (comma)
            out << ',';
        comma = true;
        out << '"' << name << "\":" << count;
    }
    out << "},\"failures\":[";
    comma = false;
    for (const auto& name : c.failures) {
        if (comma)
            out << ',';
        comma = true;
        out << '"' << name << '"';
    }
    out << "]}\n";
    for (const auto& name : c.failures)
        std::cerr << name << '\n';
    return c.failures.empty() ? 0 : 1;
}
