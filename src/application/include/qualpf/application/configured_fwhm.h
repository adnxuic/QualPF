#pragma once
#include "qualpf/application/geometric_fwhm.h"

namespace qualpf::application {
struct ConfiguredFwhmRequest {
    Pattern pattern;
    FwhmConfiguration configuration;
    // A checked raw/background snapshot from a compound peak operation.
    std::shared_ptr<const AnalysisSpectrum> spectrum;
    bool manual = false;
    std::optional<preprocessing::InitialPeakOptions> initial_peaks;
    std::optional<WorkflowId> workflow;
};
struct ConfiguredFwhmEvaluation {
    ProductHandle source;
    FwhmConfiguration configuration;
    std::optional<FwhmUsage> usage;
    std::optional<preprocessing::FwhmEstimationResult> gaussian;
    std::shared_ptr<const preprocessing::GeometricFwhmResult> geometric;
    std::shared_ptr<const InitialPeakUsage> initial;
    ProductHandle preparation;
};
Result<ConfiguredFwhmEvaluation> evaluateConfiguredFwhm(
    ProductHandle, const ConfiguredFwhmRequest &, const ExecutionContext &);
Error configuredFwhmFailure(const ConfiguredFwhmEvaluation &);
Status applyConfiguredFwhm(ProductState &, const ConfiguredFwhmEvaluation &);
Result<preprocessing::PeakCalibration> prepareConfiguredPeakParameters(
    const preprocessing::PeakOptions &, const FwhmUsage &, WavelengthAngstrom, const ExecutionContext &);
void invalidateAutomaticFwhm(PreprocessingConfiguration &);
} // namespace qualpf::application
