#pragma once

#include "qualpf/domain/pattern.h"
#include "qualpf/domain/result.h"
#include <array>

namespace qualpf::peaks {

// Isolated Phase28M compatibility operations. No product registration or
// qualification is implied. See measurement-core/contract-001.md.
struct NeighborPeak {
  TwoThetaDegrees two_theta;
  AngularWidthDegrees fwhm;
};
struct PeakMeasurement {
  TwoThetaDegrees two_theta;
  DSpacingAngstrom d_spacing;
  double intensity;
  double original_intensity;
  AngularWidthDegrees fwhm;
  std::optional<double> integrated_intensity;
};
// Ephemeral numerical evidence, deliberately separate from canonical peak data.
struct MeasurementTrace {
  double input_coordinate = 0;
  std::size_t initial_sample = 0, sample = 0;
  std::size_t fit_first = 0, fit_last = 0;
  double mean = 0, sample_sd = 0;
  std::array<double, 3> coefficients{};
  double fitted_center = 0, fitted_height = 0;
  std::optional<double> fitted_width;
  bool used_fallback = false;
  std::size_t integration_first = 0, integration_last = 0;
  // Per selected sample: x, observed y, log(y), normalized x, normalized x^2.
  std::vector<std::array<double, 5>> fit_inputs;
  // Per trapezoid: term and ordered cumulative sum, before the final .5f.
  std::vector<std::array<double, 2>> integration_steps;
};
struct MeasuredPeak {
  PeakMeasurement peak;
  MeasurementTrace trace;
};
struct ImportedCoordinate {
  TwoThetaDegrees two_theta;
  // Native d-mode imports preserve the parsed d, not a round trip through tth.
  std::optional<DSpacingAngstrom> original_d;
};

// Canonical 2theta only; existing.empty() selects the native first-peak branch.
// Background must be explicitly supplied and fully aligned, including zero yb.
[[nodiscard]] Result<MeasuredPeak>
measure_graph_peak(const Pattern &, TwoThetaDegrees,
                   std::span<const NeighborPeak>, WavelengthAngstrom);

// Actual table-add numerical capability. Native table intensity/area inference
// is commented out; this API does not pretend to measure those fields.
[[nodiscard]] Result<AngularWidthDegrees>
    infer_table_width(TwoThetaDegrees, std::span<const NeighborPeak>);

// Parsed, canonical inputs. Filters strictly within calculation_range, or the
// supplied grid endpoints when no calculation_range is present. Atomic result;
// no file parsing, background estimation, pool updates or publication.
[[nodiscard]] Result<std::vector<MeasuredPeak>>
measure_imported_peaks(const Pattern &, std::span<const ImportedCoordinate>,
                       WavelengthAngstrom);

} // namespace qualpf::peaks
