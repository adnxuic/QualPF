#pragma once

#include "qualpf/algorithms/identity.h"
#include "qualpf/domain/pattern.h"
#include "qualpf/migration/backends.h"
#include <cstdint>
#include <variant>

namespace qualpf::preprocessing {

struct FwhmStandardDeviations { std::vector<double> values; };
struct FwhmPoissonCounts {}; // Explicitly declares intensity to be independent raw counts.
struct FwhmPoissonRates { std::vector<double> counts, seconds; };
struct FwhmCovarianceEntry { std::size_t first = 0, second = 0; double value = 0; };
struct FwhmCovariance {
    std::vector<double> diagonal;
    // Unordered unique upper-triangle entries (first < second); omitted entries are zero.
    std::vector<FwhmCovarianceEntry> upper;
};
using FwhmNoiseData = std::variant<std::monostate, FwhmStandardDeviations,
    FwhmPoissonCounts, FwhmPoissonRates, FwhmCovariance>;
enum class FwhmNoiseModel { unknown, independent_standard_deviations, poisson_counts, poisson_rates, covariance };

// This is a separate geometric algorithm, not the Classical fitted-width contract.
struct GeometricFwhmInput {
    DatasetId dataset;
    std::string source_id;
    Radiation radiation;
    ProcessingState processing;
    std::vector<TwoThetaDegrees> two_theta;
    std::vector<double> intensity;
    std::vector<double> background; // Required, aligned, in the intensity units; zero is explicit.
    // Empty means all valid, none saturated, or no explicit breaks respectively.
    std::vector<std::uint8_t> valid;
    std::vector<std::uint8_t> saturated;
    std::vector<std::uint8_t> break_before;
    FwhmNoiseData noise;
    std::string noise_source; // Caller-supplied provenance, in the net signal's units.
};

struct GeometricFwhmOptions {
    double minimum_relative_prominence = 0.05;
    double minimum_samples_per_fwhm = 6;
    double preferred_samples_per_fwhm = 8;
    double minimum_prominence_snr = 5;
    double minimum_snr = 20;
    double preferred_snr = 50;
    std::optional<AngularWidthDegrees> minimum_width;
    std::optional<AngularWidthDegrees> maximum_width;
};

enum class FwhmNoiseState { noise_unknown, noise_verified, zero_noise };
enum class FwhmRejection {
    none, nonpositive_height, insignificant, plateau, saturated,
    invalid_apex, missing_left_crossing, missing_right_crossing,
    overlapping_peak, half_height_plateau, invalid_width, undersampled, width_out_of_range,
    insufficient_snr
};
const char *fwhm_rejection_name(FwhmRejection) noexcept;

struct FwhmCrossing {
    std::size_t first = 0, second = 0; // Ascending original sample indices.
    TwoThetaDegrees position;
    double fraction = 0; // From first towards second; fixed bracket for propagation.
};
struct PeakFwhmDiagnostic {
    std::size_t sample_index = 0, plateau_last = 0;
    double sampled_height = 0, prominence = 0;
    bool significant = false, eligible = false;
    FwhmRejection rejection = FwhmRejection::none;
    FwhmNoiseState noise_state = FwhmNoiseState::noise_unknown;
    std::optional<TwoThetaDegrees> apex;
    std::optional<double> apex_offset_degrees; // Local offset avoids cancellation in uncertainty propagation.
    std::optional<double> height;
    std::optional<FwhmCrossing> left, right;
    std::optional<AngularWidthDegrees> width, left_width, right_width;
    std::optional<double> sampling_density, asymmetry, snr;
    std::optional<AngularWidthDegrees> uncertainty;
    std::optional<double> relative_uncertainty;
    bool preferred_sampling = false, preferred_snr = false;
    bool conditional_uncertainty = false; // Fixed apex index and crossing brackets.
};
struct GeometricFwhmResult {
    enum class State { measured, no_eligible_peak };
    AlgorithmIdentity algorithm{"qualpf.peaks.fwhm.geometric", 1};
    NumericalPolicy numerical_policy = NumericalPolicy::explicit_float64;
    DatasetId dataset;
    std::string source_id;
    Radiation radiation;
    ProcessingState processing;
    FwhmNoiseModel noise_model = FwhmNoiseModel::unknown;
    std::string noise_source;
    GeometricFwhmOptions applied;
    State state = State::no_eligible_peak;
    std::vector<PeakFwhmDiagnostic> candidates;
    std::optional<std::size_t> selected_candidate;
    std::optional<AngularWidthDegrees> default_fwhm;
};

// Synchronous, nonpublishing and cancellable. No borrowing is retained.
// Invalid intensity/background samples split segments, but invalid coordinates fail.
Result<GeometricFwhmResult> estimate_geometric_fwhm(
    const GeometricFwhmInput &, const GeometricFwhmOptions &, const ExecutionContext &);

} // namespace qualpf::preprocessing
