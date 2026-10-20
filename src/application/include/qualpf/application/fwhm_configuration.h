#pragma once
#include "qualpf/application/product_values.h"
#include "qualpf/application/original_signal.h"
#include "qualpf/preprocessing/geometric_fwhm.h"
#include "qualpf/preprocessing/algorithms.h"
#include <memory>

namespace qualpf::application {
enum class FwhmMethod { gaussian, geometric };
enum class FwhmOrigin { fixed, gaussian, geometric };
struct FwhmConfiguration {
    FwhmMethod method = FwhmMethod::gaussian;
    bool use_fixed = false;
    AngularWidthDegrees default_fwhm{.1};
    int minimum_sampling_density = 6;
    MetadataObject extensions;
    friend bool operator==(const FwhmConfiguration &, const FwhmConfiguration &) = default;
};
struct FwhmUsage {
    AngularWidthDegrees width;
    FwhmOrigin origin = FwhmOrigin::fixed;
    AlgorithmIdentity algorithm;
    DatasetId dataset;
    std::string source_id;
    std::uint64_t source_revision = 0;
    int minimum_sampling_density = 6;
    std::size_t width_count = 0;
    bool current = false; // QXP v1/v2 automatic evidence remains historical.
    bool manual = false; // Explicit GUI estimation; persisted only by QXP v3, never inferred on load.
    std::optional<preprocessing::InitialPeakOptions> initial_peak_options; // Actual Gaussian request; persisted only by QXP v3.
    std::shared_ptr<const Pattern> source_pattern; // Before estimator preparation; persisted only by QXP v3.
    std::optional<OriginalSignalProvenance> source_original_signal;
    std::shared_ptr<const Pattern> input_pattern; // Actual estimator input; persisted only by QXP v3.
    std::shared_ptr<const preprocessing::GeometricFwhmResult> geometric;
    MetadataObject extensions;
};
Status validateFwhmConfiguration(const FwhmConfiguration &);
Status validateFwhmUsage(const FwhmUsage &);
MetadataObject fwhmConfigurationValues(const FwhmConfiguration &);
MetadataObject fwhmUsageValues(const FwhmUsage &);
} // namespace qualpf::application
