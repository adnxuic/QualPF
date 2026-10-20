#pragma once
// QPF 1.0 protocol tokens are immutable, independent of C++ enumeration order.
#include "qpf_schema.h"
#include <array>
#include <utility>
namespace qualpf::infrastructure::qpf {
template <class T> struct Tokens;
template <> struct Tokens<RadiationKind> {
    static constexpr std::array values{std::pair{RadiationKind::x_ray, "x_ray"},
                                       std::pair{RadiationKind::neutron, "neutron"},
                                       std::pair{RadiationKind::electron, "electron"}};
};
template <> struct Tokens<IntensityScale> {
    static constexpr std::array values{std::pair{IntensityScale::unspecified, "unspecified"},
                                       std::pair{IntensityScale::counts, "counts"},
                                       std::pair{IntensityScale::relative, "relative"}};
};
template <> struct Tokens<SourcePrecision> {
    static constexpr std::array values{std::pair{SourcePrecision::unknown, "unknown"},
                                       std::pair{SourcePrecision::float32_abi, "float32_abi"},
                                       std::pair{SourcePrecision::float64, "float64"}};
};
template <> struct Tokens<NumericalPolicy> {
    static constexpr std::array values{
        std::pair{NumericalPolicy::unspecified, "unspecified"},
        std::pair{NumericalPolicy::legacy_mixed_precision, "legacy_mixed_precision"},
        std::pair{NumericalPolicy::explicit_float64, "explicit_float64"}};
};
template <> struct Tokens<BackgroundMethod> {
    static constexpr std::array values{std::pair{BackgroundMethod::chebyshev, "chebyshev"},
                                       std::pair{BackgroundMethod::polynomial, "polynomial"},
                                       std::pair{BackgroundMethod::cosine, "cosine"},
                                       std::pair{BackgroundMethod::spline, "spline"},
                                       std::pair{BackgroundMethod::bezier, "bezier"},
                                       std::pair{BackgroundMethod::bruckner, "bruckner"},
                                       std::pair{BackgroundMethod::none, "none"}};
};
template <> struct Tokens<SmoothingMethod> {
    static constexpr std::array values{std::pair{SmoothingMethod::savitzky_golay, "savitzky_golay"},
                                       std::pair{SmoothingMethod::averaging, "averaging"}};
};
template <> struct Tokens<FwhmMethod> {
    static constexpr std::array values{std::pair{FwhmMethod::gaussian, "gaussian"},
                                       std::pair{FwhmMethod::geometric, "geometric"}};
};
template <> struct Tokens<FwhmOrigin> {
    static constexpr std::array values{std::pair{FwhmOrigin::fixed, "fixed"},
                                       std::pair{FwhmOrigin::gaussian, "gaussian"},
                                       std::pair{FwhmOrigin::geometric, "geometric"}};
};
template <> struct Tokens<FwhmNoiseState> {
    static constexpr std::array values{std::pair{FwhmNoiseState::noise_unknown, "noise_unknown"},
                                       std::pair{FwhmNoiseState::noise_verified, "noise_verified"},
                                       std::pair{FwhmNoiseState::zero_noise, "zero_noise"}};
};
template <> struct Tokens<FwhmNoiseModel> {
    static constexpr std::array values{
        std::pair{FwhmNoiseModel::unknown, "unknown"},
        std::pair{FwhmNoiseModel::independent_standard_deviations, "independent_standard_deviations"},
        std::pair{FwhmNoiseModel::poisson_counts, "poisson_counts"},
        std::pair{FwhmNoiseModel::poisson_rates, "poisson_rates"},
        std::pair{FwhmNoiseModel::covariance, "covariance"}};
};
template <> struct Tokens<FwhmRejection> {
    static constexpr std::array values{
        std::pair{FwhmRejection::none, "none"},
        std::pair{FwhmRejection::nonpositive_height, "nonpositive_height"},
        std::pair{FwhmRejection::insignificant, "insignificant"},
        std::pair{FwhmRejection::plateau, "plateau"},
        std::pair{FwhmRejection::saturated, "saturated"},
        std::pair{FwhmRejection::invalid_apex, "invalid_apex"},
        std::pair{FwhmRejection::missing_left_crossing, "missing_left_crossing"},
        std::pair{FwhmRejection::missing_right_crossing, "missing_right_crossing"},
        std::pair{FwhmRejection::overlapping_peak, "overlapping_peak"},
        std::pair{FwhmRejection::half_height_plateau, "half_height_plateau"},
        std::pair{FwhmRejection::invalid_width, "invalid_width"},
        std::pair{FwhmRejection::undersampled, "undersampled"},
        std::pair{FwhmRejection::width_out_of_range, "width_out_of_range"},
        std::pair{FwhmRejection::insufficient_snr, "insufficient_snr"}};
};
template <> struct Tokens<GeometricFwhmResult::State> {
    static constexpr std::array values{
        std::pair{GeometricFwhmResult::State::measured, "measured"},
        std::pair{GeometricFwhmResult::State::no_eligible_peak, "no_eligible_peak"}};
};
template <> struct Tokens<FwhmEstimate::State> {
    static constexpr std::array values{std::pair{FwhmEstimate::State::no_initial_peaks, "no_initial_peaks"},
                                       std::pair{FwhmEstimate::State::measured, "measured"}};
};
template <> struct Tokens<PeakCalibration::Source> {
    static constexpr std::array values{
        std::pair{PeakCalibration::Source::manual_parameters, "manual_parameters"},
        std::pair{PeakCalibration::Source::wavelength_seed, "wavelength_seed"},
        std::pair{PeakCalibration::Source::strongest_initial_widths, "strongest_initial_widths"},
        std::pair{PeakCalibration::Source::fixed_fwhm, "fixed_fwhm"},
        std::pair{PeakCalibration::Source::geometric_fwhm, "geometric_fwhm"}};
};
template <> struct Tokens<PeakDetectionStage> {
    static constexpr std::array values{std::pair{PeakDetectionStage::initial, "initial"},
                                       std::pair{PeakDetectionStage::formal, "formal"}};
};
template <> struct Tokens<PfsmSettings::Weighting> {
    static constexpr std::array values{std::pair{PfsmSettings::Weighting::automatic, "automatic"},
                                       std::pair{PfsmSettings::Weighting::equal, "equal"}};
};
template <> struct Tokens<WorkflowId> {
    static constexpr std::array values{std::pair{WorkflowId::preprocessing, "preprocessing"},
                                       std::pair{WorkflowId::csm, "csm"}, std::pair{WorkflowId::pfsm, "pfsm"},
                                       std::pair{WorkflowId::peak_search, "peak_search"}};
};
template <> struct Tokens<WorkflowStep> {
    static constexpr std::array values{std::pair{WorkflowStep::range, "range"},
                                       std::pair{WorkflowStep::smoothing, "smoothing"},
                                       std::pair{WorkflowStep::kalpha2, "kalpha2"},
                                       std::pair{WorkflowStep::background, "background"},
                                       std::pair{WorkflowStep::initial_peaks, "initial_peaks"},
                                       std::pair{WorkflowStep::fwhm, "fwhm"},
                                       std::pair{WorkflowStep::peaks, "peaks"},
                                       std::pair{WorkflowStep::search, "search"},
                                       std::pair{WorkflowStep::phases, "phases"},
                                       std::pair{WorkflowStep::rir, "rir"}};
};
template <> struct Tokens<WorkflowStepStatus> {
    static constexpr std::array values{std::pair{WorkflowStepStatus::not_started, "not_started"},
                                       std::pair{WorkflowStepStatus::running, "running"},
                                       std::pair{WorkflowStepStatus::completed, "completed"},
                                       std::pair{WorkflowStepStatus::skipped, "skipped"},
                                       std::pair{WorkflowStepStatus::failed, "failed"},
                                       std::pair{WorkflowStepStatus::cancelled, "cancelled"},
                                       std::pair{WorkflowStepStatus::invalidated, "invalidated"}};
};
template <> struct Tokens<SearchMethod> {
    static constexpr std::array values{std::pair{SearchMethod::classic_sm, "classic_sm"},
                                       std::pair{SearchMethod::pfsm, "pfsm"}};
};
template <> struct Tokens<AnalysisPurpose> {
    static constexpr std::array values{std::pair{AnalysisPurpose::peak_measurement, "peak_measurement"},
                                       std::pair{AnalysisPurpose::classical_search, "classical_search"}};
};
template <> struct Tokens<CandidateQuery::Route> {
    static constexpr std::array values{
        std::pair{CandidateQuery::Route::constrained_browse, "constrained_browse"},
        std::pair{CandidateQuery::Route::matching, "matching"},
        std::pair{CandidateQuery::Route::stored_strongest, "stored_strongest"}};
};
template <> struct Tokens<CandidateQuery::ElementOperator> {
    static constexpr std::array values{std::pair{CandidateQuery::ElementOperator::and_or, "and_or"},
                                       std::pair{CandidateQuery::ElementOperator::only, "only"},
                                       std::pair{CandidateQuery::ElementOperator::any, "any"}};
};
template <> struct Tokens<ProfileFitTermination> {
    static constexpr std::array values{
        std::pair{ProfileFitTermination::fixed_shifts, "fixed_shifts"},
        std::pair{ProfileFitTermination::gradient, "gradient"},
        std::pair{ProfileFitTermination::cost, "cost"},
        std::pair{ProfileFitTermination::step, "step"},
        std::pair{ProfileFitTermination::cost_and_step, "cost_and_step"},
        std::pair{ProfileFitTermination::budget, "budget"},
        std::pair{ProfileFitTermination::no_progress, "no_progress"},
        std::pair{ProfileFitTermination::feasible_fallback, "feasible_fallback"}};
};
template <> struct Tokens<PfsmObservationRequest::Signal> {
    static constexpr std::array values{
        std::pair{PfsmObservationRequest::Signal::observed, "observed"},
        std::pair{PfsmObservationRequest::Signal::original_observed, "original_observed"}};
};
template <> struct Tokens<PfsmCandidateRecallMode> {
    static constexpr std::array values{std::pair{PfsmCandidateRecallMode::off, "off"},
                                       std::pair{PfsmCandidateRecallMode::shadow, "shadow"},
                                       std::pair{PfsmCandidateRecallMode::enabled, "enabled"}};
};
template <> struct Tokens<PfsmCandidateRecallVersion> {
    static constexpr std::array values{std::pair{PfsmCandidateRecallVersion::v2, "v2"}};
};
template <> struct Tokens<PfsmRirRevision::Radiation> {
    static constexpr std::array values{std::pair{PfsmRirRevision::Radiation::cu_kalpha, "cu_kalpha"}};
};
template <> struct Tokens<io::powder_cif::RadiationOrigin> {
    static constexpr std::array values{
        std::pair{io::powder_cif::RadiationOrigin::source_file, "source_file"},
        std::pair{io::powder_cif::RadiationOrigin::user_supplied, "user_supplied"},
        std::pair{io::powder_cif::RadiationOrigin::application_default, "application_default"}};
};
} // namespace qualpf::infrastructure::qpf
