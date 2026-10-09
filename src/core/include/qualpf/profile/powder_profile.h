#pragma once

#include "qualpf/domain/result.h"
#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace qualpf::profile {

struct ProfileRange {
  double start;
  double end;
  double step;
};

class ProfileGrid {
public:
  static Result<ProfileGrid> regular(ProfileRange range);
  static Result<ProfileGrid> coordinates(std::vector<double> points);
  std::span<const double> points() const noexcept { return points_; }

private:
  explicit ProfileGrid(std::vector<double> points)
      : points_(std::move(points)) {}
  std::vector<double> points_;
};

struct RadiationComponent {
  double wavelength_angstrom;
  double intensity_ratio;
};
struct ProfileRadiation {
  std::vector<RadiationComponent> components;
};
enum class PeakModel { pearson_vii, pseudo_voigt, tchz };
struct PeakWidthParameters {
  double u = 0, v = 0, w = .01, z = 0, x = .01, y = 0;
};
struct PeakShapeParameters {
  double beta0 = 2, beta1 = 0, beta2 = 0;
  double eta0 = .5, eta1 = 0, eta2 = 0;
  std::array<double, 4> asymmetry{}; // Berar-Baldinozzi a0,a1,b0,b1
};
struct ProfileParameters {
  PeakModel model = PeakModel::pearson_vii;
  PeakWidthParameters width;
  PeakShapeParameters shape;
};
struct PeakShape {
  PeakModel model;
  double center_degrees;
  double fwhm_degrees;
  double beta_or_eta;
  std::array<double, 4> asymmetry;
  bool gaussian_width_floored = false;
  bool combined_width_floored = false;
  bool beta_clamped = false;
};
struct ReflectionComponent {
  double center_degrees;
  double lp = 1;
  double absorption = 1;
};
// Explicit scalars do not claim ownership by a CrystalStructure. The checked
// structure adapter is a separate entry; there is no grouped card-line input.
struct ProfileReflection {
  std::size_t identity;
  double fc_squared;
  std::size_t multiplicity;
  double preferred_orientation = 1;
  double support_widths = 20;
  double support_intensity_ratio = 1;
  std::vector<ReflectionComponent> components;
};
struct ProfileContribution {
  PeakShape shape;
  double integrated_intensity;
  std::size_t first_index;
  std::size_t last_index;
  std::vector<double> intensity; // only the inclusive support interval
};
struct ProfilePhase {
  std::vector<ProfileReflection> reflections; // stable caller order
  ProfileParameters parameters;
  double scale = 1;
};
struct ProfileDisplay {
  double scale = 1000;
  double background = 0;
};
struct PowderProfile {
  std::vector<double> two_theta;
  std::vector<double> weighted_intensity; // before display normalization
  std::vector<double> background; // actually applied, including zero-max branch
  std::vector<double> calculated_intensity;
};

Result<PeakShape> peak_shape(double center_degrees,
                             const ProfileParameters &parameters);
Result<double> symmetric_kernel(const PeakShape &shape, double delta_degrees);
Result<double> asymmetry_factor(const PeakShape &shape, double delta_degrees);
Result<double> integrated_intensity(const ProfileReflection &reflection,
                                    std::size_t component,
                                    const ProfileRadiation &radiation);
Result<ProfileContribution> reflection_contribution(
    const ProfileGrid &grid, const ProfileReflection &reflection,
    std::size_t component, const ProfileRadiation &radiation,
    const ProfileParameters &parameters);
Result<std::vector<double>> phase_profile(const ProfileGrid &grid,
                                          const ProfilePhase &phase,
                                          const ProfileRadiation &radiation);
Result<double> density_phase_scale(double weight, double density,
                                   double volume);
Result<PowderProfile> postprocess(const ProfileGrid &grid,
                                  std::span<const double> weighted,
                                  ProfileDisplay display);
Result<PowderProfile> powder_profile(const ProfileGrid &grid,
                                     const ProfileRadiation &radiation,
                                     std::span<const ProfilePhase> phases,
                                     ProfileDisplay display = {});

} // namespace qualpf::profile
