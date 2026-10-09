#include "qualpf/profile/powder_profile.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <set>

namespace qualpf::profile {
namespace {
constexpr double epsilon32 = 0x1p-23;
constexpr std::size_t max_points = 2'000'000;
constexpr double pi = std::numbers::pi;
Error error(const char *field, const char *reason) {
  return {ErrorCode::invalid_input, field, reason, reason};
}
bool nonnegative(double x) { return std::isfinite(x) && x >= 0; }
bool positive(double x) { return std::isfinite(x) && x > 0; }
bool angle(double x) { return positive(x) && x < 180; }
Status validate_radiation(const ProfileRadiation &radiation) {
  if (radiation.components.empty() || radiation.components.size() > 2)
    return Status(error("profile.radiation", "unsupported_component_count"));
  for (auto c : radiation.components) {
    if (!positive(c.wavelength_angstrom))
      return Status(error("profile.wavelength", "invalid_wavelength"));
    if (!nonnegative(c.intensity_ratio))
      return Status(error("profile.ratio", "invalid_intensity_ratio"));
  }
  return Status(std::monostate{});
}
Status validate_shape(const PeakShape &s) {
  if (!angle(s.center_degrees))
    return Status(error("profile.center", "invalid_center"));
  if (!positive(s.fwhm_degrees))
    return Status(error("profile.width", "invalid_fwhm"));
  if (s.model != PeakModel::pearson_vii && s.model != PeakModel::pseudo_voigt &&
      s.model != PeakModel::tchz)
    return Status(error("profile.model", "unsupported_model"));
  if (!std::isfinite(s.beta_or_eta) ||
      (s.model == PeakModel::pearson_vii
           ? (s.beta_or_eta < .51 || s.beta_or_eta > 20)
           : (s.beta_or_eta < 0 || s.beta_or_eta > 1)))
    return Status(error("profile.shape", "invalid_shape_parameter"));
  for (double v : s.asymmetry)
    if (!std::isfinite(v))
      return Status(error("profile.asymmetry", "invalid_shape_parameter"));
  return Status(std::monostate{});
}
std::size_t nearest(std::span<const double> x, double value) {
  auto upper = std::lower_bound(x.begin(), x.end(), value);
  if (upper == x.begin())
    return 0;
  if (upper == x.end())
    return x.size() - 1;
  const auto i = static_cast<std::size_t>(upper - x.begin());
  return value - x[i - 1] < x[i] - value ? i - 1 : i;
}
Result<double> checked(double x) {
  if (!std::isfinite(x))
    return Result<double>(error("profile.result", "nonfinite_result"));
  return Result<double>(x);
}
} // namespace

Result<ProfileGrid> ProfileGrid::coordinates(std::vector<double> points) {
  if (points.empty() || points.size() > max_points)
    return Result<ProfileGrid>(error("profile.grid", "invalid_grid_size"));
  for (std::size_t i = 0; i < points.size(); ++i)
    if (!std::isfinite(points[i]) || (i && points[i] <= points[i - 1]))
      return Result<ProfileGrid>(error("profile.grid", "invalid_grid"));
  return Result<ProfileGrid>(ProfileGrid(std::move(points)));
}
Result<ProfileGrid> ProfileGrid::regular(ProfileRange r) {
  if (!std::isfinite(r.start) || !std::isfinite(r.end) || r.end < r.start ||
      !positive(r.step))
    return Result<ProfileGrid>(error("profile.grid", "invalid_grid"));
  const double count = std::floor((r.end - r.start) / r.step + 1);
  if (!positive(count) || count > max_points)
    return Result<ProfileGrid>(error("profile.grid", "invalid_grid_size"));
  std::vector<double> x(static_cast<std::size_t>(count));
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = r.start + static_cast<double>(i) * r.step;
  return coordinates(std::move(x));
}

Result<PeakShape> peak_shape(double center, const ProfileParameters &p) {
  if (!angle(center))
    return Result<PeakShape>(error("profile.center", "invalid_center"));
  if (p.model != PeakModel::pearson_vii && p.model != PeakModel::pseudo_voigt &&
      p.model != PeakModel::tchz)
    return Result<PeakShape>(error("profile.model", "unsupported_model"));
  const auto &w = p.width;
  const auto &s = p.shape;
  for (double v : {w.u, w.v, w.w, w.z, w.x, w.y, s.beta0, s.beta1, s.beta2,
                   s.eta0, s.eta1, s.eta2})
    if (!std::isfinite(v))
      return Result<PeakShape>(
          error("profile.parameters", "nonfinite_parameter"));
  const double theta = center * pi / 360, t = std::tan(theta),
               c = std::cos(theta);
  PeakShape result{p.model, center, 0, 0, s.asymmetry};
  double q = w.u * t * t + w.v * t + w.w;
  if (p.model == PeakModel::tchz)
    q += w.z / (c * c);
  if (!nonnegative(q))
    return Result<PeakShape>(error("profile.width", "invalid_fwhm"));
  if (p.model == PeakModel::tchz) {
    if (q < epsilon32) {
      q = 1e-7;
      result.gaussian_width_floored = true;
    }
    const double g = std::sqrt(q), l = w.x * t + w.y / c;
    if (!nonnegative(l))
      return Result<PeakShape>(error("profile.width", "invalid_fwhm"));
    double poly = std::pow(g, 5) + 2.69269 * std::pow(g, 4) * l +
                  2.42843 * std::pow(g, 3) * l * l +
                  4.47163 * g * g * std::pow(l, 3) +
                  .07842 * g * std::pow(l, 4) + std::pow(l, 5);
    if (poly < epsilon32) {
      poly = 1e-7;
      result.combined_width_floored = true;
    }
    result.fwhm_degrees = std::pow(poly, .2);
    const double r = l / result.fwhm_degrees;
    result.beta_or_eta = 1.36603 * r - .47719 * r * r + .11116 * r * r * r;
  } else {
    const double threshold =
        p.model == PeakModel::pearson_vii ? epsilon32 : .0001;
    if (q < threshold) {
      q = .0001;
      result.gaussian_width_floored = true;
    }
    result.fwhm_degrees = std::sqrt(q);
    if (p.model == PeakModel::pearson_vii) {
      const double beta =
          s.beta0 + s.beta1 / center + s.beta2 / (center * center);
      if (!std::isfinite(beta))
        return Result<PeakShape>(
            error("profile.shape", "invalid_shape_parameter"));
      result.beta_or_eta = std::clamp(beta, .51, 20.);
      result.beta_clamped = beta != result.beta_or_eta;
    } else
      result.beta_or_eta = s.eta0 + s.eta1 * t + s.eta2 * t * t;
  }
  auto valid = validate_shape(result);
  if (!valid)
    return Result<PeakShape>(valid.error());
  return Result<PeakShape>(result);
}
Result<double> symmetric_kernel(const PeakShape &s, double delta) {
  auto valid = validate_shape(s);
  if (!valid)
    return Result<double>(valid.error());
  if (!std::isfinite(delta))
    return Result<double>(error("profile.delta", "nonfinite_parameter"));
  const double h = s.fwhm_degrees, z = delta / h, a = s.beta_or_eta;
  if (!std::isfinite(z * z))
    return Result<double>(error("profile.result", "nonfinite_result"));
  if (s.model == PeakModel::pearson_vii) {
    const double p = std::pow(2., 1 / a) - 1;
    return checked(std::tgamma(a) / std::tgamma(a - .5) * 2 *
                   std::sqrt(p / pi) / h * std::pow(1 + 4 * p * z * z, -a));
  }
  const double lorentz = 2 / (pi * h) / (1 + 4 * z * z);
  const double gaussian = std::sqrt(4 * std::log(2.) / pi) / h *
                          std::exp(-4 * std::log(2.) * z * z);
  return checked(a * lorentz + (1 - a) * gaussian);
}
Result<double> asymmetry_factor(const PeakShape &s, double delta) {
  auto valid = validate_shape(s);
  if (!valid)
    return Result<double>(valid.error());
  if (!std::isfinite(delta))
    return Result<double>(error("profile.delta", "nonfinite_parameter"));
  if (std::all_of(s.asymmetry.begin(), s.asymmetry.end(),
                  [](double x) { return x == 0; }))
    return Result<double>(1.);
  const double theta = s.center_degrees * pi / 360, z = delta / s.fwhm_degrees,
               z2 = z * z;
  if (!std::isfinite(z2))
    return Result<double>(error("profile.result", "nonfinite_result"));
  const auto &p = s.asymmetry;
  const double a = p[0] / std::tan(theta) + p[1] / std::tan(2 * theta);
  const double b = p[2] / std::tan(theta) + p[3] / std::tan(2 * theta);
  return checked(1 + z * std::exp(-z2) * (2 * a + b * (8 * z2 - 12)));
}
Result<double> integrated_intensity(const ProfileReflection &r, std::size_t k,
                                    const ProfileRadiation &rad) {
  auto valid = validate_radiation(rad);
  if (!valid)
    return Result<double>(valid.error());
  if (r.components.size() != rad.components.size() || k >= r.components.size())
    return Result<double>(error("profile.components", "component_mismatch"));
  if (!nonnegative(r.fc_squared))
    return Result<double>(error("profile.fc_squared", "invalid_intensity"));
  if (!r.multiplicity)
    return Result<double>(
        error("profile.multiplicity", "invalid_multiplicity"));
  if (!nonnegative(r.preferred_orientation))
    return Result<double>(error("profile.orientation", "invalid_orientation"));
  const auto &c = r.components[k];
  if (!angle(c.center_degrees))
    return Result<double>(error("profile.center", "invalid_center"));
  if (!nonnegative(c.lp))
    return Result<double>(error("profile.lp", "invalid_lp"));
  if (!nonnegative(c.absorption))
    return Result<double>(error("profile.absorption", "invalid_absorption"));
  return checked(static_cast<double>(r.multiplicity) * r.fc_squared *
                 r.preferred_orientation * c.lp * c.absorption *
                 rad.components[k].intensity_ratio);
}
Result<ProfileContribution>
reflection_contribution(const ProfileGrid &grid, const ProfileReflection &r,
                        std::size_t k, const ProfileRadiation &rad,
                        const ProfileParameters &parameters) {
  auto intensity = integrated_intensity(r, k, rad);
  if (!intensity)
    return Result<ProfileContribution>(intensity.error());
  if (!positive(r.support_widths) || !nonnegative(r.support_intensity_ratio))
    return Result<ProfileContribution>(
        error("profile.support", "invalid_support"));
  auto shape = peak_shape(r.components[k].center_degrees, parameters);
  if (!shape)
    return Result<ProfileContribution>(shape.error());
  const double support =
      shape.value().fwhm_degrees * r.support_widths * r.support_intensity_ratio;
  const double center = shape.value().center_degrees;
  if (!std::isfinite(support) || !std::isfinite(center + support) ||
      !std::isfinite(center - support))
    return Result<ProfileContribution>(
        error("profile.support", "nonfinite_result"));
  auto x = grid.points();
  const auto first = nearest(x, center - support),
             last = nearest(x, center + support);
  ProfileContribution result{shape.value(), intensity.value(), first, last, {}};
  result.intensity.reserve(last - first + 1);
  for (std::size_t i = first; i <= last; ++i) {
    auto kernel = symmetric_kernel(shape.value(), x[i] - center);
    if (!kernel)
      return Result<ProfileContribution>(kernel.error());
    auto asym = asymmetry_factor(shape.value(), x[i] - center);
    if (!asym)
      return Result<ProfileContribution>(asym.error());
    auto value = checked(intensity.value() * kernel.value() * asym.value());
    if (!value)
      return Result<ProfileContribution>(value.error());
    result.intensity.push_back(value.value());
  }
  return Result<ProfileContribution>(std::move(result));
}
Result<std::vector<double>> phase_profile(const ProfileGrid &grid,
                                          const ProfilePhase &phase,
                                          const ProfileRadiation &rad) {
  auto valid = validate_radiation(rad);
  if (!valid)
    return Result<std::vector<double>>(valid.error());
  if (phase.reflections.empty())
    return Result<std::vector<double>>(
        error("profile.reflections", "empty_reflections"));
  std::set<std::size_t> identities;
  for (const auto &r : phase.reflections)
    if (!identities.insert(r.identity).second)
      return Result<std::vector<double>>(
          error("profile.identity", "duplicate_identity"));
  std::vector<double> y(grid.points().size(), 0.);
  for (std::size_t k = 0; k < rad.components.size(); ++k)
    for (const auto &r : phase.reflections) {
      auto contribution =
          reflection_contribution(grid, r, k, rad, phase.parameters);
      if (!contribution)
        return Result<std::vector<double>>(contribution.error());
      const auto &c = contribution.value();
      for (std::size_t j = 0; j < c.intensity.size(); ++j) {
        double &v = y[c.first_index + j];
        v += c.intensity[j];
        if (!std::isfinite(v))
          return Result<std::vector<double>>(
              error("profile.result", "nonfinite_result"));
      }
    }
  return Result<std::vector<double>>(std::move(y));
}
Result<double> density_phase_scale(double weight, double density,
                                   double volume) {
  if (!nonnegative(weight))
    return Result<double>(error("profile.weight", "invalid_phase_weight"));
  if (!positive(density) || !positive(volume))
    return Result<double>(error("profile.density", "invalid_density_volume"));
  double denominator = density * volume * volume;
  if (!positive(denominator))
    return Result<double>(error("profile.density", "nonfinite_result"));
  return checked(weight / denominator);
}
Result<PowderProfile> postprocess(const ProfileGrid &grid,
                                  std::span<const double> weighted,
                                  ProfileDisplay display) {
  if (weighted.size() != grid.points().size())
    return Result<PowderProfile>(error("profile.grid", "size_mismatch"));
  if (!nonnegative(display.scale) || !std::isfinite(display.background))
    return Result<PowderProfile>(error("profile.display", "invalid_display"));
  for (double v : weighted)
    if (!std::isfinite(v))
      return Result<PowderProfile>(error("profile.result", "nonfinite_result"));
  PowderProfile result{{grid.points().begin(), grid.points().end()},
                       {weighted.begin(), weighted.end()},
                       std::vector<double>(weighted.size(), 0.),
                       {weighted.begin(), weighted.end()}};
  const double maximum = *std::max_element(weighted.begin(), weighted.end());
  if (std::abs(maximum) > epsilon32) {
    const double scale = display.scale > display.background
                             ? display.scale - display.background
                             : display.scale;
    for (std::size_t i = 0; i < weighted.size(); ++i) {
      const double y = scale * weighted[i] / maximum + display.background;
      if (!std::isfinite(y))
        return Result<PowderProfile>(
            error("profile.result", "nonfinite_result"));
      result.calculated_intensity[i] = y;
      result.background[i] = display.background;
    }
  }
  return Result<PowderProfile>(std::move(result));
}
Result<PowderProfile> powder_profile(const ProfileGrid &grid,
                                     const ProfileRadiation &rad,
                                     std::span<const ProfilePhase> phases,
                                     ProfileDisplay display) {
  auto valid = validate_radiation(rad);
  if (!valid)
    return Result<PowderProfile>(valid.error());
  if (phases.empty())
    return Result<PowderProfile>(error("profile.phases", "empty_phases"));
  std::vector<double> sum(grid.points().size(), 0.);
  for (const auto &phase : phases) {
    if (!nonnegative(phase.scale))
      return Result<PowderProfile>(
          error("profile.scale", "invalid_phase_weight"));
    auto y = phase_profile(grid, phase, rad);
    if (!y)
      return Result<PowderProfile>(y.error());
    for (std::size_t i = 0; i < sum.size(); ++i) {
      sum[i] += phase.scale * y.value()[i];
      if (!std::isfinite(sum[i]))
        return Result<PowderProfile>(
            error("profile.result", "nonfinite_result"));
    }
  }
  return postprocess(grid, sum, display);
}
} // namespace qualpf::profile
