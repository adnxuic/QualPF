#pragma once
#include "qualpf/application/product_values.h"
#include "qualpf/application/fwhm_configuration.h"
#include "qualpf/application/initial_peak_configuration.h"
#include "qualpf/preprocessing/legacy.h"
namespace qualpf::application {
struct PeakSelection {
    double minimum = 0, maximum = 180;
    float threshold_percent = 1.5f;
    int maximum_peaks = -1;
    bool append = false;
};
struct PreprocessingConfiguration {
    std::optional<InitialPeakConfiguration> initial_peaks;
    std::shared_ptr<const InitialPeakUsage> initial_peak_usage;
    // Absent keeps legacy/non-GUI callers on their existing preparation contract.
    std::optional<FwhmConfiguration> fwhm;
    std::optional<FwhmUsage> fwhm_usage;
    std::optional<FwhmUsage> peak_fwhm_usage; // Evidence for the existing peak snapshot.
    MetadataObject extensions;
    std::optional<preprocessing::BackgroundOptions> background;
    std::optional<preprocessing::SmoothingOptions> smoothing;
    std::optional<preprocessing::PeakOptions> peaks;
    std::optional<PeakSelection> selection;
    // Complete pre-filter pool, never reconstructed from the selected snapshot.
    // Absent means unavailable, whereas a present empty pool is a known result.
    std::optional<PeakSet> peak_candidates;
    std::optional<std::string> candidate_failure;
    std::optional<bool> smoothing_applied;
    std::optional<std::vector<preprocessing::BackgroundPoint>> background_points;
    std::vector<preprocessing::BackgroundFilter> background_filters;
    // Runtime preparation evidence; not a new persistence field or peak table.
    std::optional<preprocessing::PeakCalibration> peak_calibration;
    std::optional<preprocessing::FwhmEstimate> fwhm_estimate;
    std::vector<preprocessing::RejectedPeakCandidate> rejected_candidates;
};
MetadataObject preprocessingValues(const PreprocessingConfiguration &);
Status validatePreprocessing(const PreprocessingConfiguration &);
Status validatePeakRejections(std::span<const preprocessing::RejectedPeakCandidate>, const Pattern &,
    std::optional<preprocessing::PeakDetectionStage> expected_stage = {});
} // namespace qualpf::application
