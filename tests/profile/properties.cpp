#include "qualpf/profile/powder_profile.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
using namespace qualpf::profile;
static std::size_t checks = 0, errors = 0;
void require(bool p, const char *s) {
  ++checks;
  if (!p)
    throw std::runtime_error(s);
}
void near(double a, double b, double t, const char *s) {
  require(std::abs(a - b) <= t, s);
}
template <class T> T take(qualpf::Result<T> r) {
  if (!r)
    throw std::runtime_error(r.error().field + "/" + r.error().reason);
  return std::move(r).value();
}
template <class T> void fails(qualpf::Result<T> r, const char *reason) {
  require(!r, "expected error");
  require(r.error().reason == reason, "error reason");
  ++errors;
}
int main() {
  try {
    constexpr double pi = std::numbers::pi;
    ProfileRadiation rad{{{1.54059307, 1}}};
    auto grid = take(ProfileGrid::regular({20, 40, .01}));
    ProfileReflection reflection{1, 10, 2, 1, 20, 1, {{30, 1, 1}}};
    for (auto model :
         {PeakModel::pearson_vii, PeakModel::pseudo_voigt, PeakModel::tchz}) {
      ProfileParameters p;
      p.model = model;
      p.width.w = .25;
      for (double center : {1., 30., 90., 170.}) {
        auto s = take(peak_shape(center, p));
        double maximum = take(symmetric_kernel(s, 0));
        near(take(symmetric_kernel(s, s.fwhm_degrees / 2)), maximum / 2,
             maximum * 2e-14, "FWHM");
        for (int i = 0; i < 1000; ++i) {
          double d = i * .001;
          double a = take(symmetric_kernel(s, d)),
                 b = take(symmetric_kernel(s, -d));
          require(std::isfinite(a) && a >= 0 && a <= maximum,
                  "finite nonnegative maximum");
          require(a == b, "symmetric");
          near(take(asymmetry_factor(s, d)), 1, 0, "default asymmetry");
        }
        // Simpson integration on [-50H,50H] against closed finite-area formula.
        const double h = s.fwhm_degrees, limit = 50 * h,
                     step = 2 * limit / 20000;
        double sum = 0;
        for (int i = 0; i <= 20000; ++i)
          sum += (i == 0 || i == 20000 ? 1 : (i % 2 ? 4 : 2)) *
                 take(symmetric_kernel(s, -limit + i * step));
        const double area = sum * step / 3;
        double expected;
        if (model == PeakModel::pearson_vii) { // beta=2 closed antiderivative
          double q = 2 * std::sqrt(std::sqrt(2.) - 1) / h, z = q * limit;
          expected = 2 / pi * (std::atan(z) + z / (1 + z * z));
        } else
          expected = s.beta_or_eta * 2 / pi * std::atan(2 * limit / h) +
                     (1 - s.beta_or_eta) *
                         std::erf(2 * std::sqrt(std::log(2.)) * limit / h);
        near(area, expected, 2e-10, "analytic finite area");
      }
      ProfilePhase phase{{reflection}, p, 1};
      auto raw = take(phase_profile(grid, phase, rad));
      require(raw == take(phase_profile(grid, phase, rad)),
              "deterministic repeat");
      auto two = phase;
      two.reflections[0].fc_squared *= 2;
      auto scaled = take(phase_profile(grid, two, rad));
      auto duplicate = phase;
      auto second = reflection;
      second.identity = 2;
      duplicate.reflections.push_back(second);
      auto doubled = take(phase_profile(grid, duplicate, rad));
      for (std::size_t i = 0; i < raw.size(); ++i) {
        near(scaled[i], 2 * raw[i], 1e-12, "intensity linearity");
        near(doubled[i], scaled[i], 1e-12, "reflection additivity");
      }
      std::array<ProfilePhase, 2> phases{phase, phase};
      phases[0].scale = .25;
      phases[1].scale = .75;
      auto combined = take(powder_profile(grid, rad, phases, {1000, 7}));
      for (std::size_t i = 0; i < raw.size(); ++i)
        near(combined.weighted_intensity[i], raw[i], 1e-12, "phase linearity");
      near(*std::max_element(combined.calculated_intensity.begin(),
                             combined.calculated_intensity.end()),
           1000, 1e-12, "display max");
      for (double v : combined.background)
        require(v == 7, "background separation");
      phase.reflections[0].fc_squared = 0;
      auto zero =
          take(powder_profile(grid, rad, std::span(&phase, 1), {1000, 7}));
      for (std::size_t i = 0; i < raw.size(); ++i) {
        require(zero.calculated_intensity[i] == 0, "zero intensity");
        require(zero.background[i] == 0, "zero max branch");
      }
      // Explicit second component is an independent additive contribution.
      phase.reflections[0] = reflection;
      phase.reflections[0].components.push_back({30, 1, 1});
      ProfileRadiation doublet{{{1.54, 1}, {1.55, .5}}};
      auto both = take(phase_profile(grid, phase, doublet));
      for (std::size_t i = 0; i < raw.size(); ++i)
        near(both[i], 1.5 * raw[i], 2e-12, "component additivity");
    }
    ProfileParameters pv;
    pv.model = PeakModel::pseudo_voigt;
    pv.width.w = .25;
    for (double eta : {0., 1.}) {
      pv.shape.eta0 = eta;
      auto s = take(peak_shape(30, pv));
      for (double d : {0., .1, 1., 10.}) {
        double expected = eta == 0
                              ? std::sqrt(4 * std::log(2.) / pi) / .5 *
                                    std::exp(-4 * std::log(2.) * d * d / .25)
                              : 2 / (pi * .5) / (1 + 4 * d * d / .25);
        near(take(symmetric_kernel(s, d)), expected, 1e-14, "PV endpoint");
      }
    }
    ProfileParameters tch;
    tch.model = PeakModel::tchz;
    tch.width.w = .25;
    tch.width.x = 0;
    auto t = take(peak_shape(30, tch));
    near(t.fwhm_degrees, .5, 1e-15, "TCH Gaussian width");
    near(t.beta_or_eta, 0, 0, "TCH Gaussian eta");
    tch.width.w = 0;
    tch.width.x = 0;
    t = take(peak_shape(30, tch));
    require(t.gaussian_width_floored && t.combined_width_floored,
            "TCH two floors");
    near(t.fwhm_degrees, std::pow(1e-7, .2), 1e-15, "TCH floor width");
    auto shifted = take(ProfileGrid::coordinates({29.9, 30., 30.1}));
    auto con = take(reflection_contribution(shifted, reflection, 0, rad, {}));
    require(con.first_index == 0 && con.last_index == 2, "inclusive support");
    auto outside = reflection;
    outside.components[0].center_degrees = 5;
    con = take(reflection_contribution(shifted, outside, 0, rad, {}));
    require(con.first_index == 0 && con.last_index == 0,
            "exterior clamped endpoint");
    outside = reflection;
    outside.support_intensity_ratio = 0;
    outside.components[0].center_degrees = 30.5;
    auto tiegrid = take(ProfileGrid::coordinates({30., 31.}));
    con = take(reflection_contribution(tiegrid, outside, 0, rad, {}));
    require(con.first_index == 1, "nearest tie upper");
    near(take(density_phase_scale(.2, 4, 10)), .0005, 1e-18, "density weight");
    // Binary64 nearest support semantics around a half-step, with one-ULP
    // probes.
    for (double center :
         {std::nextafter(30.5, 30.), 30.5, std::nextafter(30.5, 31.)}) {
      outside.components[0].center_degrees = center;
      con = take(reflection_contribution(tiegrid, outside, 0, rad, {}));
      require(con.first_index == (center < 30.5 ? 0u : 1u),
              "half-step ULP boundary");
    }
    // Translation invariance applies to constant, symmetric widths/shapes only.
    auto translated = take(ProfileGrid::coordinates({39.9, 40., 40.1}));
    auto moved = reflection;
    moved.components[0].center_degrees += 10;
    auto first = take(reflection_contribution(shifted, reflection, 0, rad, {}));
    auto last = take(reflection_contribution(translated, moved, 0, rad, {}));
    for (std::size_t i = 0; i < first.intensity.size(); ++i)
      near(first.intensity[i], last.intensity[i], 1e-10, "grid translation");
    ProfileParameters asym;
    asym.shape.asymmetry = {.01, .002, .001, .003};
    auto as = take(peak_shape(30, asym));
    for (double d : {.001, .01, .1, 1.})
      near(take(asymmetry_factor(as, d)) + take(asymmetry_factor(as, -d)), 2,
           1e-14, "asymmetry odd correction");
    ProfileParameters pear;
    pear.shape.beta0 = .1;
    require(take(peak_shape(30, pear)).beta_clamped, "Pearson low clamp");
    pear.shape.beta0 = 30;
    require(take(peak_shape(30, pear)).beta_clamped, "Pearson high clamp");
    ProfileParameters narrow;
    narrow.width.w = 1e-10;
    auto ns = take(peak_shape(30, narrow));
    require(ns.gaussian_width_floored, "Pearson narrow floor");
    near(ns.fwhm_degrees, .01, 1e-16, "Pearson floor width");
    narrow.model = PeakModel::pseudo_voigt;
    ns = take(peak_shape(30, narrow));
    require(ns.gaussian_width_floored, "PV narrow floor");
    const double nan = std::numeric_limits<double>::quiet_NaN();
    fails(ProfileGrid::regular({0, 1, 0}), "invalid_grid");
    fails(ProfileGrid::regular({1, 0, 1}), "invalid_grid");
    fails(ProfileGrid::regular({0, 1, 1e-300}), "invalid_grid_size");
    fails(ProfileGrid::coordinates({}), "invalid_grid_size");
    fails(ProfileGrid::coordinates({1, 1}), "invalid_grid");
    fails(ProfileGrid::coordinates({2, 1}), "invalid_grid");
    fails(ProfileGrid::coordinates({nan}), "invalid_grid");
    fails(peak_shape(0, {}), "invalid_center");
    fails(peak_shape(180, {}), "invalid_center");
    ProfileParameters bad;
    bad.width.w = -1;
    fails(peak_shape(30, bad), "invalid_fwhm");
    bad = {};
    bad.model = static_cast<PeakModel>(99);
    fails(peak_shape(30, bad), "unsupported_model");
    bad = pv;
    bad.shape.eta0 = 2;
    fails(peak_shape(30, bad), "invalid_shape_parameter");
    auto br = rad;
    br.components[0].wavelength_angstrom = 0;
    fails(integrated_intensity(reflection, 0, br), "invalid_wavelength");
    br = rad;
    br.components[0].intensity_ratio = -1;
    fails(integrated_intensity(reflection, 0, br), "invalid_intensity_ratio");
    auto r = reflection;
    r.fc_squared = nan;
    fails(integrated_intensity(r, 0, rad), "invalid_intensity");
    r = reflection;
    r.multiplicity = 0;
    fails(integrated_intensity(r, 0, rad), "invalid_multiplicity");
    r = reflection;
    r.preferred_orientation = -1;
    fails(integrated_intensity(r, 0, rad), "invalid_orientation");
    r = reflection;
    r.components[0].absorption = nan;
    fails(integrated_intensity(r, 0, rad), "invalid_absorption");
    r = reflection;
    r.components[0].lp = -1;
    fails(integrated_intensity(r, 0, rad), "invalid_lp");
    r = reflection;
    r.support_widths = -1;
    fails(reflection_contribution(grid, r, 0, rad, {}), "invalid_support");
    fails(integrated_intensity(reflection, 1, rad), "component_mismatch");
    fails(density_phase_scale(-1, 1, 1), "invalid_phase_weight");
    fails(density_phase_scale(1, 0, 1), "invalid_density_volume");
    fails(density_phase_scale(1, 1e300, 1e300), "nonfinite_result");
    r = reflection;
    r.fc_squared = 1e308;
    fails(integrated_intensity(r, 0, rad), "nonfinite_result");
    br = {};
    fails(integrated_intensity(reflection, 0, br),
          "unsupported_component_count");
    ProfilePhase invalid{{reflection}, {}, -1};
    fails(powder_profile(grid, rad, std::span(&invalid, 1)),
          "invalid_phase_weight");
    invalid.scale = 1;
    invalid.reflections.push_back(reflection);
    fails(phase_profile(grid, invalid, rad), "duplicate_identity");
    invalid.reflections.clear();
    fails(phase_profile(grid, invalid, rad), "empty_reflections");
    std::cout << "PASS " << checks << " checks " << errors
              << " error contracts\n";
  } catch (const std::exception &e) {
    std::cerr << "FAIL after " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
