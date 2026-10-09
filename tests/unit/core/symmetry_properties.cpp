#include "qualpf/crystallography/symmetry.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>
using namespace qualpf;
using namespace qualpf::crystallography;
namespace {
std::map<std::string, std::uint64_t> counts;
std::vector<std::string> failures;
void check(std::string name, bool good) {
    ++counts[name];
    if (!good && failures.size() < 100)
        failures.push_back(name);
}
template <class T> void rejected(const Result<T> &result, std::string field, std::string reason) {
    check("error_" + reason, !result && result.error().code == ErrorCode::invalid_input &&
                                 result.error().field == field && result.error().reason == reason);
}
double periodic_distance(double a, double b) {
    double d = std::abs(a - b);
    return std::min(d, std::abs(1 - d));
}
} // namespace
int main(int argc, char **argv) {
    SpaceGroupRegistry registry;
    const SymmetryOperation identity;
    for (int id = 0; id < registry.setting_count; ++id) {
        const auto g = registry.by_id(id).value();
        const auto ops = g.operations();
        const int n = static_cast<int>(ops.size());
        check("declared_count", n == g.primitive_count() * g.centering_count());
        check("number_setting_roundtrip",
              registry.by_number(g.number(), g.setting().index).value().setting().id == id);
        check("identity_present", std::find(ops.begin(), ops.end(), identity) != ops.end());
        std::vector<int> cayley(n * n, -1);
        for (int i = 0; i < n; ++i) {
            const auto &a = ops[i];
            auto inv = a.inverse();
            check("integer_rotation", std::all_of(a.rotation().begin(), a.rotation().end(),
                                                  [](int v) { return v >= -1 && v <= 1; }));
            check("determinant", std::abs(a.determinant()) == 1);
            check("canonical_translation",
                  std::all_of(a.translation_twelfths().begin(), a.translation_twelfths().end(),
                              [](int v) { return v >= 0 && v < 12; }));
            check("unique", std::count(ops.begin(), ops.end(), a) == 1);
            check("inverse_present",
                  inv && std::find(ops.begin(), ops.end(), inv.value()) != ops.end());
            check("identity_sides",
                  a.compose(identity).value() == a && identity.compose(a).value() == a);
            if (inv)
                check("inverse_sides", a.compose(inv.value()).value() == identity &&
                                           inv.value().compose(a).value() == identity);
            // Non-dyadic translations exercise wrapping at both boundaries and
            // negative coordinates; tolerance is only for double apply properties.
            for (const auto x : {std::array<double, 3>{0, 0, 0},
                                 {1, -1, 2},
                                 {-0.17, 1.29, 2.43},
                                 {0.5, 0.3333333333333333, 0.875}}) {
                auto y = a.apply(x);
                auto back = inv.value().apply(y.value());
                auto normalized = SymmetryOperation::wrap_fractional(x).value();
                for (int k = 0; k < 3; ++k) {
                    check("apply_canonical", y.value()[k] >= 0 && y.value()[k] < 1);
                    check("apply_inverse",
                          periodic_distance(back.value()[k], normalized[k]) < 2e-14);
                }
            }
            for (int j = 0; j < n; ++j) {
                auto product = a.compose(ops[j]);
                auto found =
                    product ? std::find(ops.begin(), ops.end(), product.value()) : ops.end();
                check("closure", found != ops.end());
                if (found != ops.end())
                    cayley[n * i + j] = static_cast<int>(found - ops.begin());
                const std::array<double, 3> x{0.137, 0.291, 0.419};
                auto left = product.value().apply(x).value();
                auto right = a.apply(ops[j].apply(x).value()).value();
                for (int k = 0; k < 3; ++k)
                    check("composition_action", periodic_distance(left[k], right[k]) < 2e-14);
            }
        }
        if (std::find(cayley.begin(), cayley.end(), -1) == cayley.end()) {
            // Exhaustive associativity, including all triples in 192-operation
            // groups.
            std::uint64_t triples = 0;
            bool associative = true;
            for (int a = 0; a < n; ++a)
                for (int b = 0; b < n; ++b)
                    for (int c = 0; c < n; ++c) {
                        ++triples;
                        associative &=
                            cayley[n * cayley[n * a + b] + c] == cayley[n * a + cayley[n * b + c]];
                    }
            counts["associativity_triples"] += triples;
            check("associative_setting", associative);
        }
    }
    // Analytic non-diagonal operation: (x,y,z) -> (-y,x-y,z+1/3).
    auto hex = SymmetryOperation::create({0, -1, 0, 1, -1, 0, 0, 0, 1}, {0, 0, 4}).value();
    auto point = hex.apply({0.2, 0.7, 0.5}, false).value();
    check("column_convention", point[0] == -0.7 && std::abs(point[1] + 0.5) < 1e-15 &&
                                   std::abs(point[2] - 5.0 / 6) < 1e-15);
    check("threefold_screw", hex.compose(hex).value().compose(hex).value() == identity);
    auto normalized = SymmetryOperation::create(identity.rotation(), {-1, 12, 25}).value();
    check("exact_negative_modulo",
          normalized.translation_twelfths() == SymmetryOperation::Translation{11, 0, 1});
    check("integer_endpoint_modulo",
          SymmetryOperation::create(identity.rotation(), {std::numeric_limits<int>::min(),
                                                          std::numeric_limits<int>::max(), 0})
                  .value()
                  .translation_twelfths() == SymmetryOperation::Translation{4, 7, 0});
    for (int v : {-1, 551, std::numeric_limits<int>::max()})
        rejected(registry.by_id(v), "space_group.id", "invalid_id");
    for (int v : {-1, 0, 231})
        rejected(registry.by_number(v), "space_group.number", "invalid_number");
    for (int v : {-1, 0, 99})
        rejected(registry.by_number(230, v), "space_group.setting", "invalid_setting");
    for (auto s : {"", "_ ~\t", "P"})
        rejected(registry.find(s), "space_group.symbol", "invalid_identifier");
    for (auto s : {"bogus", "P 1:invalid", "P 1 junk"})
        rejected(registry.find(s), "space_group.symbol", "unknown_identifier");
    rejected(registry.find(std::string(41, ' ')), "space_group.symbol", "identifier_too_long");
    for (auto s : {std::string("P\n1"), std::string("P\x7f"), std::string("P\0", 2)})
        rejected(registry.find(s), "space_group.symbol", "invalid_character");
    rejected(registry.find("P1", static_cast<SymbolKind>(99)), "space_group.symbol_kind",
             "invalid_symbol_kind");
    rejected(SymmetryOperation::create({0, 0, 0, 0, 0, 0, 0, 0, 0}, {}), "symmetry.rotation",
             "not_unimodular");
    rejected(SymmetryOperation::create({2, 0, 0, 0, 1, 0, 0, 0, 1}, {}), "symmetry.rotation",
             "not_unimodular");
    rejected(SymmetryOperation::create({32768, 0, 0, 0, 1, 0, 0, 0, 1}, {}), "symmetry.rotation",
             "rotation_range");
    auto shear = SymmetryOperation::create({1, 32767, 0, 0, 1, 32767, 0, 0, 1}, {}).value();
    rejected(shear.compose(shear), "symmetry.rotation", "rotation_range");
    rejected(shear.inverse(), "symmetry.rotation", "rotation_range");
    for (auto bad :
         {std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::quiet_NaN()})
        for (int axis = 0; axis < 3; ++axis) {
            std::array<double, 3> x{};
            x[axis] = bad;
            rejected(identity.apply(x), "symmetry.coordinate", "non_finite");
            rejected(SymmetryOperation::wrap_fractional(x), "symmetry.coordinate", "non_finite");
        }
    rejected(shear.apply({0, std::numeric_limits<double>::max(), 0}, false), "symmetry.coordinate",
             "coordinate_overflow");
    check("huge_wrapped_coordinate", bool(shear.apply({0, std::numeric_limits<double>::max(), 0})));
    auto zero = SymmetryOperation::wrap_fractional({-0.0, -1, 1}).value();
    check("positive_zero", std::all_of(zero.begin(), zero.end(),
                                       [](double x) { return x == 0 && !std::signbit(x); }));
    int listed = 0;
    for (int n = 1; n <= 230; ++n) {
        check("standard_id_roundtrip", registry.by_number(n).value().setting().standard_id ==
                                           registry.by_number(n).value().setting().id);
        const auto settings = registry.settings(n).value();
        int index = 0;
        for (const auto &group : settings) {
            ++listed;
            check("enumerated_settings", group.number() == n && group.setting().index == ++index);
        }
    }
    check("complete_setting_enumeration", listed == 551);
    rejected(registry.settings(0), "space_group.number", "invalid_number");
    const auto origin1 = registry.find("F d d d:1").value();
    const auto origin2 = registry.find("F d d d:2").value();
    check("distinct_origin_choices", origin1.number() == 70 && origin2.number() == 70 &&
                                         origin1.setting().id != origin2.setting().id &&
                                         origin1.setting().choice == "1" &&
                                         origin2.setting().choice == "2");
    check("implicit_origin_two",
          registry.find("Fddd").value().setting().id == origin2.setting().id);
    const auto hex_axes = registry.find("R -3 m:H").value();
    const auto rhomb_axes = registry.find("R -3 m:R").value();
    check("distinct_rhombohedral_axes",
          hex_axes.number() == 166 && rhomb_axes.number() == 166 &&
              hex_axes.setting().id != rhomb_axes.setting().id &&
              hex_axes.operations().size() == 3 * rhomb_axes.operations().size());
    std::ofstream out(argc > 1 ? argv[1] : "symmetry-properties.json");
    out << "{\"status\":\"" << (failures.empty() ? "pass" : "fail")
        << "\",\"settings\":551,\"counts\":{";
    bool first = true;
    for (const auto &[name, count] : counts) {
        if (!first)
            out << ',';
        first = false;
        out << '"' << name << "\":" << count;
    }
    out << "},\"failures\":[";
    first = true;
    for (const auto &f : failures) {
        if (!first)
            out << ',';
        first = false;
        out << '"' << f << '"';
    }
    out << "]}\n";
    for (const auto &f : failures)
        std::cerr << f << '\n';
    return failures.empty() ? 0 : 1;
}
