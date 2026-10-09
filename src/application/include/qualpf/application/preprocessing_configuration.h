#pragma once
#include "qualpf/application/product_values.h"
#include "qualpf/preprocessing/legacy.h"
namespace qualpf::application {
struct PeakSelection {
    double minimum = 0, maximum = 180;
    float threshold_percent = 1.5f;
    int maximum_peaks = -1;
    bool append = false;
};
struct PreprocessingConfiguration {
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
};
MetadataObject preprocessingValues(const PreprocessingConfiguration &);
Status validatePreprocessing(const PreprocessingConfiguration &);
} // namespace qualpf::application
