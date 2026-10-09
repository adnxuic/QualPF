#include "qualpf/crystallography/reflection.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <string>
#include <vector>
using namespace qualpf;
using namespace qualpf::crystallography;
namespace {
std::map<std::string, std::size_t> counts;
std::vector<std::string> failures;
void check(const std::string &name, bool good) {
    ++counts[name];
    if (!good && failures.size() < 100)
        failures.push_back(name);
}
template <class T>
void reject(const Result<T> &r, const std::string &field, const std::string &reason) {
    check("error_" + reason, !r && r.error().code == ErrorCode::invalid_input &&
                                 r.error().field == field && r.error().reason == reason);
}
UnitCell cell_for(const SpaceGroup &group) {
    const double seed[9]{16, 1.3, .7, 1.3, 25, 2.1, .7, 2.1, 36};
    double g[9]{};
    for (auto op : group.operations()) {
        auto r = op.rotation();
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int a = 0; a < 3; ++a)
                    for (int b = 0; b < 3; ++b)
                        g[3 * i + j] += r[3 * a + i] * seed[3 * a + b] * r[3 * b + j] /
                                        group.operations().size();
    }
    UnitCellParameters p;
    for (int i = 0; i < 3; ++i)
        p.lengths_angstrom[i] = std::sqrt(g[4 * i]);
    for (int i = 0; i < 3; ++i) {
        const int a = (i + 1) % 3, b = (i + 2) % 3;
        p.angles_degrees[i] =
            std::acos(g[3 * a + b] / p.lengths_angstrom[a] / p.lengths_angstrom[b]) * 180 /
            std::numbers::pi;
    }
    return UnitCell::create(p).value();
}
bool same(std::span<const Hkl> a, std::span<const Hkl> b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), equal_hkl);
}
bool close(double a, double b) {
    return std::abs(a - b) <= 1e-11 * std::max(std::abs(a), std::abs(b));
}
} // namespace
int main(int argc, char **argv) {
    const SpaceGroupRegistry registry;
    for (int id = 0; id < 551; ++id) {
        const auto group = registry.by_id(id).value();
        const auto cell = cell_for(group);
        check("setting_coverage", true);
        for (int h = -1; h <= 1; ++h)
            for (int k = -1; k <= 1; ++k)
                for (int l = -1; l <= 1; ++l) {
                    if (h == 0 && k == 0 && l == 0)
                        continue;
                    const Hkl index{h, k, l};
                    const auto base_d = cell.d_spacing(index).value().value;
                    const auto norm = cell.reciprocal_vector_norm(index).value().inverse_angstrom;
                    const bool absent = systematically_absent(index, group).value();
                    for (auto policy : {FriedelPolicy::separate, FriedelPolicy::merge}) {
                        const auto cls = reflection_class(index, group, policy).value();
                        check("reflexive", std::any_of(cls.members().begin(), cls.members().end(),
                                                       [&](Hkl m) { return equal_hkl(m, index); }));
                        std::set<Hkl, HklLess> independent;
                        for (const auto &op : group.operations()) {
                            const auto &r = op.rotation();
                            // Deliberately spell out column-dot products independently.
                            Hkl image{h * r[0] + k * r[3] + l * r[6],
                                      h * r[1] + k * r[4] + l * r[7],
                                      h * r[2] + k * r[5] + l * r[8]};
                            check("row_action",
                                  equal_hkl(image, reciprocal_image(index, op).value()));
                            independent.insert(image);
                            if (policy == FriedelPolicy::merge)
                                independent.insert({-image.h, -image.k, -image.l});
                        }
                        check("class_size", cls.multiplicity() == independent.size());
                        check("orbit_exact",
                              std::equal(independent.begin(), independent.end(),
                                         cls.members().begin(), cls.members().end(), equal_hkl));
                        check("canonical_max", equal_hkl(cls.canonical(), *independent.rbegin()));
                        for (auto member : cls.members()) {
                            const auto next = reflection_class(member, group, policy).value();
                            check("symmetric_transitive", same(cls.members(), next.members()));
                            check("canonical_invariant",
                                  equal_hkl(cls.canonical(), next.canonical()));
                            check("d_invariant",
                                  close(base_d, cell.d_spacing(member).value().value));
                            check(
                                "reciprocal_norm_invariant",
                                close(
                                    norm,
                                    cell.reciprocal_vector_norm(member).value().inverse_angstrom));
                            check("absence_invariant",
                                  absent == systematically_absent(member, group).value());
                        }
                    }
                }
        for (auto a : group.operations())
            for (auto b : group.operations()) {
                const Hkl seed{1, 2, 3};
                const auto product = a.compose(b).value();
                check("row_action_composition",
                      equal_hkl(reciprocal_image(seed, product).value(),
                                reciprocal_image(reciprocal_image(seed, a).value(), b).value()));
            }
        ReflectionGenerationOptions options;
        options.minimum_d = DSpacingAngstrom{2.13};
        options.wavelength = WavelengthAngstrom{.01};
        for (auto policy : {FriedelPolicy::separate, FriedelPolicy::merge}) {
            options.friedel = policy;
            const auto generated = generate_reflections(cell, group, options);
            check("generation_success", bool(generated));
            if (!generated)
                continue;
            const auto repeated = generate_reflections(cell, group, options).value();
            const auto refs = generated.value().reflections();
            check("repeat_count", refs.size() == repeated.reflections().size());
            std::set<Hkl, HklLess> seen;
            for (std::size_t i = 0; i < refs.size(); ++i) {
                const auto &ref = refs[i];
                check("no_duplicates", seen.insert(ref.hkl).second);
                check("no_zero", !equal_hkl(ref.hkl, {0, 0, 0}));
                check("finite_geometry", std::isfinite(ref.d.value) && ref.d.value >= 2.13 &&
                                             std::isfinite(ref.two_theta.value) &&
                                             ref.two_theta.value > 0);
                check("generated_allowed", !systematically_absent(ref.hkl, group).value());
                check("generated_multiplicity",
                      ref.multiplicity ==
                          reflection_class(ref.hkl, group, policy).value().multiplicity());
                const auto &rep = repeated.reflections()[i];
                check("deterministic", equal_hkl(ref.hkl, rep.hkl) && ref.d == rep.d &&
                                           ref.two_theta == rep.two_theta &&
                                           ref.multiplicity == rep.multiplicity);
                if (i)
                    check("ordered", refs[i - 1].two_theta.value <= ref.two_theta.value);
            }
            // Independently scan an oversized cube and collect all orbit representatives.
            // No use of the implementation's index bound or canonical-only insertion.
            std::set<Hkl, HklLess> brute;
            for (int h = -5; h <= 5; ++h)
                for (int k = -5; k <= 5; ++k)
                    for (int l = -5; l <= 5; ++l) {
                        Hkl candidate{h, k, l};
                        if (equal_hkl(candidate, {0, 0, 0}))
                            continue;
                        if (cell.d_spacing(candidate).value().value < 2.13 ||
                            systematically_absent(candidate, group).value())
                            continue;
                        brute.insert(
                            reflection_class(candidate, group, policy).value().canonical());
                    }
            check("enumeration_complete",
                  seen.size() == brute.size() &&
                      std::equal(seen.begin(), seen.end(), brute.begin(), brute.end(), equal_hkl));
        }
        // Symmorphic examples isolate centering from screw/glide extinctions.
        if (group.number() == 1 || group.number() == 5 || group.number() == 22 ||
            group.number() == 23 || group.number() == 146) {
            const char lattice = group.hermann_mauguin().front();
            // Four resource settings have xHM "Unknown"; general exact and
            // invariance checks above still cover them. A named analytic rule
            // must not infer their lattice from that placeholder.
            if (std::string("PABCIFR").find(lattice) == std::string::npos)
                continue;
            for (int h = -3; h <= 3; ++h)
                for (int k = -3; k <= 3; ++k)
                    for (int l = -3; l <= 3; ++l) {
                        if (h == 0 && k == 0 && l == 0)
                            continue;
                        bool absent = false;
                        switch (lattice) {
                        case 'A':
                            absent = (k + l) % 2 != 0;
                            break;
                        case 'B':
                            absent = (h + l) % 2 != 0;
                            break;
                        case 'C':
                            absent = (h + k) % 2 != 0;
                            break;
                        case 'I':
                            absent = (h + k + l) % 2 != 0;
                            break;
                        case 'F':
                            absent = (h + k) % 2 != 0 || (h + l) % 2 != 0;
                            break;
                        case 'R':
                            absent = group.centering_count() > 1 && (-h + k + l) % 3 != 0;
                            break;
                        }
                        check(std::string("centering_") + lattice,
                              absent == systematically_absent({h, k, l}, group).value());
                    }
        }
    }
    for (const auto [number, period] :
         {std::pair{4, 2}, {19, 2}, {76, 4}, {144, 3}, {145, 3}, {169, 6}, {170, 6}, {227, 4}}) {
        const auto g = registry.by_number(number).value();
        for (int n = -120; n <= 120; ++n) {
            if (n == 0)
                continue;
            const Hkl h = number == 4 ? Hkl{0, n, 0} : Hkl{0, 0, n};
            check("analytic_axis_" + std::to_string(number),
                  systematically_absent(h, g).value() == (n % period != 0));
        }
    }
    const auto glide = registry.by_number(14).value();
    for (int h = -4; h <= 4; ++h)
        for (int l = -4; l <= 4; ++l) {
            if (h == 0 && l == 0)
                continue;
            check("analytic_c_glide",
                  systematically_absent({h, 0, l}, glide).value() == (l % 2 != 0));
        }
    const auto p1 = registry.by_number(1).value(), pinv = registry.by_number(2).value();
    const auto cubic = UnitCell::create({{4, 4, 4}, {90, 90, 90}}).value();
    check("acentric_separate",
          reflection_class({1, 2, 3}, p1, FriedelPolicy::separate).value().multiplicity() == 1);
    check("acentric_powder",
          reflection_class({1, 2, 3}, p1, FriedelPolicy::merge).value().multiplicity() == 2);
    check("centric_inversion",
          reflection_class({1, 2, 3}, pinv, FriedelPolicy::separate).value().multiplicity() == 2);
    const auto trig = registry.by_number(144).value();
    check("non_diagonal_convention",
          equal_hkl(reciprocal_image({1, 2, 0}, trig.operations()[2]).value(), {-3, 1, 0}));
    ReflectionGenerationOptions base;
    base.wavelength = WavelengthAngstrom{1};
    base.minimum_d = cubic.d_spacing({1, 0, 0}).value();
    base.maximum_d = base.minimum_d;
    auto boundary = generate_reflections(cubic, p1, base).value();
    check("closed_d_boundary",
          std::any_of(boundary.reflections().begin(), boundary.reflections().end(), [](auto r) {
              return equal_hkl(r.hkl, {1, 0, 0});
          }));
    base.maximum_d.reset();
    base.minimum_d = DSpacingAngstrom{.5};
    base.minimum_two_theta = two_theta(cubic.d_spacing({1, 0, 0}).value(), base.wavelength).value();
    base.maximum_two_theta = base.minimum_two_theta;
    const auto angle_boundary = generate_reflections(cubic, p1, base).value();
    check("closed_angle_boundary", std::any_of(angle_boundary.reflections().begin(),
                                               angle_boundary.reflections().end(), [](auto r) {
                                                   return equal_hkl(r.hkl, {1, 0, 0});
                                               }));
    for (Hkl index : {Hkl{1, 0, 0}, Hkl{0, 1, 0}, Hkl{0, 0, 1}, Hkl{1, 1, 1}}) {
        auto endpoint = ReflectionGenerationOptions{};
        endpoint.wavelength = WavelengthAngstrom{1};
        const auto d = cubic.d_spacing(index).value();
        const auto angle = two_theta(d, endpoint.wavelength).value();
        auto contains = [&](const ReflectionGenerationOptions &options) {
            const auto set = generate_reflections(cubic, p1, options).value();
            return std::any_of(set.reflections().begin(), set.reflections().end(),
                               [&](auto r) { return equal_hkl(r.hkl, index); });
        };
        endpoint.minimum_d = d;
        endpoint.maximum_d = d;
        check("closed_d_per_axis", contains(endpoint));
        endpoint.maximum_d.reset();
        endpoint.minimum_d = DSpacingAngstrom{std::nextafter(d.value, INFINITY)};
        check("d_one_ulp_outside", !contains(endpoint));
        endpoint.minimum_d = DSpacingAngstrom{.5};
        endpoint.minimum_two_theta = angle;
        endpoint.maximum_two_theta = angle;
        check("closed_angle_per_axis", contains(endpoint));
        endpoint.minimum_two_theta = TwoThetaDegrees{0};
        endpoint.maximum_two_theta = TwoThetaDegrees{std::nextafter(angle.value, 0)};
        check("angle_one_ulp_outside", !contains(endpoint));
    }
    base = {};
    base.minimum_d = DSpacingAngstrom{2};
    base.wavelength = WavelengthAngstrom{.01};
    const auto longcell = UnitCell::create({{240, 1, 1}, {90, 90, 90}}).value();
    auto high = generate_reflections(longcell, p1, base).value();
    check(
        "bounds_beyond_legacy_100",
        std::any_of(high.reflections().begin(), high.reflections().end(), [](const Reflection &r) {
            return equal_hkl(r.hkl, {120, 0, 0});
        }));
    reject(reflection_class({0, 0, 0}, p1, FriedelPolicy::merge), "reflection.hkl", "zero_hkl");
    reject(systematically_absent({0, 0, 0}, p1), "reflection.hkl", "zero_hkl");
    reject(reflection_class({1, 0, 0}, p1, static_cast<FriedelPolicy>(2)), "reflection.friedel",
           "invalid_policy");
    const auto lowest = std::numeric_limits<std::int32_t>::min();
    reject(reflection_class({lowest, 0, 0}, p1, FriedelPolicy::merge), "reflection.hkl",
           "index_overflow");
    check("int32_min_separate",
          bool(reflection_class({lowest, 0, 0}, p1, FriedelPolicy::separate)));
    reject(systematically_absent({lowest, 0, 0}, pinv), "reflection.hkl", "index_overflow");
    for (double v : {0., -1., std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN()}) {
        auto o = base;
        o.wavelength = WavelengthAngstrom{v};
        reject(generate_reflections(cubic, p1, o), "reflection.wavelength",
               "nonpositive_or_nonfinite");
        o = base;
        o.minimum_d = DSpacingAngstrom{v};
        reject(generate_reflections(cubic, p1, o), "reflection.d_range",
               "nonpositive_or_nonfinite");
        o = base;
        o.maximum_d = DSpacingAngstrom{v};
        reject(generate_reflections(cubic, p1, o), "reflection.d_range",
               "nonpositive_or_nonfinite");
    }
    auto o = base;
    o.maximum_d = DSpacingAngstrom{1};
    reject(generate_reflections(cubic, p1, o), "reflection.d_range", "reversed_range");
    for (double v : {0., -1., 181., std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::quiet_NaN()}) {
        o = base;
        o.maximum_two_theta = TwoThetaDegrees{v};
        reject(generate_reflections(cubic, p1, o), "reflection.angle_range", "invalid_angle");
    }
    o = base;
    o.minimum_two_theta = TwoThetaDegrees{40};
    o.maximum_two_theta = TwoThetaDegrees{20};
    reject(generate_reflections(cubic, p1, o), "reflection.angle_range", "reversed_range");
    o = base;
    o.maximum_candidates = 0;
    reject(generate_reflections(cubic, p1, o), "reflection.maximum_candidates", "zero_budget");
    o = base;
    o.maximum_candidates = 1;
    reject(generate_reflections(cubic, p1, o), "reflection.maximum_candidates",
           "candidate_budget_exceeded");
    o = base;
    o.order = static_cast<ReflectionOrder>(3);
    reject(generate_reflections(cubic, p1, o), "reflection.order", "invalid_policy");
    o = base;
    o.friedel = static_cast<FriedelPolicy>(3);
    reject(generate_reflections(cubic, p1, o), "reflection.friedel", "invalid_policy");
    reject(generate_reflections(longcell, registry.by_number(225).value(), base), "reflection.cell",
           "incompatible_symmetry");
    const auto giant = UnitCell::create({{1e12, 1, 1}, {90, 90, 90}}).value();
    reject(generate_reflections(giant, p1, base), "reflection.bounds", "index_overflow");
    o = base;
    o.order = ReflectionOrder::hkl;
    auto lex = generate_reflections(cubic, p1, o).value();
    check("lex_order", std::is_sorted(lex.reflections().begin(), lex.reflections().end(),
                                      [](auto a, auto b) { return HklLess{}(a.hkl, b.hkl); }));
    std::ofstream file(argc > 1 ? argv[1] : "reflection-properties.json");
    file << "{\"status\":\"" << (failures.empty() ? "passed" : "failed")
         << "\",\"settings\":551,\"counts\":{";
    bool first = true;
    for (auto [name, count] : counts) {
        if (!first)
            file << ',';
        first = false;
        file << '"' << name << "\":" << count;
    }
    file << "},\"failures\":[";
    first = true;
    for (auto &f : failures) {
        if (!first)
            file << ',';
        first = false;
        file << '"' << f << '"';
    }
    file << "]}\n";
    std::size_t total = 0;
    for (auto [_, n] : counts)
        total += n;
    std::cout << total << " property checks; " << failures.size() << " failures\n";
    return failures.empty() ? 0 : 1;
}
