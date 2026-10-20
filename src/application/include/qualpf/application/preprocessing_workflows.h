#pragma once
#include "qualpf/application/product_session.h"

namespace qualpf::application {
struct CommonPreprocessingRequest {
    std::optional<preprocessing::BackgroundOptions> background;
    std::optional<std::vector<preprocessing::BackgroundPoint>> background_points;
    std::vector<preprocessing::BackgroundFilter> background_filters;
    std::optional<preprocessing::SmoothingOptions> smoothing;
    AnalysisPurpose purpose = AnalysisPurpose::peak_measurement;
    // Absent or manual parameters skip initial detection and width estimation.
    std::optional<preprocessing::PeakOptions> peaks;
    bool estimate_fwhm = true; // Configured callers evaluate physical width separately.
    std::optional<preprocessing::InitialPeakOptions> initial_peaks;
    WavelengthAngstrom wavelength;
};
struct CommonPreprocessingResult {
    ProductHandle source;
    Pattern working_pattern;
    std::shared_ptr<const AnalysisSpectrum> spectrum;
    std::optional<preprocessing::BackgroundResult> background;
    std::optional<preprocessing::InitialPeakResult> initial_peaks;
    std::optional<preprocessing::FwhmEstimationResult> fwhm;
};
// Pure evaluation: no formal peaks, SQL, scoring, accepted phases or publication.
// An explicit owned working input also supports private editor drafts.
Result<CommonPreprocessingResult> evaluateCommonPreprocessing(ProductHandle, Pattern,
    const CommonPreprocessingRequest &, const ExecutionContext &);
Result<CommonPreprocessingResult> evaluateCommonPreprocessing(ProductHandle,
    const CommonPreprocessingRequest &, const ExecutionContext &);

struct FormalPeakExtractionRequest {
    std::shared_ptr<const AnalysisSpectrum> spectrum;
    preprocessing::PeakCalibration parameters;
    WavelengthAngstrom wavelength;
    AngularWidthDegrees tolerance;
};
// Does not estimate a background, find initial peaks or estimate FWHM.
Result<preprocessing::PeakSearchResult> evaluateFormalPeakExtraction(ProductHandle,
    const FormalPeakExtractionRequest &, const ExecutionContext &);
} // namespace qualpf::application
