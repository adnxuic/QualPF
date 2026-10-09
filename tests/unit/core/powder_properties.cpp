#include "qualpf/crystallography/powder_intensity.h"
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
#include <set>
#include <string>
#include <vector>
using namespace qualpf;
using namespace qualpf::crystallography;
namespace {
std::map<std::string, std::size_t> counts;
std::vector<std::string> failures;
struct Rejection {
  std::string reason, field;
  bool success;
};
std::vector<Rejection> errors;
void check(const std::string &name, bool pass) {
  ++counts[name];
  if (!pass && failures.size() < 100)
    failures.push_back(name);
}
bool close(double a, double b) {
  return std::abs(a - b) <= 2e-10 * std::max({1., std::abs(a), std::abs(b)});
}
template <class T>
void reject(const Result<T> &r, const std::string &field,
            const std::string &reason,
            ErrorCode code = ErrorCode::invalid_input) {
  const bool pass = !r && r.error().field == field &&
                    r.error().reason == reason && r.error().code == code;
  check("error." + reason, pass);
  errors.push_back({reason, field, pass});
}
Reflection reflection(double angle, std::size_t m = 2, int h = 1) {
  return {{h, 0, 0},
          d_spacing(TwoThetaDegrees{angle}, WavelengthAngstrom{1}).value(),
          TwoThetaDegrees{angle},
          m};
}
PowderIntensityOptions options() {
  PowderIntensityOptions o;
  o.radiation.wavelength = WavelengthAngstrom{1};
  return o;
}
bool same(const PowderLineSet &a, const PowderLineSet &b, bool exact = true) {
  if (a.lines().size() != b.lines().size())
    return false;
  for (std::size_t i = 0; i < a.lines().size(); ++i) {
    const auto &x = a.lines()[i], &y = b.lines()[i];
    if (x.representative_identity != y.representative_identity ||
        x.members.size() != y.members.size() ||
        !equal_hkl(x.representative.hkl, y.representative.hkl) ||
        x.representative.two_theta.value != y.representative.two_theta.value ||
        x.representative.d.value != y.representative.d.value)
      return false;
    if (exact ? x.raw_intensity != y.raw_intensity ||
                    x.relative_intensity != y.relative_intensity
              : !close(x.relative_intensity, y.relative_intensity))
      return false;
    for (std::size_t j = 0; j < x.members.size(); ++j)
      if (x.members[j].identity != y.members[j].identity)
        return false;
  }
  return true;
}
void exercise(const CrystalStructure &structure, const ReflectionSet &refs,
              const PowderIntensityOptions &o,
              const std::vector<AtomicSite> &sites) {
  auto bound = PowderStructureFactors::calculate(structure, refs, o.radiation);
  check("bind_phase15_phase16", bool(bound));
  if (!bound)
    return;
  check("bound_owner",
        bound.value().group_setting_id() == structure.group().setting().id &&
            bound.value().cell().lengths_angstrom ==
                structure.cell().parameters().lengths_angstrom &&
            bound.value().cell().angles_degrees ==
                structure.cell().parameters().angles_degrees);
  auto result = powder_lines(refs, bound.value(), o);
  check("full_pipeline", bool(result));
  if (!result)
    return;
  auto repeated = powder_lines(structure, refs, o);
  check("deterministic_recomputation",
        repeated && same(result.value(), repeated.value()));
  auto gen = refs.options();
  gen.order = ReflectionOrder::hkl;
  auto reordered =
      generate_reflections(structure.cell(), structure.group(), gen);
  check("reflection_order_independence",
        reordered &&
            same(result.value(),
                 powder_lines(reordered.value(), bound.value(), o).value()));
  auto reversed_sites = sites;
  std::reverse(reversed_sites.begin(), reversed_sites.end());
  auto permuted = CrystalStructure::create(structure.cell(), structure.group(),
                                           reversed_sites);
  auto atom_result = powder_lines(permuted.value(), refs, o);
  check("atom_order_bitwise_independence",
        atom_result && same(result.value(), atom_result.value()));
  std::vector<PowderReflection> scalar;
  for (const auto &f : bound.value().values()) {
    auto value =
        powder_reflection(f.identity, f.reflection, f.factor.fc_squared, o);
    check("finite_raw", value && std::isfinite(value.value().raw_intensity));
    if (value)
      scalar.push_back(value.value());
    auto zero = powder_reflection(f.identity, f.reflection, 0, o);
    check("zero_fc2_zero_raw", zero && zero.value().raw_intensity == 0);
    auto doubled = f.reflection;
    doubled.multiplicity *= 2;
    auto value2 =
        powder_reflection(f.identity, doubled, f.factor.fc_squared, o);
    check("multiplicity_scaling",
          value2 && close(value2.value().raw_intensity,
                          2 * value.value().raw_intensity));
    auto f2 = structure_factor(structure, f.reflection, o.radiation);
    check("bound_fc2_identity",
          f2 && f.factor.fc_squared == f2.value().fc_squared);
  }
  auto merged = merge_powder_reflections(scalar, o.overlap).value();
  double before = 0, after = 0, maximum = 0;
  for (const auto &r : scalar)
    before += r.raw_intensity;
  std::set<ReflectionIdentity> seen;
  double angle = -1;
  for (const auto &l : result.value().lines()) {
    after += l.raw_intensity;
    maximum = std::max(maximum, l.relative_intensity);
    check("ordered_finite_lines",
          std::isfinite(l.representative.d.value) &&
              l.representative.d.value > 0 &&
              l.representative.two_theta.value >= angle &&
              l.representative.two_theta.value < 180 &&
              std::isfinite(l.raw_intensity) && l.raw_intensity >= 0 &&
              std::isfinite(l.relative_intensity));
    angle = l.representative.two_theta.value;
    for (const auto &r : l.members) {
      check("unique_membership", seen.insert(r.identity).second);
      check(
          "absence_filtered_upstream",
          !systematically_absent(r.reflection.hkl, structure.group()).value());
    }
    if (l.members.size() == 1)
      check("isolated_unchanged",
            l.raw_intensity == l.members[0].raw_intensity);
  }
  check("all_sources_present_once", seen.size() == refs.reflections().size());
  check("merge_conserves_sum", close(before, after));
  check("normalization_maximum_1000", maximum == 1000);
  std::reverse(scalar.begin(), scalar.end());
  auto shuffled = normalize_powder_lines(
      merge_powder_reflections(scalar, o.overlap).value());
  check("stable_identity_permutation",
        shuffled && same(result.value(), shuffled.value()));
  for (double scale : {1e-20, .125, 7.5, 1e20}) {
    std::vector<PowderReflection> scaled;
    for (const auto &r : scalar)
      scaled.push_back(
          powder_reflection(r.identity, r.reflection, scale * r.fc_squared, o)
              .value());
    auto normalized = normalize_powder_lines(
        merge_powder_reflections(scaled, o.overlap).value());
    check("global_fc2_scale_invariance",
          normalized && same(result.value(), normalized.value(), false));
  }
  auto wrong_options = o;
  wrong_options.radiation.wavelength =
      WavelengthAngstrom{o.radiation.wavelength.value * 1.01};
  reject(powder_lines(refs, bound.value(), wrong_options), "powder.radiation",
         "incompatible_powder_radiation");
  wrong_options = o;
  wrong_options.radiation.kind = o.radiation.kind == RadiationKind::neutron
                                     ? RadiationKind::xray
                                     : RadiationKind::neutron;
  wrong_options.radiation.anomalous = false;
  reject(powder_lines(refs, bound.value(), wrong_options), "powder.radiation",
         "incompatible_powder_radiation");
  gen.maximum_two_theta.value *= .8;
  auto wrong_refs =
      generate_reflections(structure.cell(), structure.group(), gen);
  if (wrong_refs &&
      wrong_refs.value().reflections().size() != refs.reflections().size())
    reject(powder_lines(wrong_refs.value(), bound.value(), o),
           "powder.reflections", "incompatible_reflection_set");
}
void invalids() {
  auto o = options();
  auto ref = reflection(30);
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();
  for (double bad : {-1., nan, inf})
    reject(powder_reflection(1, ref, bad, o), "powder.reflection.fc_squared",
           "invalid_fc_squared");
  auto bad_ref = ref;
  bad_ref.multiplicity = 0;
  reject(powder_reflection(1, bad_ref, 1, o), "powder.reflection.multiplicity",
         "invalid_multiplicity");
  bad_ref = ref;
  bad_ref.hkl = {0, 0, 0};
  reject(powder_reflection(1, bad_ref, 1, o), "powder.reflection.hkl",
         "zero_hkl");
  bad_ref = ref;
  bad_ref.d.value = nan;
  reject(powder_reflection(1, bad_ref, 1, o), "powder.reflection.d",
         "invalid_d_spacing");
  for (double angle : {0., 180.})
    reject(powder_correction(TwoThetaDegrees{angle}, o),
           "powder.reflection.two_theta", "lp_singularity");
  for (double angle : {-1., 181., nan, inf})
    reject(powder_correction(TwoThetaDegrees{angle}, o),
           "powder.reflection.two_theta", "invalid_two_theta");
  reject(powder_correction(TwoThetaDegrees{1e-200}, o), "powder.correction",
         "nonfinite_correction", ErrorCode::numerical_failure);
  for (double wave : {0., -1., nan, inf}) {
    auto bad = o;
    bad.radiation.wavelength.value = wave;
    reject(powder_correction(TwoThetaDegrees{30}, bad), "radiation.wavelength",
           "invalid_wavelength");
  }
  auto bad = o;
  bad.radiation.kind = static_cast<RadiationKind>(99);
  reject(powder_correction(TwoThetaDegrees{30}, bad), "radiation.kind",
         "unsupported_radiation", ErrorCode::unsupported);
  reject(powder_reflection(1, ref, std::numeric_limits<double>::max(), o),
         "powder.reflection.raw_intensity", "nonfinite_raw_intensity",
         ErrorCode::numerical_failure);
  for (double tolerance : {-1., nan, inf})
    reject(merge_powder_reflections({}, {tolerance}),
           "powder.overlap.tolerance", "invalid_overlap_configuration");
  reject(merge_powder_reflections({}), "powder.reflections",
         "empty_reflection_set");
  reject(normalize_powder_lines({}), "powder.lines", "empty_line_set");
  auto r = powder_reflection(1, ref, 1, o).value();
  std::vector<PowderReflection> values{r, r};
  reject(merge_powder_reflections(values), "powder.reflection.identity",
         "duplicate_reflection_identity");
  values.resize(1);
  values[0].raw_intensity = nan;
  reject(merge_powder_reflections(values), "powder.reflection.raw_intensity",
         "invalid_raw_intensity");
  values[0] = r;
  values[0].correction.combined = inf;
  reject(merge_powder_reflections(values), "powder.correction",
         "invalid_correction");
  auto huge = powder_reflection(1, reflection(90, 1), 1.2e308, o).value();
  auto huge2 = huge;
  huge2.identity = 2;
  huge2.reflection.hkl.h = 2;
  values = {huge, huge2, huge};
  values[2].identity = 3;
  values[2].reflection.hkl.h = 3;
  reject(merge_powder_reflections(values), "powder.line.raw_intensity",
         "nonfinite_merged_result", ErrorCode::numerical_failure);
  values = {powder_reflection(1, ref, 0, o).value()};
  reject(normalize_powder_lines(merge_powder_reflections(values).value()),
         "powder.lines", "all_zero_normalization");
  auto lines =
      merge_powder_reflections(std::vector<PowderReflection>{r}).value();
  lines[0].raw_intensity = -1;
  reject(normalize_powder_lines(lines), "powder.line.raw_intensity",
         "invalid_raw_intensity");
  lines[0].raw_intensity = r.raw_intensity;
  lines[0].members.clear();
  reject(normalize_powder_lines(lines), "powder.line.members",
         "invalid_line_membership");
  // Exact binary boundary, including transitivity beyond the total tolerance.
  values.clear();
  for (int i = 0; i < 3; ++i)
    values.push_back(
        powder_reflection(i + 1, reflection(20 + i * .0625, i + 1, i + 1), 4, o)
            .value());
  auto grouped = merge_powder_reflections(values, {.0625});
  check("transitive_inclusive_boundary",
        grouped && grouped.value().size() == 1 &&
            grouped.value()[0].members.size() == 3);
  check("representative_not_centroid_or_summed_mult",
        grouped.value()[0].representative.two_theta.value == 20 &&
            grouped.value()[0].representative.multiplicity == 1);
  auto split = merge_powder_reflections(values, {std::nextafter(.0625, 0.)});
  check("exclusive_nextafter_boundary", split && split.value().size() == 3);
  auto correction = powder_correction(TwoThetaDegrees{90}, o).value();
  check("analytic_ninety_degrees", close(correction.combined, std::sqrt(2.)) &&
                                       close(correction.polarization, 1.));
  auto neutron = o;
  neutron.radiation.kind = RadiationKind::neutron;
  auto sync = o;
  sync.synchrotron = true;
  check("neutron_sync_same_lorentz",
        powder_correction(TwoThetaDegrees{30}, neutron).value().combined ==
            powder_correction(TwoThetaDegrees{30}, sync).value().combined);
  auto electron = o;
  electron.radiation.kind = RadiationKind::electron;
  check("legacy_electron_xray_branch",
        powder_correction(TwoThetaDegrees{30}, electron).value().combined ==
            powder_correction(TwoThetaDegrees{30}, o).value().combined);
}
} // namespace
int main(int argc, char **argv) {
  if (argc != 4)
    return 2;
  invalids();
  // Independent rational/high precision expected values of formed-angle LP.
  // This exercises the public caller, including both radiation branches.
  std::ifstream reference(argv[3]);
  check("lp.exact_reference_open", bool(reference));
  std::uint64_t angle_bits, lorentz_bits, polarization_bits, lp_bits;
  int unpolarized;
  std::size_t reference_cases = 0;
  while (reference >> std::hex >> angle_bits >> std::dec >> unpolarized >>
         std::hex >> lorentz_bits >> polarization_bits >> lp_bits) {
    auto o = options();
    o.synchrotron = unpolarized != 0;
    const auto correction = powder_correction(
        TwoThetaDegrees{std::bit_cast<double>(angle_bits)}, o);
    check("lp.exact_success", bool(correction));
    if (correction) {
      check("lp.exact_lorentz", std::bit_cast<std::uint64_t>(correction.value().lorentz) == lorentz_bits);
      check("lp.exact_polarization", std::bit_cast<std::uint64_t>(correction.value().polarization) == polarization_bits);
      check("lp.exact_combined", std::bit_cast<std::uint64_t>(correction.value().combined) == lp_bits);
    }
    ++reference_cases;
  }
  check("lp.exact_reference_complete", reference.eof() && reference_cases == 34);
  for (double tiny : {std::numeric_limits<double>::denorm_min(), 1e-200})
    reject(powder_correction(TwoThetaDegrees{tiny}, options()),
           "powder.correction", "nonfinite_correction", ErrorCode::numerical_failure);
  std::fenv_t saved_environment{};
  const int saved_rounding = std::fegetround();
  check("lp.fenv_save", std::fegetenv(&saved_environment) == 0);
  for (int rounding : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
    check("lp.fenv_set", std::fesetround(rounding) == 0);
    for (double angle : {std::nextafter(180., 0.), 90., .01}) {
      const auto correction = powder_correction(TwoThetaDegrees{angle}, options());
      check("lp.formed_argument_domain", correction &&
          std::isfinite(correction.value().combined) && correction.value().combined > 0);
    }
    reject(powder_correction(TwoThetaDegrees{std::numeric_limits<double>::denorm_min()}, options()),
           "powder.correction", "nonfinite_correction", ErrorCode::numerical_failure);
  }
  check("lp.fenv_restore", std::fesetenv(&saved_environment) == 0 &&
      std::fesetround(saved_rounding) == 0 && std::fegetround() == saved_rounding);
  std::ifstream input(argv[2]);
  int id, group_number, setting, rad, sync, anomalous, na;
  while (input >> id >> group_number >> setting >> rad >> sync >> anomalous >>
         na) {
    double wave, low, high;
    UnitCellParameters cell_parameters{};
    input >> wave >> low >> high;
    for (auto &v : cell_parameters.lengths_angstrom)
      input >> v;
    for (auto &v : cell_parameters.angles_degrees)
      input >> v;
    const auto cell = UnitCell::create(cell_parameters).value();
    const auto group =
        SpaceGroupRegistry{}.by_number(group_number, setting).value();
    std::vector<AtomicSite> sites;
    for (int i = 0; i < na; ++i) {
      std::string label;
      std::array<double, 3> xyz;
      double occupancy;
      int adp;
      DisplacementTensor tensor;
      input >> label;
      for (auto &v : xyz)
        input >> v;
      input >> occupancy >> adp;
      for (auto &v : tensor)
        input >> v;
      auto displacement = adp == 0   ? Displacement::isotropic_b(tensor[0])
                          : adp == 1 ? Displacement::isotropic_u(tensor[0])
                          : adp == 2 ? Displacement::anisotropic_u(tensor)
                                     : Displacement::anisotropic_beta(tensor);
      sites.push_back({ScatteringRegistry{}.find(label).value(),
                       FractionalPosition::create(xyz).value(),
                       Occupancy::create(occupancy).value(),
                       displacement.value()});
    }
    auto structure = CrystalStructure::create(cell, group, sites);
    check("structure_from_frozen_inputs", bool(structure));
    if (!structure)
      continue;
    PowderIntensityOptions o;
    o.radiation = {static_cast<RadiationKind>(rad), WavelengthAngstrom{wave},
                   anomalous != 0};
    o.synchrotron = sync != 0;
    ReflectionGenerationOptions gen;
    gen.wavelength = WavelengthAngstrom{wave};
    gen.minimum_d = d_spacing(TwoThetaDegrees{high}, gen.wavelength).value();
    gen.minimum_two_theta = TwoThetaDegrees{low};
    gen.maximum_two_theta = TwoThetaDegrees{high};
    for (auto friedel : {FriedelPolicy::merge, FriedelPolicy::separate}) {
      gen.friedel = friedel;
      auto refs = generate_reflections(cell, group, gen);
      check("phase15_generation", bool(refs));
      if (refs)
        exercise(structure.value(), refs.value(), o, sites);
    }
  }
  check("complete_fixture_input",
        input.eof() && counts["structure_from_frozen_inputs"] == 54);
  // Explicit ownership rejection, including a same-HKL, different-cell set.
  auto g = SpaceGroupRegistry{}.by_number(1).value();
  auto cell = UnitCell::create({{5, 6, 7}, {76, 83, 67}}).value();
  AtomicSite atom{ScatteringRegistry{}.find("Si").value(),
                  FractionalPosition::create({0, 0, 0}).value(),
                  Occupancy::create(1).value(),
                  Displacement::isotropic_b(0).value()};
  auto structure = CrystalStructure::create(cell, g, {atom}).value();
  ReflectionGenerationOptions gen;
  gen.wavelength = ScatteringRadiation{}.wavelength;
  gen.minimum_d = DSpacingAngstrom{2};
  auto refs = generate_reflections(cell, g, gen).value();
  auto bound = PowderStructureFactors::calculate(structure, refs, {}).value();
  auto changed = UnitCell::create({{5.00001, 6, 7}, {76, 83, 67}}).value();
  auto changed_refs = generate_reflections(changed, g, gen).value();
  reject(powder_lines(changed_refs, bound, {}), "powder.reflections",
         "incompatible_reflection_set");
  reject(PowderStructureFactors::calculate(structure, changed_refs, {}),
         "reflection.d", "reflection_cell_mismatch");
  gen.minimum_d = DSpacingAngstrom{100};
  auto empty = generate_reflections(cell, g, gen).value();
  reject(PowderStructureFactors::calculate(structure, empty, {}),
         "powder.reflections", "empty_reflection_set");
  auto radiation = ScatteringRadiation{};
  radiation.wavelength.value = 2;
  reject(PowderStructureFactors::calculate(structure, refs, radiation),
         "reflection_set.wavelength", "reflection_radiation_mismatch");
  auto wrong_group = SpaceGroupRegistry{}.by_number(2).value();
  auto different = CrystalStructure::create(cell, wrong_group, {atom}).value();
  gen.minimum_d = DSpacingAngstrom{2};
  gen.friedel = FriedelPolicy::separate;
  auto singles = generate_reflections(cell, g, gen).value();
  reject(PowderStructureFactors::calculate(different, singles, {}),
         "reflection_set.group", "reflection_group_mismatch");
  std::ofstream out(argv[1]);
  out << "{\"success\":" << (failures.empty() ? "true" : "false")
      << ",\"counts\":{";
  bool first = true;
  for (const auto &[name, count] : counts) {
    if (!first)
      out << ',';
    first = false;
    out << '"' << name << "\":" << count;
  }
  out << "},\"failures\":[";
  for (std::size_t i = 0; i < failures.size(); ++i) {
    if (i)
      out << ',';
    out << '"' << failures[i] << '"';
  }
  out << "],\"errors\":[";
  for (std::size_t i = 0; i < errors.size(); ++i) {
    if (i)
      out << ',';
    out << "{\"reason\":\"" << errors[i].reason << "\",\"field\":\""
        << errors[i].field
        << "\",\"success\":" << (errors[i].success ? "true" : "false") << '}';
  }
  out << "]}\n";
  std::size_t total = 0;
  for (const auto &[name, count] : counts)
    total += count;
  std::cout << "Powder property checks: " << total
            << "; failures: " << failures.size() << '\n';
  return failures.empty() ? 0 : 1;
}
