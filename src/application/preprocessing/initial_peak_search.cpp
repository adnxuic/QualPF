#include "qualpf/application/initial_peak_search.h"
#include "qualpf/application/preprocessing_workflows.h"
#include "qualpf/domain/validation.h"
#include "qualpf/application/workflow.h"
#include <algorithm>

namespace qualpf::application {
namespace {
Error invalid(const char *message) {
    return {ErrorCode::invalid_input, "peaks.initial.output", message, "invalid_algorithm_output"};
}
MetadataObject backgroundSettings(const PreprocessingConfiguration &c) {
    PreprocessingConfiguration settings;
    settings.background = c.background;
    settings.background_points = c.background_points;
    settings.background_filters = c.background_filters;
    return preprocessingValues(settings);
}
bool sameInput(const ProductState &a, const ProductState &b) {
    const auto &p = a.pattern, &q = b.pattern;
    if (p.id != q.id || p.source_id != q.source_id || p.two_theta != q.two_theta || p.observed != q.observed ||
        p.processing.background_subtracted != q.processing.background_subtracted ||
        p.processing.kalpha2_subtracted != q.processing.kalpha2_subtracted ||
        bool(p.background) != bool(q.background) || p.radiation.kind != q.radiation.kind ||
        p.radiation.lines.size() != q.radiation.lines.size()) return false;
    for (std::size_t i = 0; i < p.radiation.lines.size(); ++i)
        if (p.radiation.lines[i].wavelength != q.radiation.lines[i].wavelength ||
            p.radiation.lines[i].relative_weight != q.radiation.lines[i].relative_weight) return false;
    if (bool(p.calculation_range) != bool(q.calculation_range) ||
        (p.calculation_range && (p.calculation_range->minimum != q.calculation_range->minimum ||
            p.calculation_range->maximum != q.calculation_range->maximum))) return false;
    if (p.background && (p.background->values != q.background->values ||
        bool(p.background->window) != bool(q.background->window) ||
        (p.background->window && p.background->window->first != q.background->window->first))) return false;
    return a.algorithms && b.algorithms &&
        a.algorithms->spec.selections().initial_peaks == b.algorithms->spec.selections().initial_peaks &&
        a.algorithms->spec.selections().background == b.algorithms->spec.selections().background &&
        sameBackgroundOptions(a.algorithms->spec.defaults().background, b.algorithms->spec.defaults().background) &&
        backgroundSettings(a.preprocessing) == backgroundSettings(b.preprocessing);
}
} // namespace
Result<AnalysisSpectrum> prepareAnalysisSpectrum(ProductHandle source, Pattern pattern, const ExecutionContext &context) {
    using Return = Result<AnalysisSpectrum>;
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    if (!source || !source->algorithms || pattern.id != source->pattern.id)
        return Return{Error{ErrorCode::stale_snapshot, "peaks.initial.source", "Initial peak input changed"}};
    if (pattern.two_theta.empty() || pattern.two_theta.size() != pattern.observed.size())
        return Return{Error{ErrorCode::missing_data, "fwhm.pattern", "A continuous pattern is required"}};
    if (!pattern.processing.background_subtracted)
        return Return{Error{ErrorCode::missing_data, "background.state", "Subtraction history is unknown; restart from an explicit original input", "subtraction_state_unknown"}};
    if (!pattern.background && !*pattern.processing.background_subtracted) {
        if (pattern.radiation.lines.empty())
            return Return{Error{ErrorCode::missing_data, "pattern.radiation", "A wavelength is required to prepare the background"}};
        CommonPreprocessingRequest preparation;
        preparation.background = source->preprocessing.background.value_or(source->algorithms->spec.defaults().background);
        preparation.background_points = source->preprocessing.background_points;
        preparation.background_filters = source->preprocessing.background_filters;
        preparation.wavelength = pattern.radiation.lines.front().wavelength;
        auto prepared = evaluateCommonPreprocessing(source, std::move(pattern), preparation, context);
        if (!prepared) return Return{prepared.error()};
        pattern = std::move(prepared).value().working_pattern;
    }
    if (!pattern.background || !pattern.background->window || pattern.background->window->first != 0 ||
        pattern.background->values.size() != pattern.observed.size())
        return Return{Error{ErrorCode::missing_data, "fwhm.background", "FWHM estimation requires a complete aligned background", "background_alignment_unknown"}};
    return AnalysisSpectrum::create(std::move(pattern));
}
Status validateInitialPeakUsage(const InitialPeakUsage &u) {
    auto valid = preprocessing::validate_initial_peak_options(u.options);
    if (!valid) return valid;
    if (!u.source_pattern || !u.spectrum || u.algorithm.id.empty() || u.algorithm.version == 0 ||
        (u.algorithm.id == "qualpf.peaks.initial.cpp" && u.algorithm.version != 5) ||
        u.source_pattern->id != u.spectrum->pattern().id ||
        u.source_pattern->source_id != u.spectrum->pattern().source_id ||
        u.source_pattern->two_theta != u.spectrum->pattern().two_theta ||
        !validate(u.spectrum->pattern()).empty()) return Status{invalid("Invalid initial peak input evidence")};
    valid = preprocessing::validate_initial_peak_candidates(u.detection, u.spectrum->pattern());
    if (!valid) return valid;
    const auto &applied = u.detection.applied;
    if (applied.slope != u.options.slope || applied.automatic_window != u.options.automatic_window ||
        applied.polynomial_degree != u.options.polynomial_degree ||
        (!u.options.automatic_window && applied.half_window != u.options.half_window))
        return Status{invalid("Initial detection parameters disagree with the request")};
    return Status{std::monostate{}};
}
Result<InitialPeakEvaluation> evaluateInitialPeakSearch(
    ProductHandle source, InitialPeakConfiguration configuration, const ExecutionContext &context) {
    using Return = Result<InitialPeakEvaluation>;
    auto valid = preprocessing::validate_initial_peak_options(configuration.options);
    if (!valid) return Return{valid.error()};
    if (!source || !source->algorithms) return Return{Error{ErrorCode::missing_data, "peaks.initial.source", "Missing source"}};
    const auto original = source;
    ProductHandle preparation;
    if (source->projectSettings) {
        auto plan = prepareWorkflow(source, WorkflowId::preprocessing, WorkflowStep::initial_peaks);
        if (!plan) return Return{plan.error()};
        preparation = std::make_shared<const ProductState>(std::move(plan).value().next);
        source = preparation;
    }
    if (source->pattern.radiation.lines.empty())
        return Return{Error{ErrorCode::missing_data,"pattern.radiation","Initial peak search requires a wavelength"}};
    auto execution = context;
    execution.numerical_policy = NumericalPolicy::legacy_mixed_precision;
    auto analysis = prepareAnalysisSpectrum(source, source->pattern, execution);
    if (!analysis) return Return{analysis.error()};
    auto found = source->algorithms->modules.initial_peaks->find(analysis.value(),
        analysis.value().pattern().radiation.lines.front().wavelength, configuration.options, execution);
    if (!found) return Return{found.error()};
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    auto usage = std::make_shared<InitialPeakUsage>();
    usage->options = configuration.options;
    usage->algorithm = source->algorithms->modules.initial_peaks->identity();
    usage->source_revision = source->revision;
    usage->source_pattern = std::make_shared<const Pattern>(source->pattern);
    usage->spectrum = std::make_shared<const AnalysisSpectrum>(std::move(analysis).value());
    usage->detection = std::move(found).value();
    valid = validateInitialPeakUsage(*usage);
    if (!valid) return Return{valid.error()};
    return Return{InitialPeakEvaluation{original, std::move(configuration), std::move(usage), preparation}};
}
Status applyInitialPeakSearch(ProductState &next, const InitialPeakEvaluation &out) {
    if (!out.source || !out.usage || !sameInput(*out.source, next) ||
        out.usage->detection.dataset != next.pattern.id || out.configuration.options != out.usage->options)
        return Status{invalid("Initial peak result belongs to another input or configuration")};
    auto valid = validateInitialPeakUsage(*out.usage);
    if (!valid) return valid;
    if (out.preparation) {
        next = *out.preparation;
        next.workflow.configuration_transition = false;
    }
    if (next.preprocessing.initial_peaks || out.configuration != InitialPeakConfiguration{})
        next.preprocessing.initial_peaks = out.configuration;
    next.preprocessing.initial_peak_usage = out.usage;
    if (next.projectSettings) {
        next.workflow.active = WorkflowId::preprocessing;
        next.workflow.completed_target = WorkflowStep::initial_peaks;
        if (!next.pattern.background) {
            next.pattern.background = out.usage->spectrum->pattern().background;
            next.projectMetadata["has_back"] = true;
            next.provenance.backgroundSource = AlgorithmRunSource{next.algorithms, SearchParameters{AngularWidthDegrees{0}}, out.source, out.source->revision};
            next.provenance.backgroundSource->background = next.preprocessing.background;
        }
    }
    return Status{std::monostate{}};
}
void invalidateInitialPeaksAfterEdit(const ProductState &previous, ProductState &next) {
    const bool optionsChanged = previous.preprocessing.initial_peaks.value_or(InitialPeakConfiguration{}).options !=
        next.preprocessing.initial_peaks.value_or(InitialPeakConfiguration{}).options;
    if (next.preprocessing.initial_peak_usage &&
        next.preprocessing.initial_peak_usage == previous.preprocessing.initial_peak_usage &&
        (optionsChanged || !sameInput(previous, next)))
        next.preprocessing.initial_peak_usage.reset();
    if (optionsChanged && next.preprocessing.fwhm_usage &&
        next.preprocessing.fwhm_usage->origin == FwhmOrigin::gaussian &&
        (!next.preprocessing.fwhm_usage->initial_peak_options ||
         *next.preprocessing.fwhm_usage->initial_peak_options != next.preprocessing.initial_peaks.value_or(InitialPeakConfiguration{}).options))
        next.preprocessing.fwhm_usage->current = false;
}
} // namespace qualpf::application
