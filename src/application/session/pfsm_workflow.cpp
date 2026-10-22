#include "pfsm_workflow.h"
#include "pfsm_candidate_recall.h"
#include "pfsm_formal_fit.h"
#include "pfsm_templates.h"
#include "pfsm_recall_store.h"
#include "qualpf/search/rir.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/configured_fwhm.h"
#include <atomic>
#include <cfenv>
#include <numeric>
#include <omp.h>
#include <set>

namespace qualpf::application {
namespace {
using namespace pfsm_detail;
using namespace profile;
ProfileFitInput fitInput(const PfsmAnalysis &analysis, PreparedProfileObservationsHandle prepared) {
    ProfileFitInput input;
    input.observations = prepared ? profileObservationSnapshot(prepared) : analysis.problem->observations;
    input.prepared_observations = std::move(prepared);
    input.options = analysis.problem->fit;
    input.phases = analysis.selected;
    return input;
}
double baselineRoundoff(const ProfileFitResult &baseline) {
    return 128 * std::numeric_limits<double>::epsilon() * std::max(baseline.denominator_net, baseline.q);
}
void refineBaseline(PfsmAnalysis &analysis, ProfileFitInput input, const ExecutionContext &context) {
    const auto &fitter = *analysis.problem->algorithms->modules.profile_fitter;
    const auto fit = [&] {
        checkpoint(context);
        const auto begin = Clock::now();
        auto result = checkedFit(fitter.fit(input, context), input);
        analysis.performance.solve_seconds += elapsed(begin);
        analysis.performance.residual_evaluations += result.diagnostics.residual_evaluations;
        ++analysis.performance.baseline_refinement_fits;
        return result;
    };
    auto best = fit();
    if (input.phases.empty()) {
        analysis.baseline = std::move(best);
        return;
    }
    // Cost convergence of a single fit need not meet the much finer common-
    // baseline consistency check. Confirm stability before scoring the library,
    // using unchanged solver tolerances and a bounded sequence of warm starts.
    for (std::size_t confirmation = 0; confirmation < 32; ++confirmation) {
        input.starts = {best.parameters.shifts};
        input.fallback = best.parameters;
        for (std::size_t j = 0; j < input.phases.size(); ++j)
            input.phases[j].initial_shift = best.parameters.shifts[j];
        auto trial = fit();
        const double improvement = best.q - trial.q;
        const double tolerance = baselineRoundoff(best);
        if (trial.q < best.q)
            best = std::move(trial);
        if (improvement <= tolerance) {
            analysis.baseline = std::move(best);
            return;
        }
    }
    fail("pfsm.baseline", "Baseline did not stabilize within 32 confirmation fits",
         ErrorCode::numerical_failure, "unstable_pfsm_baseline");
}
// One immutable baseline sample per selected phase, shared by candidate readers.
// Any moved shift/coordinate or exact verification delegates to the original.
class BaselineTemplate final : public IProfileTemplate {
    ProfileTemplate underlying_;
    std::shared_ptr<const ProfileObservations> data_;
    double shift_;
    ProfileSamples samples_;

