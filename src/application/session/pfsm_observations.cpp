#include "pfsm_workflow.h"
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/preprocessing_workflows.h"
#include "qualpf/domain/validation.h"
#include "qualpf/application/workflow.h"

namespace qualpf::application::pfsm_detail {
std::shared_ptr<const PfsmProblem> prepareProblem(const SearchInput &input, const PfsmRequest &request,
                                                  const ExecutionContext &context) {
    checkpoint(context);
    if (request.candidate_recall.mode != PfsmCandidateRecallMode::off && request.candidate_recall.mode != PfsmCandidateRecallMode::shadow &&
        request.candidate_recall.mode != PfsmCandidateRecallMode::enabled)
        fail("pfsm.fast", "Unknown fast mode");
    if (request.candidate_recall.version != PfsmCandidateRecallVersion::v2)
        fail("pfsm.fast", "Unknown fast version");
    const auto recallLimits = profile::validatePfsmCandidateRecallLimits(request.candidate_recall.limits);
    if (!recallLimits) throw Failure{recallLimits.error()};
    if (!request.observations)
        fail("pfsm.observations", "A new analysis requires explicit observation provenance");
    const auto &r = *request.observations;
    const auto &state = *input.source;
    const auto &p = state.pattern;
    if (!r.unclipped_signal || r.signal_source.empty() || r.intensity_units.empty() || r.noise_source.empty())
        fail("pfsm.provenance", "Unclipped-signal declaration, intensity units and "
                                "signal/noise sources are required");
    if (!std::isfinite(request.lower_shift) || !std::isfinite(request.upper_shift) ||
        request.lower_shift > request.upper_shift)
        fail("pfsm.bounds", "Invalid shift bounds");
    if (p.radiation.lines.empty())
        fail("pfsm.radiation", "Explicit radiation components are required");
    Pattern measured = p;
    if (r.signal == PfsmObservationRequest::Signal::original_observed) {
        if (!p.original_observed || p.original_observed->size() != p.two_theta.size() ||
            !p.original_kalpha2_subtracted || !r.original_intensity_scaled.has_value())
            fail("pfsm.provenance", "Original observations and their radiation "
                                    "and intensity-scaling provenance are required");
        measured.observed = *p.original_observed;
        measured.processing.background_subtracted = false;
        measured.processing.kalpha2_subtracted = p.original_kalpha2_subtracted;
        measured.processing.intensity_scaled = r.original_intensity_scaled;
        measured.intensity_scale = IntensityScale::unspecified;
    } else if (r.signal == PfsmObservationRequest::Signal::observed) {
        if (p.processing.background_subtracted != false || !state.provenance.residualHistory.empty() ||
            !state.provenance.subtractedBackgroundHistory.empty())
            fail("pfsm.provenance", "Working observations do not attest an "
                                    "unsubtracted signal; explicitly "
                                    "restore original data");
    } else
        fail("pfsm.signal", "Unknown signal selection");
    if (r.radiation) measured.radiation = *r.radiation;
    if (measured.radiation.lines.empty()) fail("pfsm.radiation", "Explicit radiation components are required");
    const auto measuredIssues = validate(measured);
    if (!measuredIssues.empty()) throw Failure{measuredIssues.front()};
    if (measured.processing.kalpha2_subtracted == true && measured.radiation.lines.size() != 1)
        fail("pfsm.radiation", "Stripped observations require their explicit remaining radiation component");
    const auto n = measured.observed.size();
    if (n < 3 || measured.two_theta.size() != n)
        fail("pfsm.observations", "Expected an aligned experimental spectrum");
    for (const auto *mask : {&r.valid, &r.saturated, &r.break_before})
        if (!mask->empty() && mask->size() != n)
            fail("pfsm.mask", "Mask dimensions disagree");
    auto data = std::make_shared<profile::ProfileObservations>();
    data->observed = measured.observed;
    for (auto x : measured.two_theta)
        data->coordinates.push_back(x.value);
    std::vector<preprocessing::BackgroundPoint> backgroundPoints;
    if (r.background) {
        data->background = *r.background;
        if (r.signal == PfsmObservationRequest::Signal::observed && p.background &&
            p.background->values == data->background && state.preprocessing.background_points)
            backgroundPoints = *state.preprocessing.background_points;
    } else if (r.signal == PfsmObservationRequest::Signal::observed && measured.background &&
        reusableWorkflowStep(state, WorkflowId::pfsm, WorkflowStep::background)) {
        take(AnalysisSpectrum::create(measured));
        data->background = measured.background->values;
        if (state.preprocessing.background_points) backgroundPoints = *state.preprocessing.background_points;
    } else {
        measured.background.reset();
        measured.background_subtracted_signal.reset();
        CommonPreprocessingRequest preparation;
        preparation.background = state.projectSettings ? state.projectSettings->profile(WorkflowId::pfsm).background :
            state.preprocessing.background.value_or(state.algorithms->spec.defaults().background);
        if (!state.projectSettings) {
            preparation.background_points = state.preprocessing.background_points;
            preparation.background_filters = state.preprocessing.background_filters;
        }
        preparation.wavelength = measured.radiation.lines.front().wavelength;
        auto prepared = take(evaluateCommonPreprocessing(input.source, measured, preparation, context));
        data->background = prepared.working_pattern.background->values;
        if (prepared.background) backgroundPoints = prepared.background->points;
    }
    measured.background = SampledSignal{data->background, SampleWindow{0}};
    take(AnalysisSpectrum::create(measured));
    if (data->background.size() != n)
        fail("pfsm.background", "Background must be aligned and in the observation units");
    data->weights.resize(n);
    std::vector<double> variances(n);
    const bool unknown = std::holds_alternative<std::monostate>(r.noise);
    if (unknown != r.equal_weights)
        fail("pfsm.weights", "Explicit equal weighting is required only when a "
                             "noise model is unavailable");
    if (const auto *s = std::get_if<preprocessing::FwhmStandardDeviations>(&r.noise)) {
        if (s->values.size() != n)
            fail("pfsm.variance", "Standard deviations must be aligned");
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isfinite(s->values[i]) || s->values[i] <= 0)
                fail("pfsm.variance", "Positive finite standard deviations are required");
            variances[i] = s->values[i] * s->values[i];
        }
    } else if (std::holds_alternative<preprocessing::FwhmPoissonCounts>(r.noise)) {
        if (measured.processing.intensity_scaled == true || measured.intensity_scale == IntensityScale::relative)
            fail("pfsm.counts", "Scaled or relative intensity cannot be declared raw Poisson counts; "
                                "supply variance propagated in the observation units");
        for (std::size_t i = 0; i < n; ++i) {
            if (!std::isfinite(measured.observed[i]) || measured.observed[i] < 0)
                fail("pfsm.counts", "Declared raw counts must be nonnegative and finite");
            variances[i] = std::max(1., measured.observed[i]);
        }
    } else if (const auto *rates = std::get_if<preprocessing::FwhmPoissonRates>(&r.noise)) {
        if (rates->counts.size() != n || rates->seconds.size() != n)
            fail("pfsm.counts", "Counts and exposure arrays must be aligned");
        for (std::size_t i = 0; i < n; ++i) {
            const double counts = rates->counts[i], seconds = rates->seconds[i];
            if (!std::isfinite(counts) || counts < 0 || !std::isfinite(seconds) || seconds <= 0 ||
                std::abs(measured.observed[i] - counts / seconds) >
                    32 * std::numeric_limits<double>::epsilon() *
                        std::max(1., std::abs(measured.observed[i])))
                fail("pfsm.counts", "Count rates disagree with supplied counts/exposure");
            variances[i] = std::max(1., counts) / (seconds * seconds);
        }
    } else if (const auto *cov = std::get_if<preprocessing::FwhmCovariance>(&r.noise)) {
        if (cov->diagonal.size() != n)
            fail("pfsm.variance", "Covariance diagonal must be aligned");
        if (!cov->upper.empty())
            fail("pfsm.variance",
                 "Correlated errors require a generalized objective outside the "
                 "fixed diagonal-weight PFSM "
                 "contract",
                 ErrorCode::unsupported);
        variances = cov->diagonal;
    }
    for (std::size_t i = 0; i < n; ++i) {
        checkpoint(context);
        if (!std::isfinite(data->coordinates[i]) || (i && data->coordinates[i] <= data->coordinates[i - 1]) ||
            !std::isfinite(data->observed[i]) || !std::isfinite(data->background[i]))
            fail("pfsm.observations", "Finite intensities and ascending finite coordinates are required");
        if (!unknown &&
            (!std::isfinite(variances[i]) || variances[i] <= 0 || !std::isfinite(1 / variances[i])))
            fail("pfsm.weights", "Variance cannot produce a finite positive weight");
        const bool valid = (r.valid.empty() || r.valid[i]) && (r.saturated.empty() || !r.saturated[i]);
        data->weights[i] = valid ? (unknown ? 1 : 1 / variances[i]) : 0;
    }
    GeometricFwhmRequest geometric;
    geometric.options = r.width_options;
    geometric.input.dataset = p.id;
    geometric.input.source_id = r.signal_source;
    geometric.input.radiation = measured.radiation;
    geometric.input.processing = measured.processing;
    geometric.input.two_theta = measured.two_theta;
    geometric.input.intensity = data->observed;
    geometric.input.background = data->background;
    geometric.input.noise = r.noise;
    geometric.input.noise_source = r.noise_source;
    geometric.input.valid = r.valid;
    geometric.input.saturated = r.saturated;
    geometric.input.break_before = r.break_before;
    auto problem = std::make_shared<PfsmProblem>();
    problem->observations = std::move(data);
    problem->background_points = std::move(backgroundPoints);
    problem->radiation = measured.radiation;
    problem->source = r;
    problem->original_signal_provenance = state.original_signal_provenance;
    if (request.supplied_width) {
        const auto &supplied = *request.supplied_width;
        take(validateFwhmUsage(supplied));
        if (supplied.origin != FwhmOrigin::fixed && !compatibleManualPfsmWidth(state, supplied))
            fail("pfsm.fwhm", "Manual FWHM evidence is incompatible with the current input",
                 ErrorCode::stale_snapshot);
        problem->physical_width = supplied;
        if (supplied.geometric)
            problem->width = *supplied.geometric;
    } else {
        auto width = take(evaluateGeometricFwhm(input.source, geometric, context));
        problem->width = std::move(width.measurement);
        if (problem->width.default_fwhm) {
            FwhmUsage usage;
            usage.width = *problem->width.default_fwhm;
            usage.origin = FwhmOrigin::geometric;
            usage.algorithm = problem->width.algorithm;
            usage.dataset = p.id;
            usage.source_id = r.signal_source;
            usage.source_revision = state.revision;
            usage.minimum_sampling_density = r.width_options.minimum_samples_per_fwhm;
            usage.width_count = 1;
            usage.current = true;
            usage.geometric = std::make_shared<const preprocessing::GeometricFwhmResult>(problem->width);
            problem->physical_width = std::move(usage);
        }
    }
    problem->algorithms = state.algorithms;
    problem->lower_shift = request.lower_shift;
    problem->upper_shift = request.upper_shift;
    problem->fit = request.fit;
    problem->template_cache = request.template_cache;
    problem->candidate_recall = request.candidate_recall;
    return problem;
}
profile::ProfileExpansionInput expansionInput(const ProductCard &card, const PfsmProblem &problem) {
    if (card.d.size() != card.intensity.size())
        fail("pfsm.reference", "Reference requires paired d/I records");
    profile::ProfileExpansionInput input;
    input.fwhm_degrees = problem.physical_width->width.value;
    input.radiation = problem.radiation;
    if (problem.template_cache)
        input.interpolation_window = {problem.observations->coordinates.front() - problem.upper_shift,
                                      problem.observations->coordinates.back() - problem.lower_shift};
    for (std::size_t i = 0; i < card.d.size(); ++i)
        input.lines.push_back({i, card.d[i], card.intensity[i]});
    return input;
}
profile::ProfileTemplate expand(const ProductCard &card, const PfsmProblem &problem,
                                const ExecutionContext &context) {
    return take(problem.algorithms->modules.profile_expander->prepare(expansionInput(card, problem), context));
}
} // namespace qualpf::application::pfsm_detail

