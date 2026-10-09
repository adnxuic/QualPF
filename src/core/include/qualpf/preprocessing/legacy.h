#pragma once

#include "qualpf/domain/pattern.h"
#include "qualpf/domain/peaks.h"
#include "qualpf/domain/result.h"
#include <span>

namespace qualpf::preprocessing {
// Versioned compatibility parameters, independent of GUI and native global
// state.
enum class SmoothingMethod { savitzky_golay, averaging };
struct SmoothingOptions {
    SmoothingMethod method = SmoothingMethod::savitzky_golay;
    int half_window = 4;
    int polynomial_degree = 5;
    int derivative = 0; // Legacy coefficient convention: no factorial or step scaling.
};
enum class BackgroundMethod { chebyshev, polynomial, cosine, spline, bezier, bruckner, none };
struct BackgroundOptions {
    BackgroundMethod method = BackgroundMethod::chebyshev;
    bool automatic = true;
    int coefficients = 6;
    int iterations = 5;
    int half_window = 50;
    std::optional<AngularRange> bruckner_range;
};
struct BackgroundPoint {
    float x, y;
};
struct BackgroundResult {
    std::vector<double> values;
    std::vector<BackgroundPoint> points;
    std::vector<double> coefficients;
    double origin = 0;
};
struct BackgroundFilter {
    double minimum = 0, maximum = 180;
    int iterations = 5;
    int half_window = 50;
};
struct PeakOptions {
    float threshold_percent = 1.5f;
    float slope = .1f;
    float search_range = .15f; // Negative explicitly requests init_peak_range.
    int half_window = 11;
    int polynomial_degree = 9;
    int maximum_peaks = -1;
    int minimum_peaks = -1;
    bool automatic_window = true;
    std::optional<AngularRange> detection_range; // peak_create input slice, not post-detection filtering.
};
struct PeakSearchResult {
    PeakSet peaks;
    PeakSet candidates;
    PeakOptions applied;
    double automatic_tolerance = .5;
};

Result<std::vector<double>> smooth(std::span<const double>, const SmoothingOptions &);
Result<BackgroundResult> background(const Pattern &, const BackgroundOptions &, WavelengthAngstrom);
// UserBack preserves manual points for fitted bases/none. Spline, Bezier and
// Brueckner reselect their own points, matching the Legacy product operation.
Result<BackgroundResult> background_from_points(const Pattern &, const BackgroundOptions &,
                                                WavelengthAngstrom, std::span<const BackgroundPoint>);
// smoothback on an existing curve, including partial intervals. Unlike a new
// partial Brueckner estimate, values outside the interval are
// defined/preserved.
Result<SampledSignal> filter_background(const Pattern &, const BackgroundFilter &);
Result<Pattern> subtract_background(const Pattern &);
Result<PeakSearchResult> find_peaks(const Pattern &, const PeakOptions &, WavelengthAngstrom,
                                    AngularWidthDegrees tolerance);
Result<PeakSet> export_peaks(const PeakSet &, AngularWidthDegrees tolerance);
Result<double> automatic_tolerance(const PeakSet &);
Result<DSpacingAngstrom> legacy_d(TwoThetaDegrees, WavelengthAngstrom);
Result<TwoThetaDegrees> legacy_two_theta(DSpacingAngstrom, WavelengthAngstrom);
enum class PeakTextFormat { two_theta, d_spacing, details };
// Empty peak sets yield no payload (Legacy SavePeaks does not create a file).
Result<std::string> export_peak_text(const PeakSet &, PeakTextFormat);
} // namespace qualpf::preprocessing