  public:
    BaselineTemplate(ProfileTemplate profile, std::shared_ptr<const ProfileObservations> data, double shift,
                     const ExecutionContext &context)
        : underlying_(std::move(profile)), data_(std::move(data)), shift_(shift),
          samples_(take(underlying_->evaluate(data_->coordinates, shift, false, context))) {}
    double fwhmDegrees() const noexcept override { return underlying_->fwhmDegrees(); }
    bool usesApproximation() const noexcept override { return underlying_->usesApproximation(); }
    Result<ProfileSamples> evaluateExact(std::span<const double> x, double shift, bool derivative,
                                         const ExecutionContext &context) const override {
        return underlying_->evaluateExact(x, shift, derivative, context);
    }
    Result<ProfileSamples> evaluate(std::span<const double> x, double shift, bool derivative,
                                    const ExecutionContext &context) const override {
        if (context.cancellation.stop_requested())
            return Result<ProfileSamples>{Error{ErrorCode::cancelled, "pfsm", "PFSM canceled"}};
        if (!derivative && shift == shift_ && x.data() == data_->coordinates.data() &&
            x.size() == data_->coordinates.size())
            return Result<ProfileSamples>{samples_};
        return underlying_->evaluate(x, shift, derivative, context);
    }
};
// The immutable problem binds observations, radiation, algorithms and formal options.
// This cache exists only inside one operation and is invalidated on any baseline escape.
struct FormalReuseBinding {
    const PfsmProblem *problem;
    const std::vector<ProductCard> *universe;
    const ProductState *source;
    std::uint64_t generation, database_version;
    std::size_t baseline_version;
    double baseline_q;
    std::vector<double> scales, shifts;
    friend bool operator==(const FormalReuseBinding &,const FormalReuseBinding &) = default;
};
struct ReusableFormal {
    std::string identity;
    PfsmFormalCandidateResult result;
};
ProductCard scoredCard(const ProductCard &reference,
                       const std::shared_ptr<const PfsmCandidateEvidence> &evidence,
                       const AlgorithmRunSource &origin) {
    // Reflection arrays remain in the shared discrete universe, not in every
    // published ranking copy. Acceptance retrieves them by checked identity.
    ProductCard card;
    card.id = reference.id;
    card.chemicalName = reference.chemicalName;
    card.chemicalFormula = reference.chemicalFormula;
    card.mineralName = reference.mineralName;
    card.quality = reference.quality;
    card.rir = reference.rir;
    card.spaceGroup = reference.spaceGroup;
    card.fom = evidence->delta_rwp;
    card.scale = evidence->parameters.scales.back();
    card.fomCalculated = true;
    card.pfsm = evidence;
    card.algorithmSource = std::make_shared<const CardAlgorithmSource>(CardAlgorithmSource{origin, origin});
    return card;
}
} // namespace
Result<PreparedSearchResult> runPfsm(const SearchInput &input, CandidateReader &reader,
                                     const ExecutionContext &context, SearchProgress progress) try {
    using namespace pfsm_detail;
    const auto stage=[&](const char *name) {if(input.workflowProgress)input.workflowProgress(name);};
    checkpoint(context);
    if (context.numerical_policy != NumericalPolicy::explicit_float64)
        fail("pfsm.precision", "PFSM requires explicit binary64 execution");
    if (input.operation != SearchOperation::initial && input.operation != SearchOperation::cli_match &&
        input.operation != SearchOperation::scored_browse && input.operation != SearchOperation::rescore &&
        input.operation != SearchOperation::accept_phase && input.operation != SearchOperation::remove_phase)
        fail("pfsm.operation", "PFSM supports initial search, rescore and joint phase acceptance/removal",
             ErrorCode::unsupported);
    const bool initial = input.operation == SearchOperation::initial ||
                         input.operation == SearchOperation::cli_match ||
                         input.operation == SearchOperation::scored_browse;
    if (input.merge)
        fail("pfsm.merge",
             "PFSM rankings require a common baseline and cannot merge independent "
             "scores",
             ErrorCode::unsupported);
    PfsmRequest request = input.pfsm ? *input.pfsm : PfsmRequest{};
    if (request.threads < 0)
        fail("pfsm.threads", "Thread count cannot be negative");
    PreparedSearchResult out{input.source, input.taskId, input.databaseVersion, *input.source};
    out.next.analysis_database = input.databaseIdentity;
    auto analysis = std::make_shared<PfsmAnalysis>();
    if (initial) {
        if (!input.source->phases.empty())
            fail("pfsm.phases", "Start a new PFSM analysis without existing phases "
                                "or rescore the current analysis");
        analysis->problem = input.source->prepared_pfsm ? input.source->prepared_pfsm : prepareProblem(input, request, context);
        if (!analysis->problem->physical_width) {
            ConfiguredFwhmEvaluation diagnostic;
            diagnostic.geometric = std::make_shared<const preprocessing::GeometricFwhmResult>(analysis->problem->width);
            auto error = configuredFwhmFailure(diagnostic);
            error.field = "pfsm.fwhm";
            error.reason = "no_eligible_geometric_fwhm";
            throw Failure{std::move(error)};
        }
        auto query = take(planCandidateQuery(input.query, true, context));
        // Peak-table prefilters belong to Classical; PFSM uses explicit common
        // restraints only.
        query.route = CandidateQuery::Route::constrained_browse;
        query.d.clear();
        query.deltaD.clear();
        query.wavelength = 0;
        query.strictPairedReflections = true;
        query.measure_read = request.measure_search_work;
        analysis->query = query;
        const auto start = Clock::now();
        stage("Candidate Recall — reference library");
        auto cards = take(reader.read(query, context, progress));
        analysis->performance.database_seconds = elapsed(start);
        analysis->performance.database_read = reader.readStatistics();
        std::set<std::string> identities;
        for (const auto &card : cards) {
            checkpoint(context);
            if (card.id.empty() || !identities.insert(card.id).second)
                fail("pfsm.reference", "Candidate identities must be unique and nonempty");
            if (card.d.size() != card.intensity.size())
                fail("pfsm.reference", "Candidate library contains unpaired reflections");
        }
        analysis->universe = std::make_shared<const std::vector<ProductCard>>(std::move(cards));
        auto quantification = std::make_shared<PfsmQuantificationLibrary>();
        quantification->universe = analysis->universe;
        for (const auto &card : *analysis->universe) {
            checkpoint(context);
            quantification->references.emplace(card.id, PfsmQuantificationReference{
                take(search::rir_reference_maximum(card.intensity)), card.referenceRir, card.rir});
        }
        analysis->quantification_library = std::move(quantification);
        analysis->generation = 1;
        analysis->database_version = input.databaseVersion;
    } else {
        if (!input.source->pfsm)
            fail("pfsm.source", "No frozen PFSM analysis to continue", ErrorCode::missing_data);
        if (request.observations || request.supplied_width)
            fail("pfsm.observations", "Continuing an analysis cannot replace its "
                                      "frozen observation problem");
        const auto &old = *input.source->pfsm;
        if (old.problem->algorithms != input.source->algorithms ||
            (old.database_version != input.databaseVersion && input.source->analysis_database.empty()))
            fail("pfsm.source", "Algorithm or database source changed", ErrorCode::stale_snapshot);
        *analysis = old;
        analysis->database_version = input.databaseVersion;
        analysis->performance = {};
        analysis->candidate_recall_reports.clear();
        if (analysis->generation == std::numeric_limits<std::uint64_t>::max())
            fail("pfsm.generation", "PFSM generation exhausted", ErrorCode::exhausted_revision);
        ++analysis->generation;
        for (std::size_t j = 0; j < analysis->selected.size(); ++j)
            analysis->selected[j].initial_shift = old.baseline.parameters.shifts[j];
    }
    analysis->origin = {input.source->algorithms, input.parameters, input.source,
                        input.source->revision,   input.taskId,     input.databaseVersion};
    const auto observations = dynamic_cast<const NnlsTrfProfileFitter *>(
        analysis->problem->algorithms->modules.profile_fitter.get())
        ? take(prepareProfileObservations(analysis->problem->observations, context)) : PreparedProfileObservationsHandle{};
    auto baselineInput = fitInput(*analysis, observations);
    if (!initial)
        baselineInput.fallback = analysis->baseline.parameters;
    if (input.operation == SearchOperation::remove_phase) {
        const auto found = std::find_if(analysis->selected.begin(), analysis->selected.end(),
                                       [&](const auto &p) { return p.identity == input.cardId; });
        if (found == analysis->selected.end())
            fail("pfsm.phase", "Selected phase is no longer present", ErrorCode::stale_snapshot);
        const auto j = std::size_t(found - analysis->selected.begin());
        analysis->selected.erase(found);
        out.next.phases.erase(out.next.phases.begin() + j);
        baselineInput.phases = analysis->selected;
        baselineInput.fallback->scales.erase(baselineInput.fallback->scales.begin() + j);
        baselineInput.fallback->shifts.erase(baselineInput.fallback->shifts.begin() + j);
        baselineInput.starts = {baselineInput.fallback->shifts};
    }
    if (input.operation == SearchOperation::accept_phase) {
        if (!input.phase)
            fail("pfsm.phase", "Missing accepted candidate");
        const auto &id = input.phase->id;
        if (std::any_of(analysis->selected.begin(), analysis->selected.end(),
                        [&](const auto &p) { return p.identity == id; })) {
            out.changed = false;
            return Result<PreparedSearchResult>{std::move(out)};
        }
        const auto found = std::find_if(input.source->candidates.begin(), input.source->candidates.end(),
                                        [&](const auto &p) { return p.id == id; });
        if (found == input.source->candidates.end() || !found->pfsm || found->pfsm != input.phase->pfsm ||
            found->pfsm->generation != input.source->pfsm->generation)
            fail("pfsm.phase", "Candidate evidence is stale", ErrorCode::stale_snapshot);
        const auto index = found->pfsm->original_index;
        if (index >= analysis->universe->size() || (*analysis->universe)[index].id != id)
            fail("pfsm.phase", "Candidate identity disagrees with its frozen library");
        const auto &card = (*analysis->universe)[index];
        auto phase =
            ProfileFitPhase{id, expand(card, *analysis->problem, context), analysis->problem->lower_shift,
                            analysis->problem->upper_shift, found->pfsm->parameters.shifts.back()};
        analysis->selected.push_back(phase);
        baselineInput.phases = analysis->selected;
        baselineInput.starts.push_back(found->pfsm->parameters.shifts);
        baselineInput.fallback = found->pfsm->parameters;
        out.next.phases.push_back(card);
    }
    stage("Joint Baseline");
    refineBaseline(*analysis, std::move(baselineInput), context);
    std::set<std::string> selected;
    for (const auto &phase : analysis->selected)
        selected.insert(phase.identity);
    std::vector<std::size_t> indices;
    for (std::size_t i = 0; i < analysis->universe->size(); ++i)
        if (!selected.contains((*analysis->universe)[i].id))
            indices.push_back(i);
    const int threads = request.threads ? request.threads : omp_get_max_threads();
    analysis->performance.threads = threads;
    const auto allIndices = indices;
    const auto entryBaseline = analysis->baseline;
    std::string fullReason = request.full_round ? "requested_full_round" : "";
    const bool reuseEnabled = analysis->problem->candidate_recall.version == PfsmCandidateRecallVersion::v2 &&
                              analysis->problem->candidate_recall.mode == PfsmCandidateRecallMode::enabled;
    std::map<std::size_t, ReusableFormal> formalCache;
    std::optional<FormalReuseBinding> cacheBinding;
    bool supplementPending = false;
    const auto templates = analysis->problem->candidate_recall.mode != PfsmCandidateRecallMode::off &&
        dynamic_cast<const IntermediateLorentzianExpander *>(
        analysis->problem->algorithms->modules.profile_expander.get())
        ? std::make_shared<PfsmTemplateStore>(analysis->problem, analysis->universe,
            128 * 1024 * 1024, analysis->template_store) : nullptr;
    for (std::size_t rebuild = 0;;) {
        checkpoint(context);
        if (supplementPending) { indices = allIndices; supplementPending = false; }
        else {
            stage("Candidate Recall");
            auto recalled = recallPfsmCandidates({analysis->problem, analysis->universe, analysis->selected,
                analysis->baseline, analysis->generation, analysis->candidate_recall_data,
                allIndices, threads, rebuild, fullReason, templates, analysis->integral_recall_data,
                request.measure_search_work, analysis->recall_store}, context);
            indices = std::move(recalled.formal_indices);
            analysis->candidate_recall_data = std::move(recalled.data);
            analysis->integral_recall_data = std::move(recalled.integral_data);
            analysis->recall_store = std::move(recalled.recall_store);
            if (recalled.report) analysis->candidate_recall_reports.push_back(std::move(*recalled.report));
            auto &timing = analysis->performance;
            timing.recall_cache_hits += recalled.cache_hits;
            timing.recall_cache_misses += recalled.cache_misses;
            timing.recall_cache_budget_fallbacks += recalled.cache_budget_fallbacks;
            timing.recall_cache_bytes = analysis->recall_store ? analysis->recall_store->bytes() : 0;
            timing.detailed_work = request.measure_search_work;
            timing.correlation_work += recalled.timing.correlation_work;
            timing.initial_work += recalled.timing.initial_work;
            timing.review_work += recalled.timing.review_work;
            timing.recall_selection_seconds += recalled.timing.selection;
            timing.candidate_recall_shared_seconds += recalled.timing.shared;
            timing.candidate_recall_correlation_seconds += recalled.timing.correlation;
            timing.candidate_recall_initial_seconds += recalled.timing.initial;
            timing.candidate_recall_review_seconds += recalled.timing.review;
            timing.candidate_recall_local_seconds = timing.candidate_recall_initial_seconds + timing.candidate_recall_review_seconds;
        }
        if (reuseEnabled) {
            FormalReuseBinding binding{analysis->problem.get(), analysis->universe.get(), input.source.get(),
                analysis->generation, analysis->database_version, rebuild, analysis->baseline.q,
                analysis->baseline.parameters.scales, analysis->baseline.parameters.shifts};
            if (!cacheBinding || *cacheBinding != binding) formalCache.clear();
            cacheBinding = std::move(binding);
        }
        analysis->normalization_over_full_pool = indices == allIndices;
        stage("Profile Expansion / Profile Fitting");
        const auto baselineStart = Clock::now();
        auto baselinePhases = analysis->selected;
        for (std::size_t j = 0; j < baselinePhases.size(); ++j) {
            auto &phase = baselinePhases[j];
            phase.initial_shift = analysis->baseline.parameters.shifts[j];
            phase.profile = std::make_shared<const BaselineTemplate>(
                phase.profile, observations ? profileObservationSnapshot(observations) : analysis->problem->observations,
                phase.initial_shift, context);
        }
        analysis->performance.expansion_seconds += elapsed(baselineStart);
        std::vector<PfsmFormalCandidateResult> results(indices.size());
        std::vector<unsigned char> reused(reuseEnabled ? indices.size() : 0, 0);
        if (reuseEnabled) for (std::size_t slot = 0; slot < indices.size(); ++slot) {
            const auto found = formalCache.find(indices[slot]);
            if (found == formalCache.end()) continue;
            if (found->second.identity != (*analysis->universe)[indices[slot]].id ||
                found->second.result.evidence->generation != analysis->generation)
                fail("pfsm.reuse", "Formal reuse binding disagrees", ErrorCode::stale_snapshot);
            results[slot] = found->second.result; reused[slot] = 1;
        }
        std::atomic<bool> failed{false};
        std::atomic<int> actualThreads{1};
        std::atomic<std::size_t> completed{0};
        std::fenv_t environment;
        std::fegetenv(&environment);
        const auto workersStart = Clock::now();
// SQL is finished. Each worker owns all matrices, decompositions, caches
// and temporary curves. The implicit barrier joins every worker on exit.
#pragma omp parallel for schedule(dynamic, 1) num_threads(threads)
        for (std::ptrdiff_t slot = 0; slot < static_cast<std::ptrdiff_t>(indices.size()); ++slot) {
            actualThreads.store(omp_get_num_threads(), std::memory_order_relaxed);
            if ((reuseEnabled && reused[std::size_t(slot)]) || failed.load(std::memory_order_relaxed))
                continue;
            std::fenv_t previous;
            std::fegetenv(&previous);
            std::fesetenv(&environment);
            try {
                checkpoint(context);
                auto formalInput = fitInput(*analysis, observations);
                formalInput.phases = baselinePhases;
                const auto &problem = *analysis->problem;
                const auto &card = (*analysis->universe)[indices[slot]];
                const auto preparationStart = Clock::now();
                auto expansion = templates ? *templates->input(indices[slot]) : expansionInput(card, problem);
                const auto prepared = templates ? templates->prepare(indices[slot], expansion, context)
                                                : PreparedProfileTemplateHandle{};
                const auto preparationSeconds = elapsed(preparationStart);
                auto fitted = fitPfsmCandidate({std::move(formalInput), std::move(expansion), card.id,
                    indices[slot], analysis->generation, problem.lower_shift, problem.upper_shift, prepared},
                    *problem.algorithms->modules.profile_expander, *problem.algorithms->modules.profile_fitter,
                    analysis->baseline, context);
                fitted.expansion_seconds += preparationSeconds;
                results[std::size_t(slot)] = std::move(fitted);
                const auto done = completed.fetch_add(1, std::memory_order_relaxed) + 1;
                // OpenMP's primary thread is the calling execution thread.
                // Publish progress there only, while other workers remain private.
                if (progress && omp_get_thread_num() == 0)
                    progress(done, results.size());
            } catch (const Failure &failure) {
                results[std::size_t(slot)].error = failure.error;
            } catch (const std::exception &error) {
                results[std::size_t(slot)].error =
                    Error{ErrorCode::numerical_failure, "pfsm.worker", error.what()};
            } catch (...) {
                results[std::size_t(slot)].error =
                    Error{ErrorCode::numerical_failure, "pfsm.worker", "Unknown worker failure"};
            }
            std::fesetenv(&previous);
            if (results[std::size_t(slot)].error)
                failed.store(true, std::memory_order_relaxed);
        }
        analysis->performance.candidate_wall_seconds += elapsed(workersStart);
        analysis->performance.threads = actualThreads.load();
        checkpoint(context);
        if (templates)
            analysis->performance.templates = templates->statistics();
        for (const auto &result : results)
            if (result.error)
                throw Failure{*result.error};
        if (progress)
            progress(results.size(), results.size());
        stage("FOM / Results");
        const PfsmCandidateEvidence *better = nullptr;
        for (std::size_t slot = 0; slot < results.size(); ++slot) {
            auto &result = results[slot];
            if (result.error)
                throw Failure{*result.error};
            if (reuseEnabled && reused[slot]) ++analysis->performance.formal_reused;
            else {
                analysis->performance.expansion_seconds += result.expansion_seconds;
                analysis->performance.solve_seconds += result.solve_seconds;
                analysis->performance.residual_evaluations += result.evidence->diagnostics.residual_evaluations;
                ++analysis->performance.evaluated;
                if (reuseEnabled) formalCache.emplace(indices[slot],
                    ReusableFormal{(*analysis->universe)[indices[slot]].id, result});
            }
            const auto &e = *result.evidence;
            const double tolerance = baselineRoundoff(analysis->baseline);
            if (e.parameters.scales.back() == 0 && e.q + tolerance < analysis->baseline.q &&
                (!better || e.q < better->q))
                better = &e;
            checkpoint(context);
        }
        if (better) {
            formalCache.clear(); cacheBinding.reset();
            if (!analysis->candidate_recall_reports.empty()) analysis->candidate_recall_reports.back().discarded = true;
            if (!analysis->normalization_over_full_pool ||
                (reuseEnabled && fullReason == "formal_underestimation")) {
                analysis->baseline = entryBaseline;
                fullReason = "baseline_rebuild_full_replay";
                rebuild = 0;
                continue;
            }
            // A protected/tied selection can already equal the full pool.
            // Keep subsequent rebuilds full as well, so later screening cannot
            // discard that replay and reset the original two-rebuild budget.
            if (analysis->problem->candidate_recall.mode == PfsmCandidateRecallMode::enabled && fullReason.empty())
                fullReason = "baseline_rebuild_full_replay";
            if (rebuild >= 2)
                fail("pfsm.baseline", "Baseline remained unstable after two rebuilds",
                     ErrorCode::numerical_failure, "unstable_pfsm_baseline");
            auto parameters = better->parameters;
            parameters.scales.pop_back();
            parameters.shifts.pop_back();
            auto input = fitInput(*analysis, observations);
            input.starts = {parameters.shifts};
            input.fallback = parameters;
            for (std::size_t j = 0; j < input.phases.size(); ++j)
                input.phases[j].initial_shift = parameters.shifts[j];
            refineBaseline(*analysis, std::move(input), context);
            ++analysis->performance.baseline_rebuilds;
            ++rebuild;
            continue;
        }
        if (analysis->problem->candidate_recall.version == PfsmCandidateRecallVersion::v2 && !analysis->candidate_recall_reports.empty()) {
            auto &report = analysis->candidate_recall_reports.back();
            if (report.backend == PfsmCandidateRecallBackend::v2 && report.selection && !report.full_fallback) {
                bool underestimated = false;
                // The gate sees only the simulated formal set, even in shadow mode.
                for (auto &local : report.refinement_candidates) {
                    const auto index = local.recall.original_index;
                    if (!std::binary_search(report.retained_indices.begin(), report.retained_indices.end(), index)) continue;
                    const auto found = std::lower_bound(indices.begin(), indices.end(), index);
                    if (found == indices.end() || *found != index) fail("pfsm.fast.v2", "Missing simulated formal evidence");
                    const auto &formal = *results[std::size_t(found-indices.begin())].evidence;
                    local.formal_gain = formal.delta_q / analysis->baseline.denominator_net;
                    underestimated |= pfsmCandidateRecallUnderestimated(local, report.selection->g_star, *local.formal_gain);
                }
                if (underestimated) {
                    report.full_fallback = report.selection->full_fallback = true;
                    report.fallback_reason = report.selection->fallback_reason = "formal_underestimation";
                    report.retained_indices = report.selection->retained = allIndices;
                    for (auto &e : report.candidates) {
                        e.retained = true;
                        if (e.retention_reason.empty()) e.retention_reason = "full_fallback";
                    }
                    for (auto &e : report.refinement_candidates) e.retention_reasons.push_back("full_fallback");
                    if (indices != allIndices) {
                        fullReason = "formal_underestimation"; supplementPending = true;
                        continue; // Matching cache binding reuses only this baseline's completed cards.
                    }
                }
            }
        }
        const auto sortStart = Clock::now();
        std::vector<ProductCard> ranked;
        ranked.reserve(results.size());
        analysis->normalization_maximum = 0;
        for (std::size_t slot = 0; slot < results.size(); ++slot) {
            analysis->normalization_maximum =
                std::max(analysis->normalization_maximum, results[slot].evidence->delta_rwp);
            ranked.push_back(
                scoredCard((*analysis->universe)[indices[slot]], results[slot].evidence, analysis->origin));
        }
        out.next.candidates = take(rankCandidates(
            std::move(ranked),
            {CandidateRankingRequest::Pass::rescore, 0, CandidateRankingRequest::Order::descending_all},
            context));
        if (!analysis->candidate_recall_reports.empty() && analysis->normalization_over_full_pool)
            recordCandidateRecallTop5(analysis->candidate_recall_reports.back(), out.next.candidates);
        analysis->performance.sorting_seconds = elapsed(sortStart);
        break;
    }
    for (std::size_t j = 0; j < out.next.phases.size(); ++j) {
        auto &card = out.next.phases[j];
        card.scale = analysis->baseline.parameters.scales[j];
        card.algorithmSource = std::make_shared<const CardAlgorithmSource>(
            CardAlgorithmSource{analysis->origin, analysis->origin});
    }
    out.evaluated = analysis->performance.evaluated;
    analysis->template_store = templates;
    out.next.pfsm = std::move(analysis);
    if (out.next.projectSettings) out.next.workflow.completed_target =
        input.operation == SearchOperation::accept_phase || input.operation == SearchOperation::remove_phase ?
        WorkflowStep::phases : WorkflowStep::search;
    checkpoint(context);
    return Result<PreparedSearchResult>{std::move(out)};
} catch (const pfsm_detail::Failure &failure) {
    return Result<PreparedSearchResult>{failure.error};
}
} // namespace qualpf::application
