#pragma once
// QXP v3's explicit persisted field inventory. Runtime tasks, weak sources,
// executable templates, caches and session history are deliberately absent.
#include "qualpf/application/pfsm.h"
#include "qualpf/application/workflow.h"
#include "qualpf/domain/analysis_spectrum.h"
namespace qualpf::infrastructure::legacy_fixture::qxp3 {
using namespace application;
using namespace preprocessing;
using namespace profile;
template <class T> struct Schema;
#define MODEL(T, BODY)                                                         \
  template <> struct Schema<T> {                                               \
    template <class A, class V> static void visit(A &a, V &v) { BODY }         \
  };
#define F(NAME) a(#NAME, v.NAME);
MODEL(AngularRange, F(minimum) F(maximum))
MODEL(RadiationLine, F(wavelength) F(relative_weight))
MODEL(Radiation, F(kind) F(lines))
MODEL(SampleWindow, F(first))
MODEL(SampledSignal, F(values) F(window))
MODEL(ProcessingState,
      F(background_subtracted) F(kalpha2_subtracted) F(intensity_scaled))
MODEL(Pattern,
      F(id) F(source_id) F(two_theta) F(observed) F(corrected_two_theta)
          F(original_observed) F(original_kalpha2_subtracted) F(background)
              F(calculated) F(background_subtracted_signal) F(weights)
                  F(radiation) F(calculation_range) F(processing)
                      F(intensity_scale) F(source_precision))
MODEL(Peak, F(id) F(two_theta) F(d_spacing) F(delta_d) F(intensity)
                F(original_intensity) F(fwhm) F(integrated_intensity))
MODEL(PeakSet, F(dataset) F(peaks) F(effective_wavelength) F(intensity_scale)
                   F(source_precision))
MODEL(AlgorithmIdentity, F(id) F(version))
MODEL(AlgorithmSelections,
      F(background) F(initial_peaks) F(fwhm) F(experimental_peaks) F(matcher)
          F(profile_expander) F(profile_fitter))
MODEL(AlgorithmDefaults, F(background) F(peaks))
MODEL(SearchParameters,
      F(delta_two_theta) F(weight_position) F(weight_intensity) F(weight_phases)
          F(minimum_score) F(maximum_entries) F(filter_strongest)
              F(strongest_count) F(include_deleted_cards) F(residual_search))
MODEL(BackgroundOptions, F(method) F(automatic) F(coefficients) F(iterations)
                             F(half_window) F(bruckner_range))
MODEL(BackgroundPoint, F(x) F(y))
MODEL(BackgroundFilter, F(minimum) F(maximum) F(iterations) F(half_window))
MODEL(SmoothingOptions,
      F(method) F(half_window) F(polynomial_degree) F(derivative))
MODEL(PeakOptions, F(threshold_percent) F(slope) F(search_range) F(half_window)
                       F(polynomial_degree) F(maximum_peaks) F(minimum_peaks)
                           F(automatic_window) F(detection_range))
MODEL(InitialPeakOptions,
      F(slope) F(automatic_window) F(half_window) F(polynomial_degree))
MODEL(InitialPeakCandidate, F(sample_index))
MODEL(InitialPeakResult, F(dataset) F(candidates) F(applied))
MODEL(InitialPeakConfiguration, F(options) F(extensions))
MODEL(InitialPeakUsage, F(options) F(algorithm) F(source_revision)
                            F(source_pattern) F(spectrum) F(detection))
MODEL(FwhmConfiguration, F(method) F(use_fixed) F(default_fwhm)
                             F(minimum_sampling_density) F(extensions))
MODEL(FwhmUsage,
      F(width) F(origin) F(algorithm) F(dataset) F(source_id) F(source_revision)
          F(minimum_sampling_density) F(width_count) F(current) F(manual)
              F(initial_peak_options) F(source_pattern)
                  F(source_original_signal) F(input_pattern) F(geometric)
                      F(extensions))
MODEL(FwhmStandardDeviations, F(values))
MODEL(FwhmPoissonCounts, )
MODEL(FwhmPoissonRates, F(counts) F(seconds))
MODEL(FwhmCovarianceEntry, F(first) F(second) F(value))
MODEL(FwhmCovariance, F(diagonal) F(upper))
MODEL(GeometricFwhmOptions,
      F(minimum_relative_prominence) F(minimum_samples_per_fwhm)
          F(preferred_samples_per_fwhm) F(minimum_prominence_snr) F(minimum_snr)
              F(preferred_snr) F(minimum_width) F(maximum_width))
MODEL(FwhmCrossing, F(first) F(second) F(position) F(fraction))
MODEL(PeakFwhmDiagnostic,
      F(sample_index) F(plateau_last) F(sampled_height) F(prominence)
          F(significant) F(eligible) F(rejection) F(noise_state) F(apex)
              F(apex_offset_degrees) F(height) F(left) F(right) F(width)
                  F(left_width) F(right_width) F(sampling_density) F(asymmetry)
                      F(snr) F(uncertainty) F(relative_uncertainty)
                          F(preferred_sampling) F(preferred_snr)
                              F(conditional_uncertainty))
MODEL(GeometricFwhmResult,
      F(algorithm) F(numerical_policy) F(dataset) F(source_id) F(radiation)
          F(processing) F(noise_model) F(noise_source) F(applied) F(state)
              F(candidates) F(selected_candidate) F(default_fwhm))
MODEL(FwhmEstimate, F(state) F(width) F(peak_count))
MODEL(PeakCalibration, F(applied) F(estimated_fwhm) F(formal_search_range)
                           F(width_count) F(source))
MODEL(RejectedPeakCandidate,
      F(stage) F(candidate_index) F(sample_index) F(two_theta) F(reason))
MODEL(PeakSelection,
      F(minimum) F(maximum) F(threshold_percent) F(maximum_peaks) F(append))
MODEL(PreprocessingConfiguration,
      F(initial_peaks) F(initial_peak_usage) F(fwhm) F(fwhm_usage)
          F(peak_fwhm_usage) F(extensions) F(background) F(smoothing) F(peaks)
              F(selection) F(peak_candidates) F(candidate_failure)
                  F(smoothing_applied) F(background_points)
                      F(background_filters) F(peak_calibration) F(fwhm_estimate)
                          F(rejected_candidates))
MODEL(OriginalSignalProvenance,
      F(source) F(intensity_units) F(unclipped) F(intensity_scaled)
          F(standard_deviations) F(raw_poisson_counts) F(noise_source))
MODEL(CsmSettings, F(minimum_fom) F(weight_position) F(weight_intensity)
                       F(weight_phases) F(delta) F(automatic_delta) F(residual)
                           F(strongest) F(exclude_deleted) F(maximum_entries))
MODEL(PfsmSettings,
      F(lower_shift) F(upper_shift) F(minimum_relative_fom) F(weighting)
          F(threads) F(template_cache) F(candidate_recall_enabled)
              F(candidate_recall_limits))
MODEL(WorkflowProfile,
      F(background) F(initial_peaks) F(fwhm) F(peaks) F(selection) F(smoothing)
          F(range) F(csm) F(pfsm) F(extensions))
MODEL(WorkflowTarget, F(workflow) F(step))
MODEL(WorkflowActivity, F(target) F(status) F(detail))
MODEL(WorkflowRecord, F(id) F(workflow) F(step) F(status) F(manual)
                          F(dependencies) F(parameters) F(actual_parameters)
                              F(algorithms) F(input) F(detail) F(extensions))
MODEL(WorkflowState,
      F(active) F(imported_pattern) F(records) F(next_id) F(last_attempt))
MODEL(AlgorithmRunSource, F(algorithms) F(parameters) F(sourceRevision)
                              F(databaseVersion) F(background) F(peaks) F(fwhm))
MODEL(CardAlgorithmSource, F(score) F(scale))
MODEL(AnalysisProvenance,
      F(legacyUnverified) F(residualHistory) F(backgroundSource) F(peakSource)
          F(retainedPeakSources) F(subtractedBackgroundHistory))
MODEL(ProductCard,
      F(id) F(chemicalName) F(chemicalFormula) F(mineralName) F(quality) F(rir)
          F(spaceGroup) F(referenceRir) F(fom) F(fomPeakPos) F(fomIntensity)
              F(scale) F(fomCalculated) F(d) F(tth) F(intensity)
                  F(algorithmSource) F(pfsm))
MODEL(ProductPeaks, F(d) F(deltaD) F(tth) F(intensity) F(intensityOrig) F(fwhm)
                        F(integratedIntensity) F(wave) F(valid))
MODEL(CandidateQuery,
      F(route) F(elementOperator) F(names) F(elements) F(minimumSpecies)
          F(maximumSpecies) F(subfiles) F(crystalSystems) F(spaceGroups)
              F(colors) F(ids) F(cellMinimum) F(cellMaximum)
                  F(calculatedDensityMinimum) F(calculatedDensityMaximum)
                      F(measuredDensityMinimum) F(measuredDensityMaximum) F(d)
                          F(deltaD) F(wavelength) F(excludeDeleted)
                              F(decodeReflections) F(print)
                                  F(strictPairedReflections) F(preselectedIds))
MODEL(ProfileFitOptions,
      F(ftol) F(xtol) F(gtol) F(maximum_evaluations) F(maximum_nnls_iterations))
MODEL(ProfileObservations, F(coordinates) F(observed) F(background) F(weights))
MODEL(ProfileFitParameters, F(scales) F(shifts))
MODEL(ProfileFitDiagnostics,
      F(residual_evaluations) F(difference_evaluations) F(nnls_iterations)
          F(starts) F(rank) F(kkt_violation) F(optimality) F(rank_deficient)
              F(kkt_checked) F(interpolated_iterations)
                  F(exact_final_verification) F(zero_templates)
                      F(unidentifiable_shifts) F(at_bound))
MODEL(ProfileFitResult,
      F(parameters) F(calculated) F(residual) F(q) F(denominator_net)
          F(denominator_observed) F(rwp_net) F(rwp_observed) F(termination)
              F(diagnostics))
MODEL(ProfileFitPhase,
      F(identity) F(lower_shift) F(upper_shift) F(initial_shift))
MODEL(PfsmObservationRequest,
      F(signal) F(signal_source) F(intensity_units) F(noise_source)
          F(unclipped_signal) F(equal_weights) F(original_intensity_scaled)
              F(noise) F(radiation) F(background) F(valid) F(saturated)
                  F(break_before) F(width_options))
MODEL(PfsmCandidateRecallLimits,
      F(recall_candidates) F(local_shortlist) F(protected_per_channel)
          F(review_limit) F(formal_limit))
MODEL(PfsmCandidateRecallConfiguration, F(mode) F(version) F(limits))
MODEL(PfsmProblem, F(background_points) F(observations) F(radiation) F(source)
                       F(original_signal_provenance) F(width) F(physical_width)
                           F(algorithms) F(lower_shift) F(upper_shift) F(fit)
                               F(template_cache) F(candidate_recall))
MODEL(PfsmCandidateEvidence,
      F(original_index) F(generation) F(parameters) F(diagnostics)
          F(termination) F(q) F(rwp_net) F(delta_q) F(delta_rwp))
MODEL(PfsmQuantificationReference, F(maximum_intensity) F(rir) F(original_rir))
MODEL(PfsmRirRevision, F(value) F(source) F(radiation))
MODEL(PfsmCandidateRecallEvidence,
      F(original_index) F(original_score) F(residual_score) F(local_q) F(gain)
          F(original_rank) F(residual_rank) F(local_rank) F(original_shift)
              F(residual_shift) F(original_seeds) F(residual_seeds)
                  F(nnls_iterations) F(recalled) F(retained) F(recall_reason)
                      F(retention_reason) F(protection_reason))
MODEL(PfsmCandidateRecallPhysicalFit,
      F(parameters) F(q) F(gain) F(difference_gain) F(kkt) F(baseline_fallback)
          F(approximated))
MODEL(PfsmCandidateRecallStep,
      F(anchor) F(linear_coefficients) F(physical) F(predicted) F(actual)
          F(distortion) F(radius) F(ratio) F(accepted) F(indistinguishable)
              F(local_boundary))
MODEL(PfsmCandidateRecallSeed, F(initial_shift) F(radius) F(initial_fit)
                                   F(current) F(steps) F(initialized))
MODEL(PfsmCandidateRecallCandidate,
      F(recall) F(best) F(seeds) F(starts) F(initial_gain) F(initial_eta) F(eta)
          F(maximum_distortion) F(nnls_calls) F(nnls_iterations) F(initialized)
              F(reviewed) F(numerical_protection) F(initial_rank) F(local_rank)
                  F(review_reasons) F(retention_reasons) F(formal_gain))
MODEL(PfsmCandidateRecallSelection,
      F(limits) F(g_star) F(initial_top) F(local_top) F(channel) F(review)
          F(protected_indices) F(proposed) F(retained) F(review_skipped)
              F(full_fallback) F(fallback_reason))
MODEL(PfsmCandidateRecallReport,
      F(algorithm_version) F(limits) F(selection) F(refinement_candidates)
          F(generation) F(baseline_version) F(pool_size) F(grid_nodes)
              F(formal_baseline_q) F(local_baseline_q) F(grid_step) F(discarded)
                  F(full_fallback) F(fallback_reason) F(candidates)
                      F(recalled_indices) F(retained_indices) F(top5_indices)
                          F(recall_top5_retained) F(final_top5_retained))
MODEL(PfsmQuantificationLibrary, F(references))
MODEL(PfsmAnalysis,
      F(query) F(problem) F(universe) F(quantification_library) F(rir_revisions)
          F(selected) F(baseline) F(generation) F(database_version) F(origin)
              F(candidate_recall_reports) F(normalization_over_full_pool)
                  F(normalization_maximum))
MODEL(ProductState,
      F(analysis_database) F(workflow) F(projectSettings) F(automaticDelta)
          F(peakTolerance) F(searchOptions) F(restraints) F(pattern)
              F(original_signal_provenance) F(comparisonPatterns) F(peaks)
                  F(candidates) F(phases) F(projectMetadata) F(preprocessing)
                      F(algorithms) F(provenance) F(pfsm) F(prepared_pfsm))
#undef MODEL
#undef F
} // namespace qualpf::infrastructure::legacy_fixture::qxp3