namespace qualpf::application {
Result<ProductState> evaluatePfsmPreparation(ProductHandle source, const ExecutionContext &context) {
    if (!source || !source->projectSettings || source->pfsm)
        return Result<ProductState>{Error{ErrorCode::invalid_input,"pfsm.preparation","A new project PFSM analysis is required"}};
    auto plan = prepareWorkflow(source, WorkflowId::pfsm, WorkflowStep::search);
    if (!plan) return Result<ProductState>{plan.error()};
    auto next = std::move(plan).value().next;
    next.workflow.configuration_transition = false;
    auto working = std::make_shared<const ProductState>(next);
    auto request = makePfsmRequest(working, next.projectSettings->profile(WorkflowId::pfsm).pfsm);
    if (!request) return Result<ProductState>{request.error()};
    auto problem = preparePfsmProblem(working, request.value(), context);
    if (!problem) return Result<ProductState>{problem.error()};
    if (!problem.value()->physical_width) {
        ConfiguredFwhmEvaluation diagnostic;
        diagnostic.geometric = std::make_shared<const preprocessing::GeometricFwhmResult>(problem.value()->width);
        return Result<ProductState>{configuredFwhmFailure(diagnostic)};
    }
    next.prepared_pfsm = problem.value();
    if (!next.pattern.background) {
        next.pattern.background = SampledSignal{problem.value()->observations->background, SampleWindow{0}};
        next.preprocessing.background_points = problem.value()->background_points;
        next.projectMetadata["has_back"] = true;
        next.provenance.backgroundSource = AlgorithmRunSource{next.algorithms, SearchParameters{AngularWidthDegrees{0}}, source, source->revision};
        next.provenance.backgroundSource->background = next.preprocessing.background;
    }
    if (!reusableWorkflowStep(*source, WorkflowId::pfsm, WorkflowStep::fwhm)) {
        next.preprocessing.fwhm_usage = problem.value()->physical_width;
        next.preprocessing.fwhm->default_fwhm = problem.value()->physical_width->width;
    }
    return Result<ProductState>{std::move(next)};
}
Result<std::shared_ptr<const PfsmProblem>>
preparePfsmProblem(ProductHandle source, const PfsmRequest &request, const ExecutionContext &context) try {
    if (!source || !source->algorithms)
        pfsm_detail::fail("pfsm.source", "Missing PFSM source", ErrorCode::missing_data);
    if (source->algorithms->spec.method() != SearchMethod::pfsm || source->editing)
        pfsm_detail::fail("pfsm.source", "A closed PFSM source is required");
    SearchInput input;
    input.source = std::move(source);
    return Result<std::shared_ptr<const PfsmProblem>>{pfsm_detail::prepareProblem(input, request, context)};
} catch (const pfsm_detail::Failure &failure) {
    return Result<std::shared_ptr<const PfsmProblem>>{failure.error};
} catch (const std::exception &error) {
    return Result<std::shared_ptr<const PfsmProblem>>{
        Error{ErrorCode::numerical_failure, "pfsm.preparation", error.what()}};
}
} // namespace qualpf::application
