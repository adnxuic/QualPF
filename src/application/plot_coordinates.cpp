#include "qualpf/application/plot_coordinates.h"
#include "qualpf/numerics/basic.h"
#include "qualpf/numerics/display_coordinates.h"
#include <cmath>
namespace qualpf::application {
namespace {
constexpr numerics::display::Axis coreAxis(PlotAxis axis) {
    return static_cast<numerics::display::Axis>(axis);
}
static_assert(int(PlotAxis::two_theta) == int(numerics::display::Axis::two_theta) &&
              int(PlotAxis::d_spacing) == int(numerics::display::Axis::d_spacing) &&
              int(PlotAxis::inverse_d) == int(numerics::display::Axis::inverse_d) &&
              int(PlotAxis::inverse_d_squared) == int(numerics::display::Axis::inverse_d2));
}

double plotDSpacing(double x, double wave) { return numerics::display::dvalue(x, wave); }
double plotTwoThetaFromD(double d, double wave) { return numerics::display::tthvalue(d, wave); }
double plotTwoTheta(double x, double wave, PlotAxis axis) { return numerics::display::tthvalue(x, wave, coreAxis(axis)); }
double plotFromTwoTheta(double x, double wave, PlotAxis axis) { return numerics::display::fromTwoTheta(x, wave, coreAxis(axis)); }
double plotFromSpacing(double x, double wave, PlotAxis axis) { return numerics::display::fromSpacing(x, wave, coreAxis(axis)); }
double plotBetweenSpacings(double x, PlotAxis from, PlotAxis to) { return numerics::display::betweenSpacings(x, coreAxis(from), coreAxis(to)); }
Result<double>
peakCanonicalCoordinate(double x, PlotAxis axis,
                        std::optional<WavelengthAngstrom> wave) {
  const auto fail = [](const char *reason) {
    return Result<double>{Error{ErrorCode::invalid_input, "peaks.coordinate",
                                "Invalid peak coordinate", reason}};
  };
  if (!std::isfinite(x))
    return fail("nonfinite_display_coordinate");
  if (axis == PlotAxis::two_theta) {
    if (x <= 0 || x >= 180)
      return fail("out_of_domain_display_coordinate");
    return Result<double>{x};
  }
  if (axis < PlotAxis::d_spacing || axis > PlotAxis::inverse_d_squared)
    return fail("out_of_domain_display_coordinate");
  if (!wave)
    return fail("missing_wavelength");
  if (!std::isfinite(wave->value) || wave->value <= 0)
    return fail("invalid_wavelength");
  if (x <= 0)
    return fail("nonpositive_display_coordinate");
  const double spacing = numerics::display::spacing(x, coreAxis(axis));
  if (!std::isfinite(spacing))
    return fail("nonfinite_conversion");
  if (!numerics::to_two_theta(DSpacingAngstrom{spacing}, *wave))
    return fail("out_of_domain_display_coordinate");
  const double canonical = plotTwoTheta(x, wave->value, axis);
  if (!std::isfinite(canonical))
    return fail("nonfinite_conversion");
  if (canonical <= 0 || canonical >= 180)
    return fail("out_of_domain_display_coordinate");
  return Result<double>{canonical};
}
Result<double> backgroundCanonicalCoordinate(double x, PlotAxis axis,
                                                    std::optional<WavelengthAngstrom> wave) {
    const auto reject = [](const char *reason) {
        return Result<double>{Error{ErrorCode::invalid_input, "background.coordinate",
                                    "Invalid background gesture coordinate", reason}};
    };
    if (!std::isfinite(x))
        return reject("nonfinite_display_coordinate");
    if (axis == PlotAxis::two_theta)
        return Result<double>{x};
    if (axis < PlotAxis::d_spacing || axis > PlotAxis::inverse_d_squared)
        return reject("out_of_domain_display_coordinate");
    if (!wave)
        return reject("missing_wavelength");
    if (!std::isfinite(wave->value) || wave->value <= 0)
        return reject("invalid_wavelength");
    if (x <= 0)
        return reject("nonpositive_display_coordinate");
    // The same inverse transport as xpdutils; Core owns the physical domain test.
    const double spacing = numerics::display::spacing(x, coreAxis(axis));
    if (!std::isfinite(spacing))
        return reject("nonfinite_conversion");
    if (!numerics::to_two_theta(DSpacingAngstrom{spacing}, *wave))
        return reject("out_of_domain_display_coordinate");
    const double canonical = plotTwoTheta(x, wave->value, axis);
    if (!std::isfinite(canonical))
        return reject("nonfinite_conversion");
    if (canonical <= 0 || canonical > 180)
        return reject("out_of_domain_display_coordinate");
    return Result<double>{canonical};
}
} // namespace qualpf::application
