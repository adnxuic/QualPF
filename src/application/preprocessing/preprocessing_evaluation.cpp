#include "qualpf/preprocessing/kalpha2.h"
#include "qualpf/application/preprocessing_evaluation.h"
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

Status PreprocessingEvaluation::evaluate(const PreprocessingRequest &request) {
    if (!source_)
        return Status{Error{ErrorCode::missing_data, "preprocessing.source", "Missing source"}};
    auto previousPeaks = draft_ ? draft_->peaks : source_->peaks;
    auto previousParameters = draft_ ? draft_->preprocessing.peaks : source_->preprocessing.peaks;
    candidateCount_ = 0;
    draft_.reset(); // A failed/new parameter request invalidates the previous
                    // preview.
    auto next = *source_;
    if (request.peak_product_operation &&
        (!request.peaks || request.smoothing || request.subtract_background || request.background_points ||
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
            request.subtract_background || request.peak_operation != PeakOperation::redetect)
            return Status{Error{ErrorCode::invalid_input, "preprocessing",
                                "Peak editing must be a separate operation"}};
        auto status = preparePeakOperation(next, request);
        if (!status)
            return status;
        detected_.reset();
        detectedOptions_.reset();
        detectionInput_.reset();
        draft_ = std::move(next);
        return Status{std::monostate{}};
    }
    if (request.strip_kalpha2) {
        if (request.manual || request.restart_from_original || request.crop_range || request.background ||
            request.smoothing || request.peaks || request.selection || request.background_points ||
            !request.background_filters.empty() || request.subtract_background ||
            request.use_background_for_peaks || request.restore_automatic_window ||
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
            next.pattern.observed = std::move(result).value();
            next.pattern.background.reset();
            next.pattern.calculated.reset();
            next.pattern.background_subtracted_signal.reset();
            next.pattern.weights.reset();
            next.preprocessing.background_points.reset();
            next.preprocessing.background_filters.clear();
            next.preprocessing.peak_candidates.reset();
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
            request.background_points || !request.background_filters.empty() || request.subtract_background ||
            request.peak_operation != PeakOperation::redetect)
            return Status{Error{ErrorCode::invalid_input, "preprocessing",
                                "Recovery/manual edit must be a separate operation"}};
        if (request.manual) {
            auto status = prepareManualPeaks(next, *request.manual, request.wavelength, request.tolerance);
            if (!status)
                return status;
        } else if (request.crop_range) {
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
            next.preprocessing = {};
            next.preprocessing.smoothing_applied = false;
            next.peaks.clear();
            next.candidates.clear();
            next.phases.clear();
            next.peakTolerance.reset();
            next.automaticDelta = .5;
            next.projectMetadata["preprocessing_recovery"] = "restart_from_stored_original";
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
        if (request.background || request.smoothing || request.subtract_background ||
            !request.background_filters.empty() || request.selection)
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
    const bool patternChanged = request.background || request.smoothing || request.subtract_background ||
                                !request.background_filters.empty();
    if (patternChanged) {
        detected_.reset();
        detectedOptions_.reset();
        if (request.peak_operation == PeakOperation::reselect)
            return Status{Error{ErrorCode::invalid_input, "peaks.operation",
                                "Changed intensity requires redetection", "redetection_required"}};
    }
    if (request.background) {
        if (request.background_points &&
            (request.background->method == preprocessing::BackgroundMethod::spline ||
             request.background->method == preprocessing::BackgroundMethod::bezier ||
             request.background->method == preprocessing::BackgroundMethod::bruckner))
            return Status{Error{ErrorCode::invalid_input, "background.points",
                                "This method does not consume manual background points",
                                "manual_points_not_consumed_by_method"}};
        auto result =
            request.background_points
                ? preprocessing::background_from_points(next.pattern, *request.background, request.wavelength,
                                                        *request.background_points)
                : preprocessing::background(next.pattern, *request.background, request.wavelength);
        if (!result)
            return Status{result.error()};
        next.preprocessing.background_points = result.value().points;
        next.preprocessing.background_filters.clear();
        next.pattern.background = SampledSignal{std::move(result).value().values, SampleWindow{0}};
        next.preprocessing.background = request.background;
        if (request.background->automatic && !result.value().coefficients.empty())
            next.preprocessing.background->coefficients = int(result.value().coefficients.size());
        next.projectMetadata["has_back"] = true;
    }
    if (request.background_points && !request.background)
        return Status{Error{ErrorCode::invalid_input, "background.points",
                            "Manual points require explicit background parameters"}};
    for (const auto &filter : request.background_filters) {
        auto result = preprocessing::filter_background(next.pattern, filter);
        if (!result)
            return Status{result.error()};
        next.pattern.background = std::move(result).value();
        next.preprocessing.background_filters.push_back(filter);
    }
    if (request.subtract_background) {
        auto result = preprocessing::subtract_background(next.pattern);
        if (!result)
            return Status{result.error()};
        next.pattern = std::move(result).value();
        next.preprocessing.background_points.reset();
        next.preprocessing.background_filters.clear();
    }
    if (request.smoothing) {
        if (request.smoothing->derivative != 0)
            return Status{Error{ErrorCode::invalid_input, "preprocessing.derivative",
                                "Derivative preview cannot replace the working intensity"}};
        auto result = preprocessing::smooth(next.pattern.observed, *request.smoothing);
        if (!result)
            return Status{result.error()};
        next.pattern.observed = std::move(result).value();
        next.preprocessing.smoothing = request.smoothing;
        next.preprocessing.smoothing_applied = true;
    }
    if (!request.background && !request.smoothing && !request.peaks && !request.selection &&
        !request.subtract_background && request.background_filters.empty())
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
        next.peaks = {};
        next.preprocessing.peak_candidates.reset();
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
            r.applied = request.peaks.value_or(
                previousParameters.value_or(detectedOptions_.value_or(preprocessing::PeakOptions{})));
        } else {
            if (!request.peaks)
                return Status{
                    Error{ErrorCode::missing_data, "peaks.options", "Detection requires parameters"}};
            detected_.reset();
            detectedOptions_.reset();
            detectionFailed_ = true;
            auto signal = next.pattern;
            if (request.use_background_for_peaks) {
                if (!signal.background || !signal.background->window ||
                    signal.background->window->first != 0 ||
                    signal.background->values.size() != signal.observed.size())
                    return Status{Error{ErrorCode::missing_data, "peaks.background",
                                        "Detection requires a complete aligned background",
                                        "background_alignment_unknown"}};
                // run_peaksearch uses y-yb even on a previously subtracted
                // pattern (where yb is normally zero). No clipping/rescaling.
                for (std::size_t i = 0; i < signal.observed.size(); ++i)
                    signal.observed[i] = float(signal.observed[i]) - float(signal.background->values[i]);
            }
            auto options = *request.peaks;
            if (request.selection) {
                options.threshold_percent = request.selection->threshold_percent;
                options.detection_range = AngularRange{TwoThetaDegrees{request.selection->minimum},
                                                       TwoThetaDegrees{request.selection->maximum}};
            }
            auto result = preprocessing::find_peaks(signal, options, request.wavelength, request.tolerance);
            if (!result)
                return Status{result.error()};
            r = std::move(result).value();
            detectionFailed_ = false;
            detected_ = r.candidates;
            detectedOptions_ = r.applied;
            detectionInput_ = next;
        }
        next.preprocessing.peak_candidates = r.candidates;
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
        ProductPeaks ep;
        ep.valid = true;
        ep.wave = r.peaks.effective_wavelength->value;
        if (std::all_of(r.peaks.peaks.begin(), r.peaks.peaks.end(),
                        [](const auto &p) { return p.integrated_intensity.has_value(); }))
            ep.integratedIntensity.emplace();
        for (const auto &p : r.peaks.peaks) {
            ep.tth.push_back(p.two_theta.value);
            ep.d.push_back(p.d_spacing->value);
            ep.deltaD.push_back(p.delta_d->value);
            ep.intensity.push_back(p.intensity);
            ep.intensityOrig.push_back(*p.original_intensity);
            ep.fwhm.push_back(p.fwhm->value);
            if (ep.integratedIntensity)
                ep.integratedIntensity->push_back(*p.integrated_intensity);
        }
        next.peaks = std::move(ep);
        // Peak-only operations apply P1/P2 after all final fields are prepared.
        if (patternChanged && !request.peak_product_operation) {
            next.projectMetadata["peak_snapshot_origin"] = "detected";
            next.projectMetadata["candidate_pool_state"] = "complete_finite";
        }
        next.automaticDelta = r.automatic_tolerance;
        next.peakTolerance = request.tolerance.value;
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
    auto status = evaluate(request);
    if (token.stop_requested()) {
        draft_.reset();
        return Status{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    }
    return status;
}
} // namespace qualpf::application
