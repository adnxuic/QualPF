#pragma once
#include "qualpf/application/product_values.h"
#include "qualpf/preprocessing/algorithms.h"
#include <memory>

namespace qualpf::application {
struct InitialPeakConfiguration {
    preprocessing::InitialPeakOptions options;
    MetadataObject extensions;
    friend bool operator==(const InitialPeakConfiguration &, const InitialPeakConfiguration &) = default;
};
// Owned evidence without a ProductHandle chain; QXP v3 also preserves candidates.
struct InitialPeakUsage {
    preprocessing::InitialPeakOptions options;
    AlgorithmIdentity algorithm;
    std::uint64_t source_revision = 0;
    std::shared_ptr<const Pattern> source_pattern;
    std::shared_ptr<const AnalysisSpectrum> spectrum;
    preprocessing::InitialPeakResult detection;
};
Status validateInitialPeakUsage(const InitialPeakUsage &);
MetadataObject initialPeakConfigurationValues(const InitialPeakConfiguration &);
} // namespace qualpf::application
