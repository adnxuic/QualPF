#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/preprocessing_workflows.h"
#include "qualpf/application/initial_peak_search.h"
#include "qualpf/application/workflow.h"
#include "qualpf/domain/validation.h"
#include <algorithm>
#include <cmath>
#include <map>

namespace qualpf::application {
namespace {
Error invalid(const char *field, const char *message) {
    return {ErrorCode::invalid_input, field, message, "invalid_fwhm_configuration"};
}
AlgorithmIdentity identity(FwhmOrigin origin) {
    switch (origin) {
    case FwhmOrigin::fixed: return {"qualpf.fwhm.fixed", 1};
    case FwhmOrigin::gaussian: return {"qualpf.peaks.fwhm.cpp", 3};
    case FwhmOrigin::geometric: return {"qualpf.peaks.fwhm.geometric", 1};
    }
    return {};
}
}
Status validateFwhmConfiguration(const FwhmConfiguration &c) {
    if ((c.method != FwhmMethod::gaussian && c.method != FwhmMethod::geometric) ||
        !std::isfinite(c.default_fwhm.value) || c.default_fwhm.value <= 0 || c.default_fwhm.value > 180 ||
        c.minimum_sampling_density < 2 || c.minimum_sampling_density > 8)
        return Status{invalid("fwhm.configuration", "FWHM must be positive and at most 180 degrees; Q must be an integer from 2 to 8")};
    return Status{std::monostate{}};
}
Status validateFwhmUsage(const FwhmUsage &u) {
    if (u.initial_peak_options && (u.origin != FwhmOrigin::gaussian ||
        !preprocessing::validate_initial_peak_options(*u.initial_peak_options)))
        return Status{invalid("fwhm.usage", "Invalid Gaussian initial peak options")};
    const bool historicalGaussian = !u.current && u.origin == FwhmOrigin::gaussian &&
        u.algorithm == AlgorithmIdentity{"qualpf.peaks.fwhm.cpp", 2};
    if ((u.origin != FwhmOrigin::fixed && u.origin != FwhmOrigin::gaussian && u.origin != FwhmOrigin::geometric) ||
        (!historicalGaussian && u.algorithm != identity(u.origin)) || !std::isfinite(u.width.value) || u.width.value <= 0 ||
        u.width.value > 180 || u.minimum_sampling_density < 2 || u.minimum_sampling_density > 8 ||
        (u.origin == FwhmOrigin::fixed ? u.width_count != 0 : u.width_count == 0))
        return Status{invalid("fwhm.usage", "Invalid physical FWHM evidence")};
    if (u.input_pattern && (u.input_pattern->id != u.dataset || u.input_pattern->source_id != u.source_id))
        return Status{invalid("fwhm.usage", "FWHM input snapshot disagrees with the source identity")};
    if (u.geometric && (u.origin != FwhmOrigin::geometric || u.geometric->dataset != u.dataset ||
        u.geometric->algorithm != u.algorithm || !u.geometric->default_fwhm ||
        u.geometric->default_fwhm->value != u.width.value ||
        u.geometric->applied.minimum_samples_per_fwhm != u.minimum_sampling_density))
        return Status{invalid("fwhm.usage", "Geometric FWHM diagnostics disagree with the measured width")};
    return Status{std::monostate{}};
}
Result<ConfiguredFwhmEvaluation> evaluateConfiguredFwhm(ProductHandle source,
    const ConfiguredFwhmRequest &request, const ExecutionContext &context) {
    using Return = Result<ConfiguredFwhmEvaluation>;
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "fwhm", "FWHM estimation canceled"}};
    if (!source || !source->algorithms)
        return Return{Error{ErrorCode::missing_data, "fwhm.source", "Missing FWHM source"}};
    if (source->projectSettings && request.manual) {
        auto plan = prepareWorkflow(source, WorkflowId::preprocessing, WorkflowStep::fwhm);
        if (!plan) return Return{plan.error()};
        auto working = std::make_shared<const ProductState>(std::move(plan).value().next);
        auto prepared = request;
        prepared.pattern = working->pattern; prepared.manual = false;
        prepared.initial_peaks = working->preprocessing.initial_peaks.value_or(
            working->projectSettings->profile(WorkflowId::preprocessing).initial_peaks).options;
        prepared.workflow = WorkflowId::preprocessing;
        auto result = evaluateConfiguredFwhm(working, prepared, context);
        if (!result) return result;
        result.value().source = source;
        result.value().preparation = working;
        if (result.value().usage) result.value().usage->manual = true;
        return result;
    }
    if (request.pattern.id != source->pattern.id ||
        (request.spectrum && (request.spectrum->pattern().id != source->pattern.id ||
            request.spectrum->pattern().source_id != request.pattern.source_id ||
            request.spectrum->pattern().two_theta != request.pattern.two_theta ||
            request.spectrum->pattern().observed != request.pattern.observed)))
        return Return{Error{ErrorCode::stale_snapshot, "fwhm.source", "FWHM input belongs to another dataset"}};
    auto valid = validateFwhmConfiguration(request.configuration);
    if (!valid) return Return{valid.error()};
    ConfiguredFwhmEvaluation out{source, request.configuration, {}, {}, {}};
    FwhmUsage usage;
    usage.dataset = request.pattern.id;
    usage.source_id = request.pattern.source_id;
    usage.source_revision = source->revision;
    usage.minimum_sampling_density = request.configuration.minimum_sampling_density;
    usage.current = true;
    usage.manual = request.manual;
    usage.source_pattern = std::make_shared<const Pattern>(request.pattern);
    usage.source_original_signal = source->original_signal_provenance;
    if (request.configuration.use_fixed) {
        usage.origin = FwhmOrigin::fixed;
        usage.width = request.configuration.default_fwhm;
    } else {
        auto pattern = request.pattern;
        if (pattern.two_theta.empty() || pattern.two_theta.size() != pattern.observed.size())
            return Return{Error{ErrorCode::missing_data, "fwhm.pattern", "A continuous pattern is required"}};
        if (pattern.processing.background_subtracted != false)
            return Return{Error{ErrorCode::missing_data, "background.state", "Restart from an explicitly unsubtracted input", pattern.processing.background_subtracted ? "subtracted_input_refused" : "subtraction_state_unknown"}};
        auto legacyContext = context;
        legacyContext.numerical_policy = NumericalPolicy::legacy_mixed_precision;
        if (!pattern.background && !*pattern.processing.background_subtracted) {
            CommonPreprocessingRequest preparation;
            preparation.background = source->preprocessing.background.value_or(source->algorithms->spec.defaults().background);
            preparation.background_points = source->preprocessing.background_points;
            preparation.background_filters = source->preprocessing.background_filters;
            if (pattern.radiation.lines.empty())
                return Return{Error{ErrorCode::missing_data, "pattern.radiation", "A wavelength is required to prepare the background"}};
            preparation.wavelength = pattern.radiation.lines.front().wavelength;
            auto prepared = evaluateCommonPreprocessing(source, std::move(pattern), preparation, legacyContext);
            if (!prepared) return Return{prepared.error()};
            pattern = std::move(prepared).value().working_pattern;
        }
        const bool geometric = request.configuration.method == FwhmMethod::geometric;
        if (!pattern.background || !pattern.background->window ||
            pattern.background->window->first != 0 || pattern.background->values.size() != pattern.observed.size())
            return Return{Error{ErrorCode::missing_data, "fwhm.background", "FWHM estimation requires a complete aligned background", "background_alignment_unknown"}};
        if (geometric) {
            GeometricFwhmRequest geometricRequest;
            auto &in = geometricRequest.input;
            in.dataset = pattern.id; in.source_id = pattern.source_id;
            in.radiation = pattern.radiation; in.processing = pattern.processing;
            in.two_theta = pattern.two_theta; in.intensity = pattern.observed;
            in.background = pattern.background->values;
            in.noise_source = "GUI working signal: noise model not supplied";
            geometricRequest.options.minimum_samples_per_fwhm = request.configuration.minimum_sampling_density;
            auto preciseContext = context;
            preciseContext.numerical_policy = NumericalPolicy::explicit_float64;
            auto measured = evaluateGeometricFwhm(source, geometricRequest, preciseContext);
            if (!measured) return Return{measured.error()};
            out.geometric = std::make_shared<const preprocessing::GeometricFwhmResult>(std::move(measured).value().measurement);
            if (!out.geometric->default_fwhm) return Return{std::move(out)};
            usage.origin = FwhmOrigin::geometric; usage.width_count = 1;
            usage.width = *out.geometric->default_fwhm; usage.geometric = out.geometric;
            pattern.background = SampledSignal{std::move(in.background), SampleWindow{0}};
            usage.input_pattern = std::make_shared<const Pattern>(std::move(pattern));
        } else {
            if (source->algorithms->modules.fwhm->identity() != identity(FwhmOrigin::gaussian))
                return Return{Error{ErrorCode::unsupported, "fwhm.algorithm", "The Gaussian option requires the Classical FWHM module"}};
            auto prepared = request.spectrum ? Result<AnalysisSpectrum>{*request.spectrum} :
                prepareAnalysisSpectrum(source, std::move(pattern), legacyContext);
            if (!prepared) return Return{prepared.error()};
            auto analysis = std::move(prepared).value();
            if (analysis.pattern().radiation.lines.empty())
                return Return{Error{ErrorCode::missing_data, "pattern.radiation", "Gaussian fitting requires a wavelength"}};
            const auto wave = analysis.pattern().radiation.lines.front().wavelength;
            usage.initial_peak_options = request.initial_peaks.value_or(
                source->preprocessing.initial_peaks.value_or(InitialPeakConfiguration{}).options);
            const auto &saved = source->preprocessing.initial_peak_usage;
            const bool reuse = source->projectSettings && saved &&
                saved->options == *usage.initial_peak_options &&
                reusableWorkflowStep(*source, request.workflow.value_or(source->workflow.active), WorkflowStep::initial_peaks) &&
                saved->spectrum && saved->spectrum->pattern().background && analysis.pattern().background &&
                saved->spectrum->pattern().observed == analysis.pattern().observed &&
                saved->spectrum->pattern().background->values == analysis.pattern().background->values;
            auto initial = reuse ? Result<preprocessing::InitialPeakResult>{saved->detection} :
                source->algorithms->modules.initial_peaks->find(analysis, wave,
                    *usage.initial_peak_options, legacyContext);
            if (!initial) return Return{initial.error()};
            valid = preprocessing::validate_initial_peak_candidates(initial.value(), analysis.pattern());
            if (!valid) return Return{valid.error()};
            if (source->projectSettings) {
                auto evidence = std::make_shared<InitialPeakUsage>();
                evidence->options = *usage.initial_peak_options;
                evidence->algorithm = source->algorithms->modules.initial_peaks->identity();
                evidence->source_revision = source->revision;
                evidence->source_pattern = std::make_shared<const Pattern>(request.pattern);
                evidence->spectrum = std::make_shared<const AnalysisSpectrum>(analysis);
                evidence->detection = initial.value();
                out.initial = reuse ? saved : std::move(evidence);
            }
            auto estimated = source->algorithms->modules.fwhm->estimate(analysis, initial.value(), legacyContext);
            if (!estimated) return Return{estimated.error()};
            valid = preprocessing::validate_fwhm_result(estimated.value(), initial.value(), analysis.pattern());
            if (!valid) return Return{valid.error()};
            if (context.cancellation.stop_requested())
                return Return{Error{ErrorCode::cancelled, "fwhm", "FWHM estimation canceled"}};
            out.gaussian = std::move(estimated).value();
            if (!out.gaussian->estimate.width) return Return{std::move(out)};
            usage.origin = FwhmOrigin::gaussian;
            usage.width = *out.gaussian->estimate.width; usage.width_count = out.gaussian->estimate.peak_count;
            usage.input_pattern = std::make_shared<const Pattern>(analysis.pattern());
        }
    }
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "fwhm", "FWHM estimation canceled"}};
    usage.algorithm = identity(usage.origin);
    valid = validateFwhmUsage(usage);
    if (!valid) return Return{valid.error()};
    out.configuration.default_fwhm = usage.width;
    out.usage = std::move(usage);
    return Return{std::move(out)};
}
Error configuredFwhmFailure(const ConfiguredFwhmEvaluation &out) {
    std::string message = "No eligible peak for FWHM estimation. Adjust the parameters or use a fixed Default FWHM.";
    if (out.geometric) {
        std::map<std::string, std::size_t> counts;
        for (const auto &candidate : out.geometric->candidates)
            if (!candidate.eligible) ++counts[preprocessing::fwhm_rejection_name(candidate.rejection)];
        for (const auto &[reason, count] : counts) message += "\n" + reason + ": " + std::to_string(count);
    }
    return {ErrorCode::missing_data, "fwhm", std::move(message), "no_eligible_fwhm"};
}
Status applyConfiguredFwhm(ProductState &next, const ConfiguredFwhmEvaluation &out) {
    if (!out.usage) return Status{configuredFwhmFailure(out)};
    if (!out.source || next.pattern.id != out.usage->dataset)
        return Status{Error{ErrorCode::stale_snapshot, "fwhm.source", "FWHM result belongs to another dataset"}};
    auto valid = validateFwhmConfiguration(out.configuration);
    if (!valid) return valid;
    valid = validateFwhmUsage(*out.usage);
    if (!valid) return valid;
    if (out.preparation) {
        next = *out.preparation;
        next.workflow.configuration_transition = false;
    }
    next.preprocessing.fwhm = out.configuration;
    next.preprocessing.fwhm_usage = out.usage;
    if (next.projectSettings) {
        if (out.usage->manual) next.workflow.active = WorkflowId::preprocessing;
        if (!next.pattern.background && out.usage->input_pattern && out.usage->input_pattern->background) {
            next.pattern.background = out.usage->input_pattern->background;
            next.projectMetadata["has_back"] = true;
            next.provenance.backgroundSource = AlgorithmRunSource{next.algorithms, SearchParameters{AngularWidthDegrees{0}}, out.source, out.source->revision};
            next.provenance.backgroundSource->background = next.preprocessing.background;
        }
        if (out.initial) {
            auto configuration = next.preprocessing.initial_peaks.value_or(InitialPeakConfiguration{});
            configuration.options = out.initial->options;
            next.preprocessing.initial_peaks = std::move(configuration);
            next.preprocessing.initial_peak_usage = out.initial;
        }
        next.workflow.completed_target = WorkflowStep::fwhm;
    }
    return Status{std::monostate{}};
}
Result<preprocessing::PeakCalibration> prepareConfiguredPeakParameters(const preprocessing::PeakOptions &options,
    const FwhmUsage &usage, WavelengthAngstrom wavelength, const ExecutionContext &context) {
    auto valid = validateFwhmUsage(usage);
    if (!valid) return Result<preprocessing::PeakCalibration>{valid.error()};
    if (usage.origin == FwhmOrigin::gaussian) {
        auto automatic = options;
        automatic.search_range = -1;
        return preprocessing::prepare_classic_peak_parameters(automatic,
            {preprocessing::FwhmEstimate::State::measured, usage.width, usage.width_count}, wavelength, context);
    }
    return preprocessing::prepare_supplied_fwhm_parameters(options, usage.width,
        usage.origin == FwhmOrigin::fixed ? preprocessing::PeakCalibration::Source::fixed_fwhm :
            preprocessing::PeakCalibration::Source::geometric_fwhm, context);
}
void invalidateAutomaticFwhm(PreprocessingConfiguration &c) {
    if (c.fwhm_usage && c.fwhm_usage->origin != FwhmOrigin::fixed) c.fwhm_usage->current = false;
}
} // namespace qualpf::application
