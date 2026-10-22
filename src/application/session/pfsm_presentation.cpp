#include "pfsm_workflow.h"
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/workflow.h"
#include <cmath>

namespace qualpf::application {
namespace {
Error invalid(const char *field, const char *message) {
    return {ErrorCode::invalid_input, field, message};
}
bool sameMeasurement(const Pattern &a, const Pattern &b) {
    if (a.id != b.id || a.source_id != b.source_id || a.two_theta != b.two_theta ||
        a.observed != b.observed || a.original_observed != b.original_observed ||
        a.original_kalpha2_subtracted != b.original_kalpha2_subtracted ||
        a.corrected_two_theta != b.corrected_two_theta || a.intensity_scale != b.intensity_scale ||
        a.processing.background_subtracted != b.processing.background_subtracted ||
        a.processing.kalpha2_subtracted != b.processing.kalpha2_subtracted ||
        a.processing.intensity_scaled != b.processing.intensity_scaled ||
        a.radiation.kind != b.radiation.kind || a.radiation.lines.size() != b.radiation.lines.size() ||
        bool(a.background) != bool(b.background))
        return false;
    if (a.background && (a.background->values != b.background->values ||
        bool(a.background->window) != bool(b.background->window) ||
        (a.background->window && a.background->window->first != b.background->window->first)))
        return false;
    for (std::size_t i = 0; i < a.radiation.lines.size(); ++i)
        if (a.radiation.lines[i].wavelength != b.radiation.lines[i].wavelength ||
            a.radiation.lines[i].relative_weight != b.radiation.lines[i].relative_weight)
            return false;
    return true;
}
}
bool compatibleManualPfsmWidth(const ProductState &s, const FwhmUsage &u) {
    if (s.projectSettings && u.manual && u.current && u.origin == FwhmOrigin::fixed)
        return reusableWorkflowStep(s, WorkflowId::pfsm, WorkflowStep::fwhm) && bool(validateFwhmUsage(u));
    return u.manual && u.current && u.source_pattern && u.input_pattern &&
        s.provenance.residualHistory.empty() && s.provenance.subtractedBackgroundHistory.empty() &&
        !s.provenance.legacyUnverified && s.pattern.processing.background_subtracted == false &&
        (sameMeasurement(s.pattern, *u.source_pattern) ||
         (s.projectSettings && reusableWorkflowStep(s, WorkflowId::pfsm, WorkflowStep::fwhm) && sameMeasurement(s.pattern, *u.input_pattern))) &&
        s.original_signal_provenance == u.source_original_signal &&
        bool(validateFwhmUsage(u));
}
Status validatePfsmSettings(const PfsmSettings &s) {
    if (s.candidate_recall_resource_fallback != PfsmResourceFallback::bounded_integral &&
        s.candidate_recall_resource_fallback != PfsmResourceFallback::full_pool)
        return Status{invalid("pfsm.candidate_recall.resource_fallback", "Unknown resource-limit policy")};
    if (!std::isfinite(s.lower_shift) || !std::isfinite(s.upper_shift) || s.lower_shift > s.upper_shift ||
        !std::isfinite(s.minimum_relative_fom) || s.minimum_relative_fom < 0 || s.minimum_relative_fom > 1 ||
        s.threads < 0 || (s.weighting != PfsmSettings::Weighting::automatic && s.weighting != PfsmSettings::Weighting::equal))
        return Status{invalid("pfsm.settings", "Finite ordered shift bounds, relative FOM in [0,1] and nonnegative workers are required")};
    return profile::validatePfsmCandidateRecallLimits(s.candidate_recall_limits);
}
PfsmSettings pfsmSettings(const MetadataObject &values, std::optional<Error> *recallWarning) {
    if (recallWarning) recallWarning->reset();
    PfsmSettings s;
    const auto read = [&](const char *key, double fallback) {
        const auto i = values.find(key);
        return i == values.end() ? fallback : i->second.toDouble(fallback);
    };
    s.lower_shift = read("PFSMOptions/lowerShift", s.lower_shift);
    s.upper_shift = read("PFSMOptions/upperShift", s.upper_shift);
    s.minimum_relative_fom = read("PFSMOptions/minimumRelativeFom", 0);
    if (const auto i = values.find("PFSMOptions/workers"); i != values.end())
        s.threads = i->second.toInt();
    if (const auto i = values.find("PFSMOptions/templateCache"); i != values.end())
        s.template_cache = i->second.toBool();
    if (const auto i = values.find("PFSMOptions/weighting"); i != values.end() && i->second.toString() == "equal")
        s.weighting = PfsmSettings::Weighting::equal;
    if (!validatePfsmSettings(s)) s = {};
    constexpr auto prefix = "PFSMOptions/candidateRecall/";
    if (const auto i = values.find(std::string(prefix) + "resourceFallback"); i != values.end()) {
        if (i->second.toString() == "full_pool") s.candidate_recall_resource_fallback = PfsmResourceFallback::full_pool;
        else if (i->second.toString() != "bounded_integral" && recallWarning)
            *recallWarning = invalid("pfsm.candidate_recall.resource_fallback", "Invalid saved resource-limit policy; using bounded integral recall.");
    }
    if (const auto i = values.find(std::string(prefix) + "enabled"); i != values.end()) {
        s.candidate_recall_enabled = i->second.toBool(true);
        if (!std::holds_alternative<bool>(i->second.data) && recallWarning)
            *recallWarning = invalid("pfsm.candidate_recall.enabled", "Invalid saved recall switch; using the default (enabled).");
    }
    const auto count = [&](const char *key, int &value) {
        if (const auto i = values.find(std::string(prefix) + key); i != values.end())
            value = i->second.toInt(0); // Invalid, fractional and overflowing values fail validation.
    };
    auto &limits = s.candidate_recall_limits;
    count("recallCandidates", limits.recall_candidates);
    count("localShortlist", limits.local_shortlist);
    count("protectedPerChannel", limits.protected_per_channel);
    count("reviewLimit", limits.review_limit);
    count("formalLimit", limits.formal_limit);
    if (const auto status = profile::validatePfsmCandidateRecallLimits(limits); !status) {
        limits = {};
        if (recallWarning) *recallWarning = status.error();
    }
    return s;
}
void storePfsmSettings(MetadataObject &values, const PfsmSettings &s) {
    values["PFSMOptions/lowerShift"] = s.lower_shift;
    values["PFSMOptions/upperShift"] = s.upper_shift;
    values["PFSMOptions/minimumRelativeFom"] = s.minimum_relative_fom;
    values["PFSMOptions/workers"] = s.threads;
    values["PFSMOptions/templateCache"] = s.template_cache;
    values["PFSMOptions/weighting"] = s.weighting == PfsmSettings::Weighting::equal ? "equal" : "auto";
    values["PFSMOptions/candidateRecall/enabled"] = s.candidate_recall_enabled;
    values["PFSMOptions/candidateRecall/resourceFallback"] =
        s.candidate_recall_resource_fallback == PfsmResourceFallback::bounded_integral ? "bounded_integral" : "full_pool";
    const auto &limits = s.candidate_recall_limits;
    values["PFSMOptions/candidateRecall/recallCandidates"] = limits.recall_candidates;
    values["PFSMOptions/candidateRecall/localShortlist"] = limits.local_shortlist;
    values["PFSMOptions/candidateRecall/protectedPerChannel"] = limits.protected_per_channel;
    values["PFSMOptions/candidateRecall/reviewLimit"] = limits.review_limit;
    values["PFSMOptions/candidateRecall/formalLimit"] = limits.formal_limit;
}
Result<PfsmRequest> makePfsmRequest(ProductHandle source, const PfsmSettings &s) {
    auto valid = validatePfsmSettings(s);
    if (!valid) return Result<PfsmRequest>{valid.error()};
    if (!source || !source->algorithms)
        return Result<PfsmRequest>{invalid("pfsm.source", "Missing input")};
    PfsmRequest request;
    request.threads = s.threads;
    if (source->pfsm) // Only per-run workers change inside an existing analysis.
        return Result<PfsmRequest>{std::move(request)};
    const auto &p = source->pattern;
    const auto &original = source->original_signal_provenance;
    if (!original || !original->unclipped ||
        !p.original_observed || !p.original_kalpha2_subtracted)
        return Result<PfsmRequest>{Error{ErrorCode::missing_data, "pfsm.provenance",
            "Original signal provenance is unavailable. Reimport the original measurement before PFSM.", "pfsm_reimport_required"}};
    if (p.processing.background_subtracted != false || !source->provenance.subtractedBackgroundHistory.empty())
        return Result<PfsmRequest>{Error{ErrorCode::missing_data,"background.state",
            "Restart preprocessing or reimport before PFSM", "subtracted_input_refused"}};
    PfsmObservationRequest observations;
    observations.signal = PfsmObservationRequest::Signal::observed;
    observations.signal_source = original->source;
    observations.intensity_units = p.processing.intensity_scaled == true ? "relative working intensity" : original->intensity_units;
    observations.unclipped_signal = true;
    if (p.processing.kalpha2_subtracted == true) {
        observations.radiation = p.radiation;
        observations.radiation->lines.resize(1);
        observations.radiation->lines.front().relative_weight = 1;
    }
    if (p.background && reusableWorkflowStep(*source, WorkflowId::pfsm, WorkflowStep::background)) {
        auto checked = AnalysisSpectrum::create(p);
        if (!checked) return Result<PfsmRequest>{checked.error()};
        observations.background = p.background->values;
    }
    const bool originalStatistics = p.observed == *p.original_observed &&
        p.processing.kalpha2_subtracted == p.original_kalpha2_subtracted &&
        p.processing.intensity_scaled == original->intensity_scaled &&
        source->preprocessing.smoothing_applied != true &&
        (!original->standard_deviations || original->standard_deviations->size() == p.observed.size());
    // The text/CIF importers currently provide no attested variance model.
    // Working normalization and positivity cannot establish Poisson counts.
    observations.equal_weights = true;
    observations.noise_source = s.weighting == PfsmSettings::Weighting::equal ?
        "Equal weights selected by user" : originalStatistics ? "Equal weights: imported noise statistics unknown" : "Equal weights: working signal statistics unavailable";
    const auto &evidence = *original;
    if (s.weighting == PfsmSettings::Weighting::automatic && originalStatistics &&
        (evidence.standard_deviations || evidence.raw_poisson_counts)) {
        if (evidence.noise_source.empty())
            return Result<PfsmRequest>{invalid("pfsm.noise", "Declared input statistics require a noise source")};
        observations.equal_weights = false;
        observations.noise_source = evidence.noise_source;
        if (evidence.standard_deviations)
            observations.noise = preprocessing::FwhmStandardDeviations{*evidence.standard_deviations};
        else
            observations.noise = preprocessing::FwhmPoissonCounts{};
    }
    const auto c = source->projectSettings ? source->projectSettings->profile(WorkflowId::pfsm).fwhm :
        source->preprocessing.fwhm.value_or(FwhmConfiguration{});
    observations.width_options.minimum_samples_per_fwhm = c.minimum_sampling_density;
    request.observations = std::move(observations);
    request.lower_shift = s.lower_shift;
    request.upper_shift = s.upper_shift;
    request.template_cache = s.template_cache;
    request.candidate_recall.mode = s.candidate_recall_enabled ? PfsmCandidateRecallMode::enabled : PfsmCandidateRecallMode::off;
    request.candidate_recall.limits = s.candidate_recall_limits;
    request.candidate_recall.resource_fallback = s.candidate_recall_resource_fallback;
    if (source->projectSettings && source->preprocessing.fwhm_usage &&
        compatibleManualPfsmWidth(*source, *source->preprocessing.fwhm_usage)) {
        request.supplied_width = source->preprocessing.fwhm_usage;
    } else if (c.use_fixed) {
        valid = validateFwhmConfiguration(c);
        if (!valid) return Result<PfsmRequest>{valid.error()};
        FwhmUsage width;
        width.width = c.default_fwhm;
        width.origin = FwhmOrigin::fixed;
        width.algorithm = {"qualpf.fwhm.fixed", 1};
        width.dataset = p.id;
        width.source_id = p.source_id;
        width.source_revision = source->revision;
        width.current = true;
        width.manual = !source->projectSettings;
        width.minimum_sampling_density = c.minimum_sampling_density;
        request.supplied_width = std::move(width);
    } else if (source->preprocessing.fwhm_usage && compatibleManualPfsmWidth(*source, *source->preprocessing.fwhm_usage))
        request.supplied_width = source->preprocessing.fwhm_usage;
    return Result<PfsmRequest>{std::move(request)};
}
Result<ProductState> prepareSearchMethod(ProductHandle source, SearchMethod method, bool endAnalysis) {
    if (!source || !source->algorithms || source->editing)
        return Result<ProductState>{invalid("search.method", "Finish preprocessing before choosing a search method")};
    auto valid = preflightSearchMethod(method);
    if (!valid) return Result<ProductState>{valid.error()};
    const bool changed = method != source->algorithms->spec.method();
    const bool existing = source->pfsm || !source->candidates.empty() || !source->phases.empty();
    if (changed && existing && !endAnalysis)
        return Result<ProductState>{invalid("search.method", "Confirm ending the current analysis before switching methods")};
    ProductState next = *source;
    if (changed || endAnalysis) {
        if (!source->provenance.residualHistory.empty() || !source->provenance.subtractedBackgroundHistory.empty() ||
            source->provenance.legacyUnverified) {
            const auto &p = source->pattern;
            const auto &original = source->original_signal_provenance;
            if (!p.original_observed || !p.original_kalpha2_subtracted || !original ||
                !original->unclipped)
                return Result<ProductState>{Error{ErrorCode::missing_data, "search.original", "Reimport the original measurement to end this analysis safely", "pfsm_reimport_required"}};
            next.pattern.observed = *p.original_observed;
            next.pattern.processing = {false, p.original_kalpha2_subtracted, original->intensity_scaled};
            next.pattern.intensity_scale = IntensityScale::unspecified;
            next.pattern.background.reset();
            next.pattern.background_subtracted_signal.reset();
            next.pattern.calculated.reset();
            next.peaks.clear();
            next.peaks.wave = p.radiation.lines.empty() ? 0 : p.radiation.lines.front().wavelength.value;
            invalidateAutomaticFwhm(next.preprocessing);
            next.preprocessing.peak_fwhm_usage.reset();
            next.preprocessing.peak_candidates.reset();
            next.preprocessing.peak_calibration.reset();
            next.provenance = {};
        }
        next.candidates.clear();
        next.phases.clear();
        next.pfsm.reset();
        next.analysis_database.clear(); next.restored_project=false;
        next.prepared_pfsm.reset();
        next.workflow.configuration_transition = true;
        for (auto &record : next.workflow.records)
            if (record.step == WorkflowStep::search || record.step == WorkflowStep::phases || record.step == WorkflowStep::rir)
                record.status = WorkflowStepStatus::invalidated;
    }
    if (changed) {
        const auto &old = source->algorithms;
        auto algorithms = resolveAlgorithms(AlgorithmSpec{old->spec.selections(), method, old->spec.defaults()}, old->modules);
        if (!algorithms) return Result<ProductState>{algorithms.error()};
        next.algorithms = std::move(algorithms).value();
    }
    return Result<ProductState>{std::move(next)};
}
std::optional<PfsmCandidateMetrics> pfsmCandidateMetrics(const ProductState &state, const ProductCard &card) {
    if (!state.pfsm || !card.pfsm || card.pfsm->generation != state.pfsm->generation ||
        card.pfsm->parameters.scales.empty() || card.pfsm->parameters.shifts.empty())
        return {};
    const auto &e = *card.pfsm;
    const double maximum = state.pfsm->normalization_maximum;
    return PfsmCandidateMetrics{100 * e.rwp_net, e.parameters.shifts.back(), e.parameters.scales.back(),
        maximum > 0 ? e.delta_rwp / maximum : 0, 100 * e.delta_rwp};
}
Result<PfsmPlot> preparePfsmPlot(ProductHandle source, const std::string &id, const ExecutionContext &context) try {
    using namespace pfsm_detail;
    if (!source || !source->pfsm)
        fail("pfsm.plot", "No PFSM analysis", ErrorCode::missing_data);
    checkpoint(context);
    const auto &a = *source->pfsm;
    const auto &data = *a.problem->observations;
    PfsmPlot plot;
    auto &p = plot.pattern;
    p.id = source->pattern.id;
    p.source_id = source->pattern.source_id;
    p.radiation = a.problem->radiation;
    p.processing.background_subtracted = false;
    plot.background_points = a.problem->background_points;
    for (double x : data.coordinates) p.two_theta.emplace_back(x);
    p.observed = data.observed;
    p.background = SampledSignal{data.background, SampleWindow{0}};
    p.calculated = SampledSignal{a.baseline.calculated, SampleWindow{0}};
    plot.residual = a.baseline.residual;
    plot.intensity_units = a.problem->source.intensity_units;
    plot.candidate_id = id;
    if (!id.empty()) {
        const auto found = std::find_if(source->candidates.begin(), source->candidates.end(), [&](const auto &c) { return c.id == id; });
        if (found == source->candidates.end() || !found->pfsm || found->pfsm->generation != a.generation)
            fail("pfsm.plot", "Candidate evidence is stale", ErrorCode::stale_snapshot);
        auto phases = a.selected;
        const auto &e = *found->pfsm;
        phases.push_back({id, expand((*a.universe)[e.original_index], *a.problem, context)});
        std::vector<double> calculated(data.observed.size(), 0);
        for (std::size_t j = 0; j < phases.size(); ++j) {
            auto samples = take(phases[j].profile->evaluateExact(data.coordinates, e.parameters.shifts[j], false, context));
            for (std::size_t i = 0; i < calculated.size(); ++i) {
                checkpoint(context);
                calculated[i] += e.parameters.scales[j] * samples.intensity[i];
            }
        }
        for (std::size_t i = 0; i < calculated.size(); ++i) {
            plot.residual[i] = (data.observed[i] - data.background[i]) - calculated[i];
            calculated[i] += data.background[i];
        }
        p.calculated->values = std::move(calculated);
    }
    return Result<PfsmPlot>{std::move(plot)};
} catch (const pfsm_detail::Failure &e) { return Result<PfsmPlot>{e.error}; }
} // namespace qualpf::application
