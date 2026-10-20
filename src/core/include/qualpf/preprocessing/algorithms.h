#pragma once
#include "qualpf/algorithms/identity.h"
#include "qualpf/domain/analysis_spectrum.h"
#include "qualpf/migration/backends.h"
#include "qualpf/preprocessing/legacy.h"

namespace qualpf::preprocessing {
struct InitialPeakOptions {
    float slope = .1f;
    bool automatic_window = true;
    int half_window = 11;
    int polynomial_degree = 9;
    friend bool operator==(const InitialPeakOptions &, const InitialPeakOptions &) = default;
};
Status validate_initial_peak_options(const InitialPeakOptions &);
struct InitialPeakCandidate {
    // Full-pattern derivative trigger, before fitting and peak-top refinement.
    std::size_t sample_index = 0;
};
struct InitialPeakResult {
    DatasetId dataset;
    std::vector<InitialPeakCandidate> candidates;
    // Actual detection parameters, including the resolved sample half-window.
    // Initial detection always uses the full analysis signal.
    PeakOptions applied;
};
struct FwhmEstimationResult {
    FwhmEstimate estimate;
    std::vector<RejectedPeakCandidate> rejected_candidates;
};
Status validate_initial_peak_candidates(const InitialPeakResult &, const Pattern &);
Status validate_fwhm_result(const FwhmEstimationResult &, const InitialPeakResult &, const Pattern &);
class IBackgroundEstimator {
  public:
    virtual ~IBackgroundEstimator() = default;
    virtual AlgorithmIdentity identity() const = 0;
    virtual Result<BackgroundResult> estimate(const Pattern &, const BackgroundOptions &, WavelengthAngstrom,
        std::optional<std::span<const BackgroundPoint>>, const ExecutionContext &) const = 0;
};
class IInitialPeakFinder {
  public:
    virtual ~IInitialPeakFinder() = default;
    virtual AlgorithmIdentity identity() const = 0;
    virtual Result<InitialPeakResult> find(const AnalysisSpectrum &, WavelengthAngstrom,
        const InitialPeakOptions &, const ExecutionContext &) const = 0;
    Result<InitialPeakResult> find(const AnalysisSpectrum &p, WavelengthAngstrom w, const ExecutionContext &c) const {
        return find(p, w, InitialPeakOptions{}, c);
    }
};
class IFwhmEstimator {
  public:
    virtual ~IFwhmEstimator() = default;
    virtual AlgorithmIdentity identity() const = 0;
    virtual Result<FwhmEstimationResult> estimate(const AnalysisSpectrum &, const InitialPeakResult &,
                                                 const ExecutionContext &) const = 0;
};
class IExperimentalPeakExtractor {
  public:
    virtual ~IExperimentalPeakExtractor() = default;
    virtual AlgorithmIdentity identity() const = 0;
    virtual Result<PeakSearchResult> extract(const AnalysisSpectrum &, const PeakCalibration &, WavelengthAngstrom,
        AngularWidthDegrees matching_tolerance, const ExecutionContext &) const = 0;
};
class CppBackgroundEstimator final : public IBackgroundEstimator {
  public:
    AlgorithmIdentity identity() const override { return {"qualpf.background.cpp", 1}; }
    Result<BackgroundResult> estimate(const Pattern &, const BackgroundOptions &, WavelengthAngstrom,
        std::optional<std::span<const BackgroundPoint>>, const ExecutionContext &) const override;
};
class ClassicInitialPeakFinder final : public IInitialPeakFinder {
  public:
    using IInitialPeakFinder::find;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.initial.cpp", 5}; }
    Result<InitialPeakResult> find(const AnalysisSpectrum &, WavelengthAngstrom,
        const InitialPeakOptions &, const ExecutionContext &) const override;
};
class ClassicFwhmEstimator final : public IFwhmEstimator {
  public:
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.fwhm.cpp", 3}; }
    Result<FwhmEstimationResult> estimate(const AnalysisSpectrum &, const InitialPeakResult &,
                                         const ExecutionContext &) const override;
};
class ClassicPeakExtractor final : public IExperimentalPeakExtractor {
  public:
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.experimental.cpp", 3}; }
    Result<PeakSearchResult> extract(const AnalysisSpectrum &, const PeakCalibration &, WavelengthAngstrom,
        AngularWidthDegrees, const ExecutionContext &) const override;
};
// Fixed Classical policy: physical FWHM -> search parameters, or the original
// wavelength fallback. A manual range never needs initial peaks or width evidence.
Result<PeakCalibration> prepare_classic_peak_parameters(const PeakOptions &, const FwhmEstimate &,
    WavelengthAngstrom, const ExecutionContext &);
// Explicit supplied physical width; no initial fitting or wavelength fallback.
Result<PeakCalibration> prepare_supplied_fwhm_parameters(const PeakOptions &, AngularWidthDegrees,
    PeakCalibration::Source, const ExecutionContext &);
// Compatibility orchestration. Product use cases evaluate the stages explicitly.
// Manual range skips initial detection and FWHM; formal extraction never initializes itself.
Result<PeakSearchResult> extract_experimental_peaks(const AnalysisSpectrum &, const PeakOptions &, WavelengthAngstrom,
    AngularWidthDegrees, const IInitialPeakFinder &, const IFwhmEstimator &,
    const IExperimentalPeakExtractor &, const ExecutionContext &);
} // namespace qualpf::preprocessing
