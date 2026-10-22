#pragma once
// QPF 1.0 stable field inventory. Literal protocol names do not follow C++ renames.
#include "qualpf/application/pfsm.h"
#include "qualpf/application/project_document.h"
#include "qualpf/application/import/powder_cif_provenance.h"
#include "qualpf/application/workflow.h"
#include "qualpf/domain/analysis_spectrum.h"
namespace qualpf::infrastructure::qpf {
using namespace application;
using namespace preprocessing;
using namespace profile;
template <class T> struct Schema;
#define MODEL(T, KIND, BODY)                                                                                 \
    template <> struct Schema<T> {                                                                           \
        static constexpr const char *kind = KIND;                                                            \
        template <class A, class V> static void visit(A &a, V &v) { BODY }                                   \
    };
#define FIELD(KEY, NAME) a(KEY, v.NAME);
MODEL(AngularRange, "angular_range", FIELD("minimum", minimum) FIELD("maximum", maximum))
MODEL(RadiationLine, "radiation_line",
      FIELD("wavelength", wavelength) FIELD("relative_weight", relative_weight))
MODEL(Radiation, "radiation", FIELD("kind", kind) FIELD("lines", lines))
MODEL(SampleWindow, "sample_window", FIELD("first", first))
MODEL(SampledSignal, "sampled_signal", FIELD("values", values) FIELD("window", window))
MODEL(ProcessingState, "processing_state",
      FIELD("background_subtracted", background_subtracted) FIELD("kalpha2_subtracted", kalpha2_subtracted)
          FIELD("intensity_scaled", intensity_scaled))
MODEL(Pattern, "pattern",
      FIELD("id", id) FIELD("source_id", source_id) FIELD("two_theta", two_theta) FIELD("observed", observed)
          FIELD("corrected_two_theta", corrected_two_theta) FIELD("original_observed", original_observed)
              FIELD("original_kalpha2_subtracted", original_kalpha2_subtracted)
                  FIELD("background", background) FIELD("calculated", calculated)
                      FIELD("background_subtracted_signal", background_subtracted_signal)
                          FIELD("weights", weights) FIELD("radiation", radiation)
                              FIELD("calculation_range", calculation_range) FIELD("processing", processing)
                                  FIELD("intensity_scale", intensity_scale)
                                      FIELD("source_precision", source_precision))
MODEL(Peak, "peak",
      FIELD("id", id) FIELD("two_theta", two_theta) FIELD("d_spacing", d_spacing) FIELD("delta_d", delta_d)
          FIELD("intensity", intensity) FIELD("original_intensity", original_intensity) FIELD("fwhm", fwhm)
              FIELD("integrated_intensity", integrated_intensity))
MODEL(PeakSet, "peak_set",
      FIELD("dataset", dataset) FIELD("peaks", peaks) FIELD("effective_wavelength", effective_wavelength)
          FIELD("intensity_scale", intensity_scale) FIELD("source_precision", source_precision))
MODEL(AlgorithmIdentity, "algorithm_identity", FIELD("id", id) FIELD("version", version))
MODEL(AlgorithmSelections, "algorithm_selections",
      FIELD("background", background) FIELD("initial_peaks", initial_peaks) FIELD("fwhm", fwhm)
          FIELD("experimental_peaks", experimental_peaks) FIELD("matcher", matcher)
              FIELD("profile_expander", profile_expander) FIELD("profile_fitter", profile_fitter))
MODEL(AlgorithmDefaults, "algorithm_defaults", FIELD("background", background) FIELD("peaks", peaks))
MODEL(SearchParameters, "search_parameters",
      FIELD("delta_two_theta", delta_two_theta) FIELD("weight_position", weight_position)
          FIELD("weight_intensity", weight_intensity) FIELD("weight_phases", weight_phases)
              FIELD("minimum_score", minimum_score) FIELD("maximum_entries", maximum_entries)
                  FIELD("filter_strongest", filter_strongest) FIELD("strongest_count", strongest_count)
                      FIELD("include_deleted_cards", include_deleted_cards)
                          FIELD("residual_search", residual_search))
MODEL(BackgroundOptions, "background_options",
      FIELD("method", method) FIELD("automatic", automatic) FIELD("coefficients", coefficients)
          FIELD("iterations", iterations) FIELD("half_window", half_window)
              FIELD("bruckner_range", bruckner_range))
MODEL(BackgroundPoint, "background_point", FIELD("x", x) FIELD("y", y))
MODEL(BackgroundFilter, "background_filter",
      FIELD("minimum", minimum) FIELD("maximum", maximum) FIELD("iterations", iterations)
          FIELD("half_window", half_window))
MODEL(SmoothingOptions, "smoothing_options",
      FIELD("method", method) FIELD("half_window", half_window) FIELD("polynomial_degree", polynomial_degree)
          FIELD("derivative", derivative))
MODEL(PeakOptions, "peak_options",
      FIELD("threshold_percent", threshold_percent) FIELD("slope", slope) FIELD("search_range", search_range)
          FIELD("half_window", half_window) FIELD("polynomial_degree", polynomial_degree)
              FIELD("maximum_peaks", maximum_peaks) FIELD("minimum_peaks", minimum_peaks)
                  FIELD("automatic_window", automatic_window) FIELD("detection_range", detection_range))
MODEL(InitialPeakOptions, "initial_peak_options",
      FIELD("slope", slope) FIELD("automatic_window", automatic_window) FIELD("half_window", half_window)
          FIELD("polynomial_degree", polynomial_degree))
MODEL(InitialPeakCandidate, "initial_peak_candidate", FIELD("sample_index", sample_index))
MODEL(InitialPeakResult, "initial_peak_result",
      FIELD("dataset", dataset) FIELD("candidates", candidates) FIELD("applied", applied))
MODEL(InitialPeakConfiguration, "initial_peak_configuration",
      FIELD("options", options) FIELD("extensions", extensions))
MODEL(InitialPeakUsage, "initial_peak_usage",
      FIELD("options", options) FIELD("algorithm", algorithm) FIELD("source_revision", source_revision)
          FIELD("source_pattern", source_pattern) FIELD("spectrum", spectrum) FIELD("detection", detection))
MODEL(FwhmConfiguration, "fwhm_configuration",
      FIELD("method", method) FIELD("use_fixed", use_fixed) FIELD("default_fwhm", default_fwhm)
          FIELD("minimum_sampling_density", minimum_sampling_density) FIELD("extensions", extensions))
MODEL(FwhmUsage, "fwhm_usage",
      FIELD("width", width) FIELD("origin", origin) FIELD("algorithm", algorithm) FIELD("dataset", dataset)
          FIELD("source_id", source_id) FIELD("source_revision", source_revision)
              FIELD("minimum_sampling_density", minimum_sampling_density) FIELD("width_count", width_count)
                  FIELD("current", current) FIELD("manual", manual)
                      FIELD("initial_peak_options", initial_peak_options)
                          FIELD("source_pattern", source_pattern)
                              FIELD("source_original_signal", source_original_signal)
                                  FIELD("input_pattern", input_pattern) FIELD("geometric", geometric)
                                      FIELD("extensions", extensions))
MODEL(FwhmStandardDeviations, "fwhm_standard_deviations", FIELD("values", values))
MODEL(FwhmPoissonCounts, "fwhm_poisson_counts", )
MODEL(FwhmPoissonRates, "fwhm_poisson_rates", FIELD("counts", counts) FIELD("seconds", seconds))
MODEL(FwhmCovarianceEntry, "fwhm_covariance_entry",
      FIELD("first", first) FIELD("second", second) FIELD("value", value))
MODEL(FwhmCovariance, "fwhm_covariance", FIELD("diagonal", diagonal) FIELD("upper", upper))
MODEL(GeometricFwhmOptions, "geometric_fwhm_options",
      FIELD("minimum_relative_prominence", minimum_relative_prominence)
          FIELD("minimum_samples_per_fwhm", minimum_samples_per_fwhm)
              FIELD("preferred_samples_per_fwhm", preferred_samples_per_fwhm)
                  FIELD("minimum_prominence_snr", minimum_prominence_snr) FIELD("minimum_snr", minimum_snr)
                      FIELD("preferred_snr", preferred_snr) FIELD("minimum_width", minimum_width)
                          FIELD("maximum_width", maximum_width))
MODEL(FwhmCrossing, "fwhm_crossing",
      FIELD("first", first) FIELD("second", second) FIELD("position", position) FIELD("fraction", fraction))
MODEL(PeakFwhmDiagnostic, "peak_fwhm_diagnostic",
      FIELD("sample_index", sample_index) FIELD("plateau_last", plateau_last) FIELD("sampled_height",
                                                                                    sampled_height)
          FIELD("prominence", prominence) FIELD("significant", significant) FIELD("eligible", eligible)
              FIELD("rejection", rejection) FIELD("noise_state", noise_state) FIELD("apex", apex)
                  FIELD("apex_offset_degrees", apex_offset_degrees) FIELD("height", height)
                      FIELD("left", left) FIELD("right", right) FIELD("width", width)
                          FIELD("left_width", left_width) FIELD("right_width", right_width)
                              FIELD("sampling_density", sampling_density) FIELD("asymmetry", asymmetry)
                                  FIELD("snr", snr) FIELD("uncertainty", uncertainty)
                                      FIELD("relative_uncertainty", relative_uncertainty)
                                          FIELD("preferred_sampling", preferred_sampling)
                                              FIELD("preferred_snr", preferred_snr)
                                                  FIELD("conditional_uncertainty", conditional_uncertainty))
MODEL(GeometricFwhmResult, "geometric_fwhm_result",
      FIELD("algorithm", algorithm) FIELD("numerical_policy", numerical_policy) FIELD("dataset", dataset)
          FIELD("source_id", source_id) FIELD("radiation", radiation) FIELD("processing", processing)
              FIELD("noise_model", noise_model) FIELD("noise_source", noise_source) FIELD("applied", applied)
                  FIELD("state", state) FIELD("candidates", candidates)
                      FIELD("selected_candidate", selected_candidate) FIELD("default_fwhm", default_fwhm))
MODEL(FwhmEstimate, "fwhm_estimate",
      FIELD("state", state) FIELD("width", width) FIELD("peak_count", peak_count))
MODEL(PeakCalibration, "peak_calibration",
      FIELD("applied", applied) FIELD("estimated_fwhm", estimated_fwhm)
          FIELD("formal_search_range", formal_search_range) FIELD("width_count", width_count)
              FIELD("source", source))
MODEL(RejectedPeakCandidate, "rejected_peak_candidate",
      FIELD("stage", stage) FIELD("candidate_index", candidate_index) FIELD("sample_index", sample_index)
          FIELD("two_theta", two_theta) FIELD("reason", reason))
MODEL(PeakSelection, "peak_selection",
      FIELD("minimum", minimum) FIELD("maximum", maximum) FIELD("threshold_percent", threshold_percent)
          FIELD("maximum_peaks", maximum_peaks) FIELD("append", append))
MODEL(PreprocessingConfiguration, "preprocessing_configuration",
      FIELD("initial_peaks", initial_peaks) FIELD("initial_peak_usage", initial_peak_usage)
          FIELD("fwhm", fwhm) FIELD("fwhm_usage", fwhm_usage) FIELD("peak_fwhm_usage", peak_fwhm_usage)
              FIELD("extensions", extensions) FIELD("background", background) FIELD("smoothing", smoothing)
                  FIELD("peaks", peaks) FIELD("selection", selection)
                      FIELD("peak_candidates", peak_candidates) FIELD("candidate_failure", candidate_failure)
                          FIELD("smoothing_applied", smoothing_applied)
                              FIELD("background_points", background_points)
                                  FIELD("background_filters", background_filters)
                                      FIELD("peak_calibration", peak_calibration)
                                          FIELD("fwhm_estimate", fwhm_estimate)
                                              FIELD("rejected_candidates", rejected_candidates))
MODEL(OriginalSignalProvenance, "original_signal_provenance",
      FIELD("source", source) FIELD("intensity_units", intensity_units) FIELD("unclipped", unclipped)
          FIELD("intensity_scaled", intensity_scaled) FIELD("standard_deviations", standard_deviations)
              FIELD("raw_poisson_counts", raw_poisson_counts) FIELD("noise_source", noise_source))
MODEL(CsmSettings, "csm_settings",
      FIELD("minimum_fom", minimum_fom) FIELD("weight_position", weight_position)
          FIELD("weight_intensity", weight_intensity) FIELD("weight_phases", weight_phases)
              FIELD("delta", delta) FIELD("automatic_delta", automatic_delta) FIELD("residual", residual)
                  FIELD("strongest", strongest) FIELD("exclude_deleted", exclude_deleted)
                      FIELD("maximum_entries", maximum_entries))
MODEL(PfsmSettings, "pfsm_settings",
      a.extension("candidate_recall_resource_fallback", v.candidate_recall_resource_fallback);
      FIELD("lower_shift", lower_shift) FIELD("upper_shift", upper_shift)
          FIELD("minimum_relative_fom", minimum_relative_fom) FIELD("weighting", weighting)
              FIELD("threads", threads) FIELD("template_cache", template_cache)
                  FIELD("candidate_recall_enabled", candidate_recall_enabled)
                      FIELD("candidate_recall_limits", candidate_recall_limits))
MODEL(WorkflowProfile, "workflow_profile",
      FIELD("background", background) FIELD("initial_peaks", initial_peaks) FIELD("fwhm", fwhm)
          FIELD("peaks", peaks) FIELD("selection", selection) FIELD("smoothing", smoothing)
              FIELD("range", range) FIELD("csm", csm) FIELD("pfsm", pfsm) FIELD("extensions", extensions))
MODEL(WorkflowTarget, "workflow_target", FIELD("workflow", workflow) FIELD("step", step))
MODEL(WorkflowActivity, "workflow_activity",
      FIELD("target", target) FIELD("status", status) FIELD("detail", detail))
MODEL(WorkflowRecord, "workflow_record",
      FIELD("id", id) FIELD("workflow", workflow) FIELD("step", step) FIELD("status", status)
          FIELD("manual", manual) FIELD("dependencies", dependencies) FIELD("parameters", parameters)
              FIELD("actual_parameters", actual_parameters) FIELD("algorithms", algorithms)
                  FIELD("input", input) FIELD("detail", detail) FIELD("extensions", extensions))
MODEL(WorkflowState, "workflow_state",
      FIELD("active", active) FIELD("imported_pattern", imported_pattern) FIELD("records", records)
          FIELD("next_id", next_id) FIELD("last_attempt", last_attempt))
MODEL(AlgorithmRunSource, "algorithm_run_source",
      FIELD("algorithms", algorithms) FIELD("parameters", parameters) FIELD("source_revision", sourceRevision)
          FIELD("background", background) FIELD("peaks", peaks) FIELD("fwhm", fwhm))
MODEL(CardAlgorithmSource, "card_algorithm_source", FIELD("score", score) FIELD("scale", scale))
MODEL(AnalysisProvenance, "analysis_provenance",
      FIELD("legacy_unverified", legacyUnverified) FIELD("residual_history", residualHistory)
          FIELD("background_source", backgroundSource) FIELD("peak_source", peakSource)
              FIELD("retained_peak_sources", retainedPeakSources)
                  FIELD("subtracted_background_history", subtractedBackgroundHistory))
MODEL(ProductCard, "product_card",
      FIELD("id", id) FIELD("chemical_name", chemicalName) FIELD("chemical_formula", chemicalFormula)
          FIELD("mineral_name", mineralName) FIELD("quality", quality) FIELD("rir", rir)
              FIELD("space_group", spaceGroup) FIELD("reference_rir", referenceRir) FIELD("fom", fom)
                  FIELD("fom_peak_pos", fomPeakPos) FIELD("fom_intensity", fomIntensity) FIELD("scale", scale)
                      FIELD("fom_calculated", fomCalculated) FIELD("d", d) FIELD("tth", tth)
                          FIELD("intensity", intensity) FIELD("algorithm_source", algorithmSource)
                              FIELD("pfsm", pfsm))
MODEL(ProductPeaks, "product_peaks",
      FIELD("d", d) FIELD("delta_d", deltaD) FIELD("tth", tth) FIELD("intensity", intensity)
          FIELD("intensity_orig", intensityOrig) FIELD("fwhm", fwhm)
              FIELD("integrated_intensity", integratedIntensity) FIELD("wave", wave) FIELD("valid", valid))
MODEL(CandidateQuery, "candidate_query",
      FIELD("route", route) FIELD("element_operator", elementOperator) FIELD("names", names)
          FIELD("elements", elements) FIELD("minimum_species", minimumSpecies)
              FIELD("maximum_species", maximumSpecies) FIELD("subfiles", subfiles)
                  FIELD("crystal_systems", crystalSystems) FIELD("space_groups", spaceGroups)
                      FIELD("colors", colors) FIELD("ids", ids) FIELD("cell_minimum", cellMinimum)
                          FIELD("cell_maximum", cellMaximum) FIELD("calculated_density_minimum",
                                                                   calculatedDensityMinimum)
                              FIELD("calculated_density_maximum", calculatedDensityMaximum)
                                  FIELD("measured_density_minimum", measuredDensityMinimum)
                                      FIELD("measured_density_maximum", measuredDensityMaximum) FIELD("d", d)
                                          FIELD("delta_d", deltaD) FIELD("wavelength", wavelength)
                                              FIELD("exclude_deleted", excludeDeleted)
                                                  FIELD("decode_reflections", decodeReflections)
                                                      FIELD("print", print) FIELD("strict_paired_reflections",
                                                                                  strictPairedReflections)
                                                          FIELD("preselected_ids", preselectedIds))
MODEL(ProfileFitOptions, "profile_fit_options",
      FIELD("ftol", ftol) FIELD("xtol", xtol) FIELD("gtol", gtol)
          FIELD("maximum_evaluations", maximum_evaluations)
              FIELD("maximum_nnls_iterations", maximum_nnls_iterations))
MODEL(ProfileObservations, "profile_observations",
      FIELD("coordinates", coordinates) FIELD("observed", observed) FIELD("background", background)
          FIELD("weights", weights))
MODEL(ProfileFitParameters, "profile_fit_parameters", FIELD("scales", scales) FIELD("shifts", shifts))
MODEL(ProfileFitDiagnostics, "profile_fit_diagnostics",
      FIELD("residual_evaluations", residual_evaluations)
          FIELD("difference_evaluations", difference_evaluations) FIELD("nnls_iterations", nnls_iterations)
              FIELD("starts", starts) FIELD("rank", rank) FIELD("kkt_violation", kkt_violation)
                  FIELD("optimality", optimality) FIELD("rank_deficient", rank_deficient)
                      FIELD("kkt_checked", kkt_checked)
                          FIELD("interpolated_iterations", interpolated_iterations)
                              FIELD("exact_final_verification", exact_final_verification)
                                  FIELD("zero_templates", zero_templates)
                                      FIELD("unidentifiable_shifts", unidentifiable_shifts)
                                          FIELD("at_bound", at_bound))
MODEL(ProfileFitResult, "profile_fit_result",
      FIELD("parameters", parameters) FIELD("calculated", calculated) FIELD("residual", residual)
          FIELD("q", q) FIELD("denominator_net", denominator_net)
              FIELD("denominator_observed", denominator_observed) FIELD("rwp_net", rwp_net)
                  FIELD("rwp_observed", rwp_observed) FIELD("termination", termination)
                      FIELD("diagnostics", diagnostics))
MODEL(ProfileFitPhase, "profile_fit_phase",
      FIELD("identity", identity) FIELD("lower_shift", lower_shift) FIELD("upper_shift", upper_shift)
          FIELD("initial_shift", initial_shift))
MODEL(PfsmObservationRequest, "pfsm_observation_request",
      FIELD("signal", signal) FIELD("signal_source", signal_source) FIELD("intensity_units", intensity_units)
          FIELD("noise_source", noise_source) FIELD("unclipped_signal", unclipped_signal)
              FIELD("equal_weights", equal_weights)
                  FIELD("original_intensity_scaled", original_intensity_scaled) FIELD("noise", noise)
                      FIELD("radiation", radiation) FIELD("background", background) FIELD("valid", valid)
                          FIELD("saturated", saturated) FIELD("break_before", break_before)
                              FIELD("width_options", width_options))
MODEL(PfsmCandidateRecallLimits, "pfsm_candidate_recall_limits",
      FIELD("recall_candidates", recall_candidates) FIELD("local_shortlist", local_shortlist)
          FIELD("protected_per_channel", protected_per_channel) FIELD("review_limit", review_limit)
              FIELD("formal_limit", formal_limit))
MODEL(PfsmCandidateRecallConfiguration, "pfsm_candidate_recall_configuration",
      a.extension("resource_fallback", v.resource_fallback);
      FIELD("mode", mode) FIELD("version", version) FIELD("limits", limits))
MODEL(PfsmProblem, "pfsm_problem",
      FIELD("background_points", background_points) FIELD("observations", observations)
          FIELD("radiation", radiation) FIELD("source", source)
              FIELD("original_signal_provenance", original_signal_provenance) FIELD("width", width)
                  FIELD("physical_width", physical_width) FIELD("algorithms", algorithms)
                      FIELD("lower_shift", lower_shift) FIELD("upper_shift", upper_shift) FIELD("fit", fit)
                          FIELD("template_cache", template_cache) FIELD("candidate_recall", candidate_recall))
MODEL(PfsmCandidateEvidence, "pfsm_candidate_evidence",
      FIELD("original_index", original_index) FIELD("generation", generation) FIELD("parameters", parameters)
          FIELD("diagnostics", diagnostics) FIELD("termination", termination) FIELD("q", q)
              FIELD("rwp_net", rwp_net) FIELD("delta_q", delta_q) FIELD("delta_rwp", delta_rwp))
MODEL(PfsmQuantificationReference, "pfsm_quantification_reference",
      FIELD("maximum_intensity", maximum_intensity) FIELD("rir", rir) FIELD("original_rir", original_rir))
MODEL(PfsmRirRevision, "pfsm_rir_revision",
      FIELD("value", value) FIELD("source", source) FIELD("radiation", radiation))
MODEL(PfsmCandidateRecallEvidence, "pfsm_candidate_recall_evidence",
      FIELD("original_index", original_index) FIELD("original_score", original_score)
          FIELD("residual_score", residual_score) FIELD("local_q", local_q) FIELD("gain", gain)
              FIELD("original_rank", original_rank) FIELD("residual_rank", residual_rank)
                  FIELD("local_rank", local_rank) FIELD("original_shift", original_shift)
                      FIELD("residual_shift", residual_shift) FIELD("original_seeds", original_seeds)
                          FIELD("residual_seeds", residual_seeds) FIELD("nnls_iterations", nnls_iterations)
                              FIELD("recalled", recalled) FIELD("retained", retained)
                                  FIELD("recall_reason", recall_reason)
                                      FIELD("retention_reason", retention_reason)
                                          FIELD("protection_reason", protection_reason))
MODEL(PfsmCandidateRecallPhysicalFit, "pfsm_candidate_recall_physical_fit",
      FIELD("parameters", parameters) FIELD("q", q) FIELD("gain", gain)
          FIELD("difference_gain", difference_gain) FIELD("kkt", kkt)
              FIELD("baseline_fallback", baseline_fallback) FIELD("approximated", approximated))
MODEL(PfsmCandidateRecallStep, "pfsm_candidate_recall_step",
      FIELD("anchor", anchor) FIELD("linear_coefficients", linear_coefficients) FIELD("physical", physical)
          FIELD("predicted", predicted) FIELD("actual", actual) FIELD("distortion", distortion)
              FIELD("radius", radius) FIELD("ratio", ratio) FIELD("accepted", accepted)
                  FIELD("indistinguishable", indistinguishable) FIELD("local_boundary", local_boundary))
MODEL(PfsmCandidateRecallSeed, "pfsm_candidate_recall_seed",
      FIELD("initial_shift", initial_shift) FIELD("radius", radius) FIELD("initial_fit", initial_fit)
          FIELD("current", current) FIELD("steps", steps) FIELD("initialized", initialized))
MODEL(PfsmCandidateRecallCandidate, "pfsm_candidate_recall_candidate",
      FIELD("recall", recall) FIELD("best", best) FIELD("seeds", seeds) FIELD("starts", starts)
          FIELD("initial_gain", initial_gain) FIELD("initial_eta", initial_eta) FIELD("eta", eta)
              FIELD("maximum_distortion", maximum_distortion) FIELD("nnls_calls", nnls_calls)
                  FIELD("nnls_iterations", nnls_iterations) FIELD("initialized",
                                                                  initialized) FIELD("reviewed", reviewed)
                      FIELD("numerical_protection", numerical_protection) FIELD("initial_rank", initial_rank)
                          FIELD("local_rank", local_rank) FIELD("review_reasons", review_reasons)
                              FIELD("retention_reasons", retention_reasons) FIELD("formal_gain", formal_gain))
MODEL(PfsmCandidateRecallSelection, "pfsm_candidate_recall_selection",
      FIELD("limits", limits) FIELD("g_star", g_star) FIELD("initial_top", initial_top)
          FIELD("local_top", local_top) FIELD("channel", channel) FIELD("review", review)
              FIELD("protected_indices", protected_indices) FIELD("proposed", proposed)
                  FIELD("retained", retained) FIELD("review_skipped", review_skipped)
                      FIELD("full_fallback", full_fallback) FIELD("fallback_reason", fallback_reason))
MODEL(PfsmCandidateRecallReport, "pfsm_candidate_recall_report",
      a.extension("backend", v.backend); a.extension("switch_reason", v.switch_reason);
      a.extension("integral", v.integral);
      FIELD("algorithm_version", algorithm_version) FIELD("limits", limits) FIELD("selection", selection)
          FIELD("refinement_candidates", refinement_candidates) FIELD("generation", generation)
              FIELD("baseline_version", baseline_version) FIELD("pool_size", pool_size)
                  FIELD("grid_nodes", grid_nodes) FIELD("formal_baseline_q", formal_baseline_q)
                      FIELD("local_baseline_q", local_baseline_q) FIELD("grid_step", grid_step)
                          FIELD("discarded", discarded) FIELD("full_fallback", full_fallback)
                              FIELD("fallback_reason", fallback_reason) FIELD("candidates", candidates)
                                  FIELD("recalled_indices", recalled_indices)
                                      FIELD("retained_indices", retained_indices)
                                          FIELD("top5_indices", top5_indices)
                                              FIELD("recall_top5_retained", recall_top5_retained)
                                                  FIELD("final_top5_retained", final_top5_retained))
MODEL(PfsmIntegralOptions, "pfsm_integral_options",
      FIELD("version", version) FIELD("fine_bins", fine_bins) FIELD("coarse_positions", coarse_positions)
      FIELD("neighborhoods", neighborhoods) FIELD("fine_positions", fine_positions) FIELD("fine_budget", fine_budget)
      FIELD("coarse_budget", coarse_budget) FIELD("original_budget", original_budget) FIELD("residual_budget", residual_budget)
      FIELD("tail_mass_epsilon", tail_mass_epsilon))
MODEL(PfsmRelaxedOptions, "pfsm_relaxed_options",
      FIELD("version", version) FIELD("radius_widths", radius_widths) FIELD("maximum_span_fraction", maximum_span_fraction)
      FIELD("old_coordinates", old_coordinates) FIELD("bank_positions", bank_positions) FIELD("starts", starts)
      FIELD("stages", stages) FIELD("maximum_evaluations", maximum_evaluations) FIELD("candidate_cache", candidate_cache)
      FIELD("budget", budget))
MODEL(PfsmSupportEvidence, "pfsm_integral_support",
      FIELD("score", score) FIELD("shift", shift) FIELD("visible_fraction", visible_fraction)
      FIELD("edge_fraction", edge_fraction) FIELD("visible_groups", visible_groups) FIELD("uncertain", uncertain))
MODEL(PfsmRelaxedPoint, "pfsm_relaxed_point", FIELD("q", q) FIELD("shifts", shifts) FIELD("scales", scales))
MODEL(PfsmRelaxedEvidence, "pfsm_relaxed_evidence",
      FIELD("best", best) FIELD("null", null) FIELD("fixed_q", fixed_q) FIELD("gain", gain) FIELD("kkt", kkt)
      FIELD("released", released) FIELD("evaluations", evaluations) FIELD("iterations", iterations)
      FIELD("cache_columns", cache_columns) FIELD("cache_bytes", cache_bytes) FIELD("buffer_bytes", buffer_bytes)
      FIELD("uncertainty", uncertainty) FIELD("complete", complete) FIELD("selected", selected))
MODEL(PfsmScreenEvidence, "pfsm_integral_evidence",
      FIELD("original_index", original_index) FIELD("gain", gain) FIELD("normalized_gain", normalized_gain)
      FIELD("coarse_shift", coarse_shift) FIELD("original", original) FIELD("residual", residual)
      FIELD("uncertainty", uncertainty) FIELD("nnls_iterations", nnls_iterations) FIELD("scan_evaluations", scan_evaluations)
      FIELD("main_selected", main_selected) FIELD("original_selected", original_selected)
      FIELD("residual_selected", residual_selected) FIELD("retained", retained)
      FIELD("fine_gain", fine_gain) FIELD("normalized_fine_gain", normalized_fine_gain) FIELD("fine_shift", fine_shift)
      FIELD("coarse_evaluations", coarse_evaluations) FIELD("fine_evaluations", fine_evaluations)
      FIELD("buffer_bytes", buffer_bytes) FIELD("coarse_selected", coarse_selected)
      FIELD("integral_complete", integral_complete) FIELD("relaxed", relaxed))
MODEL(PfsmIntegralRecallReport, "pfsm_integral_recall_report",
      FIELD("baseline_shifts", baseline_shifts)
      FIELD("options", options) FIELD("relaxed_options", relaxed_options)
      FIELD("fine_bins", fine_bins) FIELD("coarse_bins", coarse_bins) FIELD("bank_columns", bank_columns)
      FIELD("peak_buffer_bytes", peak_buffer_bytes) FIELD("coarse_evaluations", coarse_evaluations)
      FIELD("fine_evaluations", fine_evaluations) FIELD("relaxed_evaluations", relaxed_evaluations)
      FIELD("coarse_baseline_q", coarse_baseline_q) FIELD("fine_baseline_q", fine_baseline_q)
      FIELD("fine_denominator", fine_denominator) FIELD("tau", tau) FIELD("relaxation_radius", relaxation_radius)
      FIELD("shared_seconds", shared_seconds) FIELD("integral_seconds", integral_seconds)
      FIELD("relaxed_seconds", relaxed_seconds) FIELD("candidates", candidates))
MODEL(PfsmQuantificationLibrary, "pfsm_quantification_library", FIELD("references", references))
MODEL(PfsmAnalysis, "pfsm_analysis",
      FIELD("query", query) FIELD("problem", problem) FIELD("universe", universe)
          FIELD("quantification_library", quantification_library) FIELD("rir_revisions", rir_revisions)
              FIELD("selected", selected) FIELD("baseline", baseline) FIELD("generation", generation)
                  FIELD("origin", origin) FIELD("candidate_recall_reports", candidate_recall_reports)
                      FIELD("normalization_over_full_pool", normalization_over_full_pool)
                          FIELD("normalization_maximum", normalization_maximum))
MODEL(ProductState, "product_state",
      FIELD("analysis_database", analysis_database) FIELD("workflow", workflow) FIELD("project_settings",
                                                                                      projectSettings)
          FIELD("automatic_delta", automaticDelta) FIELD("peak_tolerance", peakTolerance)
              FIELD("search_options", searchOptions) FIELD("restraints", restraints) FIELD("pattern", pattern)
                  FIELD("original_signal_provenance", original_signal_provenance)
                      FIELD("comparison_patterns", comparisonPatterns) FIELD("peaks", peaks)
                          FIELD("candidates", candidates) FIELD("phases", phases)
                              FIELD("project_metadata", projectMetadata) FIELD("preprocessing", preprocessing)
                                  FIELD("algorithms", algorithms) FIELD("provenance", provenance)
                                      FIELD("powder_cif_provenance", powderCifProvenance) FIELD("pfsm", pfsm)
                                          FIELD("prepared_pfsm", prepared_pfsm))

MODEL(ProjectSettings, "project_settings",
      FIELD("preprocessing", profile(WorkflowId::preprocessing)) FIELD("csm", profile(WorkflowId::csm))
          FIELD("pfsm", profile(WorkflowId::pfsm)) FIELD("peak_search", profile(WorkflowId::peak_search))
              FIELD("extensions", extensions))
MODEL(ProjectPen, "pen",
      FIELD("rgba", rgba) FIELD("width", width) FIELD("style", style) FIELD("dash_pattern", dash_pattern)
          FIELD("cap", cap) FIELD("join", join) FIELD("cosmetic", cosmetic))
MODEL(ProjectCurveStyle, "curve_style",
      FIELD("pen", pen) FIELD("scatter_pen", scatter_pen) FIELD("scatter_rgba", scatter_rgba)
          FIELD("scatter_fill", scatter_fill) FIELD("scatter_pen_defined", scatter_pen_defined)
              FIELD("scatter", scatter) FIELD("connection", connection) FIELD("scatter_size", scatter_size)
                  FIELD("visible", visible) FIELD("label", label))
MODEL(ProjectViewState, "project_view",
      FIELD("axis", axis) FIELD("y_scale", y_scale) FIELD("bounds", bounds)
          FIELD("subtract_background", subtract_background) FIELD("offset_enabled", offset_enabled)
              FIELD("legend", legend) FIELD("x_tick_labels", x_tick_labels)
                  FIELD("y_tick_labels", y_tick_labels) FIELD("offset", offset)
                      FIELD("background_style", background_style) FIELD("layer_style", layer_style)
                          FIELD("background_colors", background_colors) FIELD("layer_colors", layer_colors)
                              FIELD("pens", pens) FIELD("curves", curves) FIELD("layers", layers)
                                  FIELD("card_colors", card_colors) FIELD("selected_cards", selected_cards)
                                      FIELD("result_order", result_order))
MODEL(io::powder_cif::SourceSpan, "source_span", FIELD("begin", begin) FIELD("end", end))
MODEL(io::cif::Location, "source_location",
      FIELD("offset", offset) FIELD("line", line) FIELD("column", column))
// SourceArtifact: source_id, sha256, source_bytes (raw u8 array); see qpf_archive.h.
MODEL(io::powder_cif::LocatedField, "located_field",
      FIELD("tag", tag) FIELD("location", location) FIELD("span", span) FIELD("row", row))
MODEL(io::powder_cif::Notice, "import_notice",
      FIELD("reason", reason) FIELD("tag", tag) FIELD("span", span) FIELD("block_index", block_index))
MODEL(io::powder_cif::RadiationProvenance, "radiation_provenance",
      FIELD("origin", origin) FIELD("kind", kind) FIELD("wavelength_bits", wavelength_bits)
          FIELD("source_field", source_field))
MODEL(PowderCifProvenance, "powder_cif_provenance",
      FIELD("source", source) FIELD("fragment", fragment) FIELD("block_id", block_id)
          FIELD("block_index", block_index) FIELD("block_span", block_span) FIELD("axis_source", axis_source)
              FIELD("intensity_source", intensity_source) FIELD("axis_fields", axis_fields)
                  FIELD("intensity_field", intensity_field) FIELD("radiation", radiation)
                      FIELD("reversed", reversed) FIELD("notices", notices))

#undef MODEL
#undef FIELD
} // namespace qualpf::infrastructure::qpf
