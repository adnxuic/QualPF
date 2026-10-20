#include "qualpf/preprocessing/kalpha2.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/preprocessing_workflows.h"
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/workflow.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace qualpf::application {
bool hasActiveBackground(const Pattern &pattern) {
    if (!pattern.background || pattern.background->values.empty())
        return false;
    const auto &background = *pattern.background;
    const bool subtractedZero =
        pattern.processing.background_subtracted.value_or(false) && background.window &&
        background.window->first == 0 && background.values.size() == pattern.observed.size() &&
        std::all_of(background.values.begin(), background.values.end(), [](double v) { return v == 0; });
    return !subtractedZero;
}

Status PreprocessingEvaluation::evaluate(const PreprocessingRequest &request, const ExecutionContext &context) {
    if (!source_)
        return Status{Error{ErrorCode::missing_data, "preprocessing.source", "Missing source"}};
    const auto algorithms = request.algorithms ? request.algorithms : source_->algorithms;
    if (algorithms != source_->algorithms)
        return Status{Error{ErrorCode::stale_snapshot, "algorithms", "Preprocessing algorithm selection changed"}};
    auto previousPeaks = draft_ ? draft_->peaks : source_->peaks;
    auto previousParameters = draft_ ? draft_->preprocessing.peaks : source_->preprocessing.peaks;
    candidateCount_ = 0;
    fwhmEvaluation_.reset();
    draft_.reset(); // A failed/new parameter request invalidates the previous
                    // preview.
    auto next = *source_;
    // Inspection and selection operate on the saved measurements. Preparing
    // upstream workflow steps here would replace the pool they are selecting.
    const bool frozenPeaks = request.peak_product_operation && request.peak_operation != PeakOperation::redetect;
    if (source_->projectSettings && request.workflow && !frozenPeaks) {
        auto preparation = prepareWorkflow(source_, *request.workflow,
            request.peaks ? WorkflowStep::peaks : WorkflowStep::background);
        if (!preparation) return Status{preparation.error()};
        next = std::move(preparation).value().next;
        next.workflow.configuration_transition = false;
    }
    next.workflow.active = request.workflow.value_or(WorkflowId::preprocessing);
    if (request.initial_peaks) next.preprocessing.initial_peaks = request.initial_peaks;
    if (request.peak_product_operation &&
        (!request.peaks || request.smoothing || request.background_points ||
         !request.background_filters.empty() || request.crop_range || request.strip_kalpha2 ||
         request.restart_from_original))
        return Status{Error{ErrorCode::invalid_input, "peaks.operation",
                            "Peak preparation cannot include an independent pattern edit"}};
    const int peakChanges = int(request.manual.has_value()) + int(request.table.has_value()) +
                            int(request.inferred_graph.has_value()) + int(request.imported.has_value());
    if (peakChanges) {
        if (peakChanges != 1 || request.strip_kalpha2 || request.restart_from_original ||
            request.crop_range || request.background || request.smoothing || request.peaks ||
            request.selection || request.background_points || !request.background_filters.empty() ||
            request.peak_operation != PeakOperation::redetect)
            return Status{Error{ErrorCode::invalid_input, "preprocessing",
                                "Peak editing must be a separate operation"}};
        auto status = preparePeakOperation(next, request, context);
        if (!status)
            return status;
        if (!peakMatchingEquivalent(next, *source_) || next.peaks.integratedIntensity != source_->peaks.integratedIntensity) {
            if (next.provenance.peakSource) next.provenance.retainedPeakSources.push_back(*next.provenance.peakSource);
            next.provenance.peakSource = AlgorithmRunSource{algorithms, SearchParameters{AngularWidthDegrees{0}}, source_, source_->revision};
        }
        if (next.projectSettings && next.peaks != source_->peaks) next.workflow.completed_target = WorkflowStep::peaks;
        detected_.reset();
        detectedOptions_.reset();
        detectionInput_.reset();
        draft_ = std::move(next);
        return Status{std::monostate{}};
    }
    if (request.strip_kalpha2) {
        if (request.manual || request.restart_from_original || request.crop_range || request.background ||
            request.smoothing || request.peaks || request.selection || request.background_points ||
            !request.background_filters.empty() || request.restore_automatic_window ||
            request.peak_operation != PeakOperation::redetect)
            return Status{Error{ErrorCode::invalid_input, "preprocessing",
                                "Stripping must be a separate operation", "kalpha2_mixed_request"}};
        if (next.projectMetadata.contains("synchrotron") &&
            std::get_if<bool>(&next.projectMetadata.at("synchrotron").data) &&
            std::get<bool>(next.projectMetadata.at("synchrotron").data))
            return Status{Error{ErrorCode::unsupported, "pattern.radiation",
                                "Synchrotron data is unsupported by the Cu model",
                                "unsupported_kalpha2_radiation"}};
        // Reject repeat/unknown history through the existing validator before
        // entering either selected numerical evaluator.
        if (next.pattern.processing.kalpha2_subtracted != false)
            return preprocessing::validate_kalpha2(next.pattern);
        auto result = stripper_(next.pattern);
        if (!result)
            return Status{result.error()};
        const auto &stripped = result.value();
        const auto &opening = source_->pattern.observed;
        const bool unchanged =
            stripped.size() == opening.size() &&
            std::equal(stripped.begin(), stripped.end(), opening.begin(), [](double final, double original) {
                return std::isfinite(final) && std::isfinite(original) && final == original;
            });
        next.pattern.processing.kalpha2_subtracted = true;
        // Preserve the opening representation and dependent state for an exact
        // numerical no-op, including numerically equal signed zeros.
        if (!unchanged) {
            invalidateAutomaticFwhm(next.preprocessing);
            next.preprocessing.peak_fwhm_usage.reset();
            next.pattern.observed = std::move(result).value();
            next.pattern.background.reset();
            next.pattern.calculated.reset();
            next.pattern.background_subtracted_signal.reset();
            next.pattern.weights.reset();
            next.preprocessing.background_points.reset();
            next.preprocessing.background_filters.clear();
            next.preprocessing.peak_candidates.reset();
            next.preprocessing.peak_calibration.reset();
            next.preprocessing.fwhm_estimate.reset();
            next.preprocessing.rejected_candidates.clear();
            next.preprocessing.candidate_failure.reset();
            next.preprocessing.selection.reset();
            next.peaks.clear();
            next.candidates.clear();
            next.phases.clear();
            next.peakTolerance.reset();
            next.automaticDelta = .5;
            next.projectMetadata["has_back"] = false;
            next.projectMetadata["candidate_pool_state"] = "invalidated_by_kalpha2";
        }
        auto status = validateProductState(next);
        if (!status)
            return status;
        detected_.reset();
        detectedOptions_.reset();
        detectionInput_.reset();
        draft_ = std::move(next);
        return Status{std::monostate{}};
    }
    if (request.manual || request.restart_from_original || request.crop_range) {
        if ((int(request.manual.has_value()) + int(request.restart_from_original) +
                 int(request.crop_range.has_value()) !=
             1) ||
            request.background || request.smoothing || request.peaks || request.selection ||
            request.background_points || !request.background_filters.empty() || request.peak_operation != PeakOperation::redetect)
            return Status{Error{ErrorCode::invalid_input, "preprocessing",
                                "Recovery/manual edit must be a separate operation"}};
        if (request.manual) {
            auto status = prepareManualPeaks(next, *request.manual, request.wavelength, request.tolerance);
            if (!status)
                return status;
        } else if (request.crop_range) {
            invalidateAutomaticFwhm(next.preprocessing);
            next.preprocessing.peak_fwhm_usage.reset();
            auto &p = next.pattern;
            const auto range = *request.crop_range;
            if (p.two_theta.empty() || !std::isfinite(range.minimum.value) ||
                !std::isfinite(range.maximum.value) || range.minimum.value >= range.maximum.value)
                return Status{Error{ErrorCode::invalid_input, "pattern.range", "Invalid calculation window"}};
            auto begin = std::lower_bound(p.two_theta.begin(), p.two_theta.end(), float(range.minimum.value),
                                          [](auto x, float v) { return float(x.value) < v; });
            auto end = std::upper_bound(p.two_theta.begin(), p.two_theta.end(), float(range.maximum.value),
                                        [](float v, auto x) { return v < float(x.value); });
            const auto first = std::size_t(begin - p.two_theta.begin()),
                       last = std::size_t(end - p.two_theta.begin());
            if (last <= first + 1)
                return Status{Error{ErrorCode::invalid_input, "pattern.range",
                                    "At least two samples must remain", "empty_calculation_window"}};
            auto signal = [&](std::optional<SampledSignal> &s) -> bool {
                if (!s)
                    return true;
                if (!s->window)
                    return false;
                const auto lo = std::max(first, s->window->first),
                           hi = std::min(last, s->window->first + s->values.size());
                if (lo >= hi) {
                    s.reset();
                    return true;
                }
                s->values = std::vector<double>(s->values.begin() + lo - s->window->first,
                                                s->values.begin() + hi - s->window->first);
                s->window->first = lo - first;
                return true;
            };
            if (!signal(p.background) || !signal(p.calculated) || !signal(p.background_subtracted_signal))
                return Status{
                    Error{ErrorCode::missing_data, "pattern.range",
                          "Cannot crop a signal with unknown alignment. Restart from the original input.",
                          "background_alignment_unknown"}};
            auto slice = [first, last](auto &v) {
                v = std::decay_t<decltype(v)>(v.begin() + first, v.begin() + last);
            };
            slice(p.two_theta);
            slice(p.observed);
            if (p.original_observed)
                slice(*p.original_observed);
            if (p.corrected_two_theta)
                slice(*p.corrected_two_theta);
            if (p.weights)
                slice(*p.weights);
            p.calculation_range = AngularRange{p.two_theta.front(), p.two_theta.back()};
            ManualPeakChange removal;
            removal.operation = ManualPeakChange::Operation::erase;
            for (int i = 0; i < next.peaks.tth.size(); ++i)
                if (next.peaks.tth[i] < p.two_theta.front().value ||
                    next.peaks.tth[i] > p.two_theta.back().value)
                    removal.rows.push_back(std::size_t(i));
            if (!removal.rows.empty()) {
                auto status = prepareManualPeaks(next, removal, request.wavelength, request.tolerance);
                if (!status)
                    return status;
            }
            next.preprocessing.peak_candidates.reset();
            next.preprocessing.peak_calibration.reset();
            next.preprocessing.fwhm_estimate.reset();
            next.preprocessing.rejected_candidates.clear();
            next.preprocessing.candidate_failure.reset();
            next.preprocessing.background_points.reset();
            next.preprocessing.background_filters.clear();
            next.preprocessing.selection.reset();
            next.projectMetadata["candidate_pool_state"] = "invalidated_by_range_edit";
            next.candidates.clear();
            next.phases.clear();
        } else {
            if (!next.pattern.original_observed)
                return Status{Error{ErrorCode::missing_data, "pattern.original_observed",
                                    "Original intensity is unavailable. Import the original diffraction file "
                                    "to start a new analysis.",
                                    "raw_intensity_unavailable"}};
            next.pattern.observed = *next.pattern.original_observed;
            next.pattern.background.reset();
            next.pattern.calculated.reset();
            next.pattern.background_subtracted_signal.reset();
            next.pattern.intensity_scale = IntensityScale::counts;
            next.pattern.processing.background_subtracted = false;
            next.pattern.processing.intensity_scaled = false;
            next.pattern.processing.kalpha2_subtracted = next.pattern.original_kalpha2_subtracted;
            const auto fwhm = next.preprocessing.fwhm;
            const auto usage = next.preprocessing.fwhm_usage;
            const auto initial = next.preprocessing.initial_peaks;
            const auto extensions = next.preprocessing.extensions;
            next.preprocessing = {};
            next.preprocessing.fwhm = fwhm;
            next.preprocessing.fwhm_usage = usage;
            next.preprocessing.initial_peaks = initial;
            next.preprocessing.extensions = extensions;
            invalidateAutomaticFwhm(next.preprocessing);
            next.preprocessing.smoothing_applied = false;
            next.peaks.clear();
            next.candidates.clear();
            next.phases.clear();
            next.peakTolerance.reset();
            next.automaticDelta = .5;
            next.projectMetadata["preprocessing_recovery"] = "restart_from_stored_original";
            next.provenance = {}; // Explicit controlled restart, after restoring the stored original signal.
            next.projectMetadata["has_back"] = false;
        }
        auto status = validateProductState(next);
        if (!status)
            return status;
        detected_.reset();
        detectedOptions_.reset();
        detectionInput_.reset();
        draft_ = std::move(next);
        return Status{std::monostate{}};
    }
    if (request.peak_operation == PeakOperation::restore_snapshot) {
        if (request.background || request.smoothing || !request.background_filters.empty() || request.selection)
            return Status{
                Error{ErrorCode::invalid_input, "preprocessing", "Restore must be a separate operation"}};
        auto valid = validateProductState(next);
        if (!valid)
            return valid;
        candidateCount_ =
            next.preprocessing.peak_candidates ? next.preprocessing.peak_candidates->peaks.size() : 0;
        draft_ = std::move(next);
        return Status{std::monostate{}};
    }
    const bool patternChanged = request.background || request.smoothing || !request.background_filters.empty();
    if (patternChanged) {
        detected_.reset();
        detectedOptions_.reset();
        if (request.peak_operation == PeakOperation::reselect)
            return Status{Error{ErrorCode::invalid_input, "peaks.operation",
                                "Changed intensity requires redetection", "redetection_required"}};
    }
    if (request.background_points && !request.background)
        return Status{Error{ErrorCode::invalid_input, "background.points",
                            "Manual points require explicit background parameters"}};
    if (request.peaks && request.peak_operation == PeakOperation::redetect) {
        // A failure in any preparation stage invalidates the previous detection,
        // just as a failure in the formal extractor does.
        invalidateDetection();
    }
    std::optional<CommonPreprocessingResult> common;
    if (patternChanged || (request.peaks && request.peak_operation == PeakOperation::redetect)) {
        CommonPreprocessingRequest stages;
        stages.background = request.background;
        if (request.initial_peaks) stages.initial_peaks = request.initial_peaks->options;
        stages.background_points = request.background_points;
        stages.background_filters = request.background_filters;
        stages.smoothing = request.smoothing;
        stages.wavelength = request.wavelength;
        stages.estimate_fwhm = !request.fwhm;
        stages.purpose = request.purpose;
        if (request.peak_operation == PeakOperation::redetect) {
            stages.peaks = request.peaks;
            if (stages.peaks && request.selection) {
                stages.peaks->threshold_percent = request.selection->threshold_percent;
                stages.peaks->detection_range = AngularRange{TwoThetaDegrees{request.selection->minimum},
                                                           TwoThetaDegrees{request.selection->maximum}};
            }
        }
        auto evaluated = evaluateCommonPreprocessing(source_, next.pattern, stages, context);
        if (!evaluated) return Status{evaluated.error()};
        common = std::move(evaluated).value();
        next.pattern = common->working_pattern;
    }
    if (request.background) {
        const auto &background = *common->background;
        next.preprocessing.background_points = background.points;
        next.preprocessing.background_filters.clear();
        next.preprocessing.background = request.background;
        next.provenance.backgroundSource = AlgorithmRunSource{algorithms, SearchParameters{AngularWidthDegrees{0}}, source_, source_->revision};
        next.provenance.backgroundSource->background = request.background;
        if (request.background->automatic && !background.coefficients.empty())
            next.preprocessing.background->coefficients = int(background.coefficients.size());
        next.projectMetadata["has_back"] = true;
    }
    next.preprocessing.background_filters.insert(next.preprocessing.background_filters.end(),
        request.background_filters.begin(), request.background_filters.end());
    if (request.smoothing) {
        next.preprocessing.smoothing = request.smoothing;
        next.preprocessing.smoothing_applied = true;
    }
    if (!request.background && !request.smoothing && !request.peaks && !request.selection &&
        request.background_filters.empty())
        return Status{Error{ErrorCode::invalid_input, "preprocessing", "Empty request"}};
    const auto &oldBackground = source_->pattern.background;
    const auto &newBackground = next.pattern.background;
    const bool sameBackground =
        (!oldBackground && !newBackground) ||
        (oldBackground && newBackground && oldBackground->values == newBackground->values &&
         oldBackground->window.has_value() == newBackground->window.has_value() &&
         (!oldBackground->window || oldBackground->window->first == newBackground->window->first));
    const bool inputChanged = next.pattern.observed != source_->pattern.observed ||
                              next.pattern.two_theta != source_->pattern.two_theta || !sameBackground;
    if (inputChanged) {
        invalidateAutomaticFwhm(next.preprocessing);
        next.preprocessing.peak_fwhm_usage.reset();
        next.peaks = {};
        next.preprocessing.peak_candidates.reset();
        next.preprocessing.peak_calibration.reset();
        next.preprocessing.fwhm_estimate.reset();
        next.preprocessing.rejected_candidates.clear();
        next.preprocessing.candidate_failure.reset();
        next.peakTolerance.reset();
        next.automaticDelta = .5;
    }
    if (!request.peak_product_operation &&
        (inputChanged || (patternChanged && (request.peaks || request.selection)))) {
        next.candidates.clear();
        next.phases.clear();
    }
    if (request.peaks || request.selection) {
        preprocessing::PeakSearchResult r;
        if (request.peak_operation == PeakOperation::reselect) {
            if (detected_ && detectionInput_) {
                next.pattern = detectionInput_->pattern;
                next.preprocessing = detectionInput_->preprocessing;
            }
            if (next.preprocessing.candidate_failure && !detected_)
                return Status{Error{ErrorCode::undefined_preprocessing, "peaks.candidates",
                                    "Legacy candidate pool contains undefined measurements",
                                    *next.preprocessing.candidate_failure}};
            if (detectionFailed_)
                return Status{Error{ErrorCode::missing_data, "peaks.candidates",
                                    "The last detection failed; repeat detection or cancel the edit",
                                    "candidate_pool_invalidated"}};
            auto pool = detected_ ? detected_ : next.preprocessing.peak_candidates;
            if (!pool)
                return Status{Error{ErrorCode::missing_data, "peaks.candidates",
                                    "Candidate pool is unavailable; explicit redetection is required",
                                    "candidate_pool_unavailable"}};
            if (!request.selection)
                return Status{Error{ErrorCode::missing_data, "peaks.selection",
                                    "Reselection requires selection parameters"}};
            r.candidates = *pool;
            r.peaks = *pool;
            r.rejected_candidates = next.preprocessing.rejected_candidates;
            r.applied = request.peaks.value_or(
                previousParameters.value_or(detectedOptions_.value_or(preprocessing::PeakOptions{})));
        } else {
            if (!request.peaks)
                return Status{
                    Error{ErrorCode::missing_data, "peaks.options", "Detection requires parameters"}};
            detected_.reset();
            detectedOptions_.reset();
            detectionFailed_ = true;
            auto options = *request.peaks;
            if (request.selection) {
                options.threshold_percent = request.selection->threshold_percent;
                options.detection_range = AngularRange{TwoThetaDegrees{request.selection->minimum},
                                                       TwoThetaDegrees{request.selection->maximum}};
            }
            if (request.fwhm) {
                auto estimated = request.supplied_width ?
                    Result<ConfiguredFwhmEvaluation>{ConfiguredFwhmEvaluation{source_, *request.fwhm, request.supplied_width, {}, {}}} :
                    evaluateConfiguredFwhm(source_,
                        {common->working_pattern, *request.fwhm, common->spectrum, false,
                         request.initial_peaks ? std::optional{request.initial_peaks->options} : std::nullopt,
                         request.workflow}, context);
                if (!estimated) return Status{estimated.error()};
                fwhmEvaluation_ = std::make_shared<const ConfiguredFwhmEvaluation>(std::move(estimated).value());
                auto applied = applyConfiguredFwhm(next, *fwhmEvaluation_);
                if (!applied) return applied;
                next.workflow.completed_target.reset();
                next.preprocessing.peak_fwhm_usage = fwhmEvaluation_->usage;
            }
            auto parameters = request.fwhm ? prepareConfiguredPeakParameters(options,
                *fwhmEvaluation_->usage, request.wavelength, context) :
                preprocessing::prepare_classic_peak_parameters(options,
                    common->fwhm ? common->fwhm->estimate : preprocessing::FwhmEstimate{}, request.wavelength, context);
            if (!parameters) return Status{parameters.error()};
            auto result = evaluateFormalPeakExtraction(source_,
                {common->spectrum, parameters.value(), request.wavelength, request.tolerance}, context);
            if (!result) return Status{result.error()};
            r = std::move(result).value();
            if (common->fwhm) {
                r.fwhm = common->fwhm->estimate;
                const auto &initial = common->fwhm->rejected_candidates;
                r.rejected_candidates.insert(r.rejected_candidates.begin(), initial.begin(), initial.end());
            }
            if (fwhmEvaluation_ && fwhmEvaluation_->gaussian) {
                r.fwhm = fwhmEvaluation_->gaussian->estimate;
                const auto &rejected = fwhmEvaluation_->gaussian->rejected_candidates;
                r.rejected_candidates.insert(r.rejected_candidates.begin(), rejected.begin(), rejected.end());
            }
            detectionFailed_ = false;
            detected_ = r.candidates;
            detectedOptions_ = r.applied;
            detectionInput_ = next;
            detectionInput_->preprocessing.peak_calibration = r.calibration;
            detectionInput_->preprocessing.fwhm_estimate = r.fwhm;
            detectionInput_->preprocessing.peak_candidates = r.candidates;
            detectionInput_->preprocessing.rejected_candidates = r.rejected_candidates;
        }
        next.preprocessing.peak_candidates = r.candidates;
        next.preprocessing.rejected_candidates = r.rejected_candidates;
        if (r.calibration) {
            next.preprocessing.peak_calibration = r.calibration;
            next.preprocessing.fwhm_estimate = r.fwhm;
        }
        next.preprocessing.candidate_failure.reset();
        candidateCount_ = r.candidates.peaks.size();
        next.preprocessing.selection.reset();
        if (request.selection) {
            auto selection = *request.selection;
            const bool reselect = request.peak_operation == PeakOperation::reselect;
            auto list = reselect ? r.candidates.peaks : r.peaks.peaks;
            if (reselect) {
                float maximum = 0;
                for (const auto &p : list)
                    maximum = std::max(maximum, float(p.intensity));
                if (maximum <= 0 && !list.empty())
                    return Status{Error{ErrorCode::undefined_preprocessing, "peaks.filter",
                                        "Undefined maximum height", "invalid_maximum_height"}};
                const float threshold =
                    selection.maximum_peaks >= 0 ? 0 : selection.threshold_percent / 100.f;
                std::erase_if(list, [&](const Peak &p) {
                    return !(float(p.intensity) / maximum >= threshold) ||
                           p.two_theta.value < float(selection.minimum) ||
                           p.two_theta.value > float(selection.maximum);
                });
                if (selection.maximum_peaks >= 0) {
                    std::stable_sort(list.begin(), list.end(),
                                     [](const auto &a, const auto &b) { return a.intensity > b.intensity; });
                    if (list.size() > std::size_t(selection.maximum_peaks))
                        list.resize(selection.maximum_peaks);
                    if (!selection.append)
                        selection.threshold_percent =
                            list.empty() ? 100.f : float(list.back().intensity) * (100.f / maximum);
                }
            } else {
                selection.threshold_percent = r.applied.threshold_percent;
                selection.maximum_peaks = -1;
            }
            if (selection.append && previousPeaks.valid) {
                auto previous = productSearchPeaks(previousPeaks);
                if (!previous)
                    return Status{previous.error()};
                for (const auto &p : previous.value().peaks)
                    if (p.two_theta.value < float(selection.minimum) ||
                        p.two_theta.value > float(selection.maximum))
                        list.push_back(p);
            }
            std::stable_sort(list.begin(), list.end(), [](const auto &a, const auto &b) {
                return a.two_theta.value < b.two_theta.value;
            });
            r.peaks.peaks = std::move(list);
            auto automatic = preprocessing::automatic_tolerance(r.peaks);
            if (!automatic)
                return Status{automatic.error()};
            r.automatic_tolerance = automatic.value();
            next.preprocessing.selection = selection;
        }
        auto ep = productMeasuredPeaks(r.peaks);
        if (!ep) return Status{ep.error()};
        next.peaks = std::move(ep).value();
        if (request.selection && request.selection->append && next.provenance.peakSource)
            next.provenance.retainedPeakSources.push_back(*next.provenance.peakSource);
        else if (request.peak_operation == PeakOperation::redetect)
            next.provenance.retainedPeakSources.clear();
        next.provenance.peakSource = AlgorithmRunSource{algorithms, SearchParameters{AngularWidthDegrees{0}}, source_, source_->revision};
        next.provenance.peakSource->peaks = r.applied;
        next.provenance.peakSource->fwhm = next.preprocessing.peak_fwhm_usage;
        // Peak-only operations apply P1/P2 after all final fields are prepared.
        if (patternChanged && !request.peak_product_operation) {
            next.projectMetadata["peak_snapshot_origin"] = "detected";
            next.projectMetadata["candidate_pool_state"] = "complete_finite";
        }
        next.automaticDelta = r.automatic_tolerance;
        next.peakTolerance = request.tolerance.value;
        if (next.projectSettings) next.workflow.completed_target = WorkflowStep::peaks;
        if (request.peak_operation == PeakOperation::redetect || detectedOptions_ || previousParameters)
            next.preprocessing.peaks = r.applied;
        if (next.preprocessing.peaks) {
            auto &parameters = *next.preprocessing.peaks;
            if (request.peak_operation == PeakOperation::redetect) {
                parameters.maximum_peaks = -1;
                parameters.minimum_peaks = -1;
                if (request.restore_automatic_window)
                    parameters.automatic_window = true;
            }
            if (next.preprocessing.selection) {
                const auto &selection = *next.preprocessing.selection;
                parameters.threshold_percent =
                    selection.maximum_peaks >= 0 && selection.append ? 0 : selection.threshold_percent;
                if (selection.maximum_peaks >= 0)
                    parameters.maximum_peaks = selection.maximum_peaks;
            }
        }
    }
    if ((!patternChanged || request.peak_product_operation) && (request.peaks || request.selection)) {
        auto policy = finalizePeakEdit(next, *source_, PeakEditOrigin::detected);
        if (!policy)
            return policy;
    }
    auto valid = validateProductState(next);
    if (!valid)
        return valid;
    draft_ = std::move(next);
    return Status{std::monostate{}};
}
Status PreprocessingEvaluation::preview(const PreprocessingRequest &request, std::stop_token token) {
    if (token.stop_requested()) {
        draft_.reset();
        invalidateDetection();
        return Status{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    }
    if (!source_)
        return Status{Error{ErrorCode::missing_data, "preprocessing.source", "Missing source"}};
    auto valid = validateProductState(*source_);
    if (!valid) {
        draft_.reset();
        return valid;
    }
    if (!stripper_) {
        draft_.reset();
        return Status{Error{ErrorCode::invalid_input, "preprocessing.evaluator", "Missing evaluator"}};
    }
    auto status = evaluate(request, {token, NumericalPolicy::legacy_mixed_precision, "product-preprocessing"});
    if (token.stop_requested()) {
        draft_.reset();
        invalidateDetection();
        return Status{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    }
    return status;
}
} // namespace qualpf::application
