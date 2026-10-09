#include "qualpf/crystallography/structure_factor.h"
#include "../../../src/core/crystallography/phase_sincos.h"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <numbers>
#include <string>
#include <stdexcept>
#include <tuple>
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
bool close(double a, double b, double tolerance = 2e-9) {
  return std::abs(a - b) <=
         tolerance * (1 + std::max(std::abs(a), std::abs(b)));
}
bool close(std::complex<double> a, std::complex<double> b) {
  return close(a.real(), b.real()) && close(a.imag(), b.imag());
}
bool same_bits(std::complex<double> a, std::complex<double> b) {
  return std::bit_cast<std::uint64_t>(a.real()) ==
             std::bit_cast<std::uint64_t>(b.real()) &&
         std::bit_cast<std::uint64_t>(a.imag()) ==
             std::bit_cast<std::uint64_t>(b.imag());
}
template <class T>
void reject(const Result<T> &r, const std::string &reason,
            const std::string &field = {}) {
  const std::map<std::string, std::string> fields{
      {"invalid_occupancy", "site.occupancy"},
      {"invalid_fractional_coordinate", "site.position"},
      {"invalid_isotropic_displacement", "site.displacement"},
      {"invalid_scattering_variable", "scattering.s_squared"},
      {"nonfinite_displacement", "site.displacement"},
      {"invalid_anisotropic_tensor", "site.displacement"},
      {"incompatible_special_position_tensor", "site.displacement"},
      {"unknown_species", "species"},
      {"unsupported_scattering_entry", "species.scattering"},
      {"unsupported_anomalous_species", "species.anomalous"},
      {"unsupported_radiation", "radiation.kind"},
      {"unsupported_anomalous_radiation", "radiation.anomalous"},
      {"invalid_wavelength", "radiation.wavelength"},
      {"wavelength_outside_table", "radiation.wavelength"},
      {"nonfinite_scattering_factor", "scattering.amplitude"},
      {"empty_structure", "structure.sites"},
      {"incompatible_symmetry", "structure.cell"},
      {"reflection_cell_mismatch", "reflection.d"},
      {"reflection_group_mismatch", "reflection_set.group"},
      {"nonfinite_result", "structure_factor.result"},
      {"nonfinite_displacement_exponent", "structure_factor.displacement"},
      {"zero_hkl", "hkl"}};
  const auto expected_field = field.empty() ? fields.at(reason) : field;
  const auto code =
      reason.starts_with("unsupported_") || reason == "wavelength_outside_table"
          ? ErrorCode::unsupported
      : reason == "nonfinite_scattering_factor" ||
              reason == "nonfinite_result" ||
              reason == "nonfinite_displacement_exponent"
          ? ErrorCode::numerical_failure
          : ErrorCode::invalid_input;
  check("error_" + reason, !r && r.error().reason == reason &&
                               r.error().field == expected_field &&
                               r.error().code == code);
  if (r)
    std::cerr << "expected error " << reason << ", received success\n";
  else if (r.error().reason != reason || r.error().field != expected_field ||
           r.error().code != code)
    std::cerr << "expected error " << reason << ", received "
              << r.error().field << ':' << r.error().reason << '\n';
}
AtomicSite site(std::string_view label, std::array<double, 3> x, double occ = 1,
                double b = 0) {
  return {ScatteringRegistry{}.find(label).value(),
          FractionalPosition::create(x).value(), Occupancy::create(occ).value(),
          Displacement::isotropic_b(b).value()};
}
UnitCell cell_for(const SpaceGroup &group) {
  const double seed[9]{16, 1.3, .7, 1.3, 25, 2.1, .7, 2.1, 36};
  double g[9]{};
  for (const auto &op : group.operations()) {
    const auto &r = op.rotation();
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        for (int a = 0; a < 3; ++a)
          for (int b = 0; b < 3; ++b)
            g[3 * i + j] += r[3 * a + i] * seed[3 * a + b] * r[3 * b + j] /
                            group.operations().size();
  }
  UnitCellParameters p{};
  for (int i = 0; i < 3; ++i)
    p.lengths_angstrom[i] = std::sqrt(g[4 * i]);
  for (int i = 0; i < 3; ++i) {
    const int a = (i + 1) % 3, b = (i + 2) % 3;
    p.angles_degrees[i] = std::acos(g[3 * a + b] / p.lengths_angstrom[a] /
                                    p.lengths_angstrom[b]) *
                          180 / std::numbers::pi;
  }
  return UnitCell::create(p).value();
}
StructureFactor evaluate(const CrystalStructure &s, Hkl h,
                         ScatteringRadiation r = {}) {
  auto result = structure_factor(s, h, r);
  check("finite_valid_result",
        bool(result) && std::isfinite(result.value().fc_squared));
  return result ? result.value() : StructureFactor{{0, 0}, 0};
}
void phase_contract(const char *reference) {
  std::ifstream file(reference ? reference : "");
  check("phase_reference_present", bool(file));
  std::vector<std::array<std::uint64_t, 3>> cases;
  std::array<std::uint64_t, 3> row;
  while (file >> std::hex >> row[0] >> row[1] >> row[2])
    cases.push_back(row);
  check("phase_reference_complete", cases.size() == 394 && file.eof());
  const int saved_rounding = std::fegetround();
  std::fenv_t saved;
  if (std::fegetenv(&saved) != 0) {
    check("phase_environment_saved", false);
    return;
  }
  for (int rounding : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
    if (std::fesetround(rounding) != 0) {
      check("phase_rounding_selected", false);
      continue;
    }
    for (const auto &entry : cases) {
      for (bool rational : {false, true}) {
        const auto result = detail::phase_sincos_bits(entry[0], rational);
        check(rational ? "phase_exact_rational" : "phase_exact_certifier",
              result.sine == entry[1] && result.cosine == entry[2]);
      }
    }
    check("phase_rounding_preserved", std::fegetround() == rounding);
  }
  check("phase_environment_restored", std::fesetenv(&saved) == 0);
  // Restore rounding explicitly before the preexisting caller/error checks.
  check("phase_rounding_restored", std::fesetround(saved_rounding) == 0 &&
                                      std::fegetround() == saved_rounding);
  for (std::uint64_t invalid : {0x400921fb54442d19ULL, 0xc00921fb54442d19ULL,
                               0x7ff0000000000000ULL, 0xfff0000000000000ULL,
                               0x7ff8000000000000ULL}) {
    bool rejected = false;
    try {
      (void)detail::phase_sincos_bits(invalid);
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    check("phase_private_invalid_argument", rejected);
  }
}
} // namespace
int main(int argc, char **argv) {
  phase_contract(argc > 2 ? argv[2] : nullptr);
  const SpaceGroupRegistry groups;
  for (int id = 0; id < 551; ++id) {
    const auto g = groups.by_id(id).value();
    const auto cell = cell_for(g);
    std::vector<AtomicSite> sites{site("Si", {.137, .271, .389}, .8, .4),
                                  site("O", {.21, .37, .43}, .7, .6)};
    auto s = CrystalStructure::create(cell, g, sites);
    check("all_setting_structure", bool(s));
    if (!s)
      continue;
    const Hkl h{1, 2, 3};
    auto f = evaluate(s.value(), h);
    auto minus = evaluate(s.value(), {-1, -2, -3});
    check("friedel", close(minus.amplitude, std::conj(f.amplitude)));
    if (g.centrosymmetry() == 1)
      check("centrosymmetric_real", close(f.amplitude.imag(), 0));
    for (const auto &op : g.operations()) {
      auto image = reciprocal_image(h, op).value();
      check("symmetry_equivalent_fc2",
            close(f.fc_squared, evaluate(s.value(), image).fc_squared));
    }
    std::reverse(sites.begin(), sites.end());
    auto permutation = CrystalStructure::create(cell, g, sites).value();
    check("bitwise_atom_order_independence",
          same_bits(f.amplitude, evaluate(permutation, h).amplitude));
    for (auto &a : sites) {
      auto x = a.position.coordinates();
      x[0] += 2;
      x[1] -= 3;
      x[2] += 1;
      a.position = FractionalPosition::create(x).value();
    }
    auto shifted = CrystalStructure::create(cell, g, sites).value();
    check("lattice_translation",
          close(f.amplitude, evaluate(shifted, h).amplitude));
    sites.push_back(site("Fe", {.123, .235, .367}, 0, 1));
    auto zero = CrystalStructure::create(cell, g, sites).value();
    check("zero_occupancy",
          evaluate(shifted, h).amplitude == evaluate(zero, h).amplitude);
  }
  const auto p1 = groups.by_number(1).value();
  const auto cubic = UnitCell::create({{4, 4, 4}, {90, 90, 90}}).value();
  const auto si = ScatteringRegistry{}.find("Si").value();
  const auto amp =
      scattering_amplitude(si, ScatteringVariable::from_s(.125).value(), {})
          .value();
  auto origin =
      CrystalStructure::create(cubic, p1, {site("Si", {0, 0, 0})}).value();
  check("origin_analytic", close(evaluate(origin, {1, 0, 0}).amplitude, amp));
  const ScatteringRadiation anomalous{RadiationKind::xray,
                                      WavelengthAngstrom{1.54059307}, true};
  const auto complex_amp =
      scattering_amplitude(si, ScatteringVariable::from_s(.125).value(),
                           anomalous)
          .value();
  check("anomalous_origin_analytic",
        close(evaluate(origin, {1, 0, 0}, anomalous).amplitude, complex_amp));
  check("anomalous_complex_not_conjugate_friedel",
        !close(evaluate(origin, {-1, 0, 0}, anomalous).amplitude,
               std::conj(complex_amp)));
  auto mono = CrystalStructure::create(cubic, groups.by_number(221).value(),
                                       {site("Si", {0, 0, 0})})
                  .value();
  check("monoatomic_cubic_unique", mono.expanded_sites().size() == 1);
  check("monoatomic_cubic_analytic",
        close(evaluate(mono, {1, 0, 0}).amplitude, amp));
  auto bcc = CrystalStructure::create(cubic, groups.by_number(229).value(),
                                      {site("Si", {0, 0, 0})})
                 .value();
  check("centered_cubic_unique", bcc.expanded_sites().size() == 2);
  check("centered_cubic_extinct", evaluate(bcc, {1, 0, 0}).fc_squared == 0);
  const auto amp110 =
      scattering_amplitude(
          si,
          ScatteringVariable::from_d(cubic.d_spacing({1, 1, 0}).value())
              .value(),
          {})
          .value();
  check("centered_cubic_allowed",
        close(evaluate(bcc, {1, 1, 0}).amplitude, 2. * amp110));
  for (auto [number, pos, count] :
       std::vector<std::tuple<int, std::array<double, 3>, std::size_t>>{
           {2, {0, 0, 0}, 1},
           {2, {.13, .27, .39}, 2},
           {6, {.13, 0, .39}, 1},
           {3, {0, .27, 0}, 1},
           {4, {.13, .27, .39}, 2},
           {19, {.13, .27, .39}, 4}}) {
    auto g = groups.by_number(number, 1).value();
    auto expansion = expand_site(site("Si", pos), cell_for(g), g);
    check("special_position_count",
          expansion && expansion.value().size() == count);
  }
  std::vector<AtomicSite> base{site("Si", {.13, .27, .39}, .7, .8),
                               site("O", {.37, .41, .21}, 1, .4)};
  auto before = CrystalStructure::create(cubic, p1, base).value();
  for (auto &a : base) {
    auto x = a.position.coordinates();
    for (double &v : x)
      v += .179;
    a.position = FractionalPosition::create(x).value();
  }
  auto after = CrystalStructure::create(cubic, p1, base).value();
  check("origin_shift_fc2", close(evaluate(before, {2, 3, 4}).fc_squared,
                                  evaluate(after, {2, 3, 4}).fc_squared));
  for (double u : {0., .001, .1, 10.}) {
    auto a = site("Si", {.13, .27, .39});
    a.displacement = Displacement::isotropic_u(u).value();
    auto b = a;
    b.displacement =
        Displacement::isotropic_b(8 * std::numbers::pi * std::numbers::pi * u)
            .value();
    check("B_U_equivalence",
          evaluate(CrystalStructure::create(cubic, p1, {a}).value(), {1, 2, 3})
                  .amplitude ==
              evaluate(CrystalStructure::create(cubic, p1, {b}).value(),
                       {1, 2, 3})
                  .amplitude);
  }
  const auto triclinic = UnitCell::create({{5, 6, 7}, {76, 83, 67}}).value();
  const DisplacementTensor u{.01, .02, .03, .003, -.002, .004};
  auto beta = Displacement::anisotropic_u(u).value().beta(triclinic).value();
  auto back = crystallographic_u_from_beta(beta, triclinic).value();
  auto tensor_b = crystallographic_b_from_beta(beta, triclinic).value();
  auto beta_b =
      Displacement::anisotropic_b(tensor_b).value().beta(triclinic).value();
  for (std::size_t i = 0; i < 6; ++i) {
    check("nonorthogonal_U_beta_roundtrip", close(u[i], back[i], 1e-13));
    check("nonorthogonal_B_beta_roundtrip", close(beta[i], beta_b[i], 1e-13));
  }
  auto aniso = site("Si", {.13, .27, .39});
  aniso.displacement = Displacement::anisotropic_u(u).value();
  auto aniso_structure =
      CrystalStructure::create(triclinic, p1, {aniso}).value();
  auto af = evaluate(aniso_structure, {1, 2, 3});
  check("anisotropic_friedel",
        close(evaluate(aniso_structure, {-1, -2, -3}).amplitude,
              std::conj(af.amplitude)));
  auto broken = site("Si", {0, 0, 0});
  broken.displacement = Displacement::anisotropic_u(u).value();
  reject(
      CrystalStructure::create(cubic, groups.by_number(221).value(), {broken}),
      "incompatible_special_position_tensor");
  const double nan = std::numeric_limits<double>::quiet_NaN(),
               inf = std::numeric_limits<double>::infinity();
  for (double v : {-.1, 1.1, nan, inf})
    reject(Occupancy::create(v), "invalid_occupancy");
  for (double v : {nan, inf})
    reject(FractionalPosition::create({v, 0, 0}),
           "invalid_fractional_coordinate");
  for (double v : {-1., nan, inf}) {
    reject(Displacement::isotropic_b(v), "invalid_isotropic_displacement");
    reject(ScatteringVariable::from_squared(v), "invalid_scattering_variable");
  }
  reject(Displacement::anisotropic_u({nan, 0, 0, 0, 0, 0}),
         "nonfinite_displacement");
  for (auto t : {DisplacementTensor{-1, -1, 1, 0, 0, 0},
                 DisplacementTensor{1, 1, 1, 2, 0, 0}})
    reject(Displacement::anisotropic_u(t), "invalid_anisotropic_tensor");
  reject(ScatteringRegistry{}.find("Fe+3"), "unknown_species");
  reject(ScatteringRegistry{}.by_index(299), "unknown_species");
  reject(scattering_amplitude(ScatteringRegistry{}.find("Rf").value(),
                              ScatteringVariable::from_s(0).value(), {}),
         "unsupported_scattering_entry");
  reject(anomalous_correction(ScatteringRegistry{}.find("Cf").value(),
                              WavelengthAngstrom{1.54}),
         "unsupported_anomalous_species");
  reject(validate_radiation(
             {static_cast<RadiationKind>(9), WavelengthAngstrom{1.54}, false}),
         "unsupported_radiation");
  reject(validate_radiation(
             {RadiationKind::neutron, WavelengthAngstrom{1.54}, true}),
         "unsupported_anomalous_radiation");
  for (double v : {0., -1., nan, inf})
    reject(
        validate_radiation({RadiationKind::xray, WavelengthAngstrom{v}, false}),
        "invalid_wavelength");
  reject(anomalous_correction(si, WavelengthAngstrom{1e9}),
         "wavelength_outside_table");
  reject(scattering_amplitude(ScatteringRegistry{}.find("O2-").value(),
                              ScatteringVariable::from_squared(1e8).value(),
                              {}),
         "nonfinite_scattering_factor");
  reject(CrystalStructure::create(cubic, p1, {}), "empty_structure");
  reject(CrystalStructure::create(triclinic, groups.by_number(221).value(),
                                  {site("Si", {0, 0, 0})}),
         "incompatible_symmetry");
  ReflectionGenerationOptions options;
  options.minimum_d = DSpacingAngstrom{2};
  options.wavelength = WavelengthAngstrom{1.54059307};
  const auto refs = generate_reflections(cubic, p1, options).value();
  check("reflection_set", bool(structure_factors(origin, refs, {})));
  auto ref = refs.reflections()[0];
  ref.d.value *= 2;
  reject(structure_factor(origin, ref, {}), "reflection_cell_mismatch");
  ref = refs.reflections()[0];
  ref.two_theta.value += 1;
  reject(structure_factor(origin, ref, {}), "reflection_radiation_mismatch",
         "reflection.two_theta");
  reject(
      structure_factors(origin, refs,
                        {RadiationKind::xray, WavelengthAngstrom{.7}, false}),
      "reflection_radiation_mismatch", "reflection_set.wavelength");
  reject(structure_factors(bcc, refs, {}), "reflection_group_mismatch");
  reject(structure_factor(origin, Hkl{0, 0, 0}, {}), "zero_hkl");
  const auto tiny =
      UnitCell::create({{.0028, .0028, .0028}, {90, 90, 90}}).value();
  const auto overflow_structure =
      CrystalStructure::create(tiny, p1, {site("O2-", {0, 0, 0})}).value();
  reject(structure_factor(overflow_structure, Hkl{1, 0, 0}, {}),
         "nonfinite_result");
  auto enormous = site("Si", {0, 0, 0});
  enormous.displacement =
      Displacement::anisotropic_beta({1e300, 1e300, 1e300, 0, 0, 0}).value();
  const auto exponent_overflow =
      CrystalStructure::create(cubic, p1, {enormous}).value();
  reject(structure_factor(exponent_overflow, Hkl{1000000000, 0, 0}, {}),
         "nonfinite_displacement_exponent");
  std::ofstream file(argc > 1 ? argv[1] : "scattering-properties.json");
  file << "{\n  \"success\": " << (failures.empty() ? "true" : "false")
       << ",\n  \"counts\": {";
  bool first = true;
  std::size_t total = 0;
  for (auto [name, n] : counts) {
    if (!first)
      file << ',';
    first = false;
    file << "\n    \"" << name << "\": " << n;
    total += n;
  }
  file << "\n  },\n  \"total\": " << total << ",\n  \"failures\": [";
  first = true;
  for (const auto &f : failures) {
    if (!first)
      file << ',';
    first = false;
    file << '"' << f << '"';
    std::cerr << f << '\n';
  }
  file << "]\n}\n";
  std::cout << total << " checks, " << failures.size() << " failures\n";
  return failures.empty() ? 0 : 1;
}
