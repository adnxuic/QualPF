#include "qualpf/application/search_engine.h"
#include "classical_workflow.h"
#include "qualpf/application/result_management.h"
#include "pfsm_workflow.h"
#include "qualpf/domain/radiation.h"
#include <algorithm>
#include <cmath>
#include <exception>
namespace qualpf::application {
namespace {
Error cancelled() { return {ErrorCode::cancelled, "search", "Search canceled"}; }
Error incompatible(const char *detail) {
    return {ErrorCode::stale_snapshot, "search.provenance", detail, "algorithm_provenance_mismatch"};
}
} // namespace
Status SearchEngine::validateBinding(const ProductState &state) const {
    if (!state.algorithms || (algorithms_ && algorithms_ != state.algorithms))
        return Status{incompatible("SearchEngine algorithms disagree with the source; prepare a new task")};
    return preflightSearchMethod(state.algorithms->spec.method());
}
Status SearchEngine::validateHistory(const ProductState &state) const {
    auto bound = validateBinding(state);
    if (!bound) return bound;
    if (state.provenance.legacyUnverified)
        return Status{Error{ErrorCode::stale_snapshot, "search.provenance",
            "QXP algorithm provenance is unverified; restart from the original measurement before scientific search",
            "legacy_algorithm_provenance_unverified"}};
    for (const auto &origin : state.provenance.subtractedBackgroundHistory)
        if (!origin.algorithms || origin.algorithms->spec.selections().background != state.algorithms->spec.selections().background ||
            !sameBackgroundOptions(origin.algorithms->spec.defaults().background, state.algorithms->spec.defaults().background))
            return Status{incompatible("Working signal contains an incompatible background; restart from the original measurement")};
    for (const auto &origin : state.provenance.residualHistory)
        if (!origin.algorithms || !sameResidualDependencies(origin.algorithms->spec, state.algorithms->spec))
            return Status{incompatible("Historical residual uses incompatible algorithms; restart from the original measurement")};
    return Status{std::monostate{}};
}
Status SearchEngine::validateScale(const ProductState &state, const ProductCard &card) const {
    const auto &origin = card.algorithmSource;
    if (origin && (!origin->scale.algorithms ||
        !sameScoreProducers(origin->scale.algorithms->spec, state.algorithms->spec)))
        return Status{incompatible("Candidate scale uses incompatible Matcher/FOM; run a fresh search")};
    return Status{std::monostate{}};
}
Result<ProductPeaks> productPeaksWithTolerance(const ProductPeaks &peaks, double delta) {
    if (!std::isfinite(delta) || delta <= 0)
        return Result<ProductPeaks>{
            Error{ErrorCode::invalid_input, "search.delta", "Invalid search tolerance"}};
    if (peaks.d.size() != peaks.deltaD.size())
        return Result<ProductPeaks>{
            Error{ErrorCode::invalid_input, "search.delta_d", "Peak columns disagree"}};
    auto result = peaks;
    // Search prefilter metadata: retain the frozen binary32 deltadval evaluation.
    // Positions and current/original intensities are never re-exported here.
    for (std::size_t i = 0; i < result.d.size(); ++i) {
        const float d = float(result.d[i]), wave = float(result.wave);
        const float value = (3.14159265f / 180.0f) * float(delta) * (d * d) *
                            std::sqrt(1.0f - wave * wave / (4.0f * (d * d))) / wave;
        if (!std::isfinite(value))
            return Result<ProductPeaks>{
                Error{ErrorCode::invalid_input, "search.delta_d", "Undefined prefilter tolerance"}};
        result.deltaD[i] = value;
    }
    return Result<ProductPeaks>{std::move(result)};
}

ReferenceCard productSearchCard(const ProductCard &card) {
    // Only scoring fields belong in this projection. Invalid/missing RIR text
    // affects quantitative display, not Legacy FOM.
    ReferenceCard result;
    result.database_id = "active";
    result.card_id = card.id;
    const auto tth = card.tth;
    const auto intensity = card.intensity;
    for (std::size_t i = 0; i < tth.size() && i < intensity.size(); ++i) {
        Reflection r;
        r.id = {std::uint64_t(i + 1)};
        r.two_theta = TwoThetaDegrees{tth[i]};
        r.intensity = intensity[i];
        result.reflections.push_back(r);
    }
    return result;
}
Result<ScoreValues> SearchEngine::score(const ProductState &state, const ProductCard &card,
                                        const SearchParameters &parameters,
                                        const ExecutionContext &context) const {
    auto history = validateHistory(state);
    if (!history) return Result<ScoreValues>{history.error()};
    if (state.algorithms->spec.method() == SearchMethod::pfsm)
        return Result<ScoreValues>{Error{ErrorCode::unsupported,"pfsm.operation","Use joint PFSM search for profile scores"}};
    return ClassicalWorkflow{algorithmsFor(state)}.score(state, card, parameters, context);
}
Result<std::vector<ProductCard>> SearchEngine::rescore(const ProductState &state,
                                                       const SearchParameters &parameters,
                                                       const ExecutionContext &context,
                                                       SearchProgress progress) const {
    auto history = validateHistory(state);
    if (!history) return Result<std::vector<ProductCard>>{history.error()};
    if (state.algorithms->spec.method() == SearchMethod::pfsm)
        return Result<std::vector<ProductCard>>{Error{ErrorCode::unsupported,"pfsm.operation","Use joint PFSM search for profile rescoring"}};
    for (const auto &card : state.candidates) {
        auto scale = validateScale(state, card);
        if (!scale) return Result<std::vector<ProductCard>>{scale.error()};
    }
    return ClassicalWorkflow{algorithmsFor(state)}.rescore(state, parameters, context, std::move(progress));
}
Result<ProductPeaks> SearchEngine::residual(const ProductState &state, const ProductCard &card,
                                            const SearchParameters &parameters,
                                            const ExecutionContext &context) const {
    auto history = validateHistory(state);
    if (!history) return Result<ProductPeaks>{history.error()};
    if (state.algorithms->spec.method() == SearchMethod::pfsm)
        return Result<ProductPeaks>{Error{ErrorCode::unsupported,"pfsm.operation","PFSM does not subtract formal experimental peaks"}};
    auto scale = validateScale(state, card);
    if (!scale) return Result<ProductPeaks>{scale.error()};
    return ClassicalWorkflow{algorithmsFor(state)}.residual(state, card, parameters, context);
}
Result<PreparedSearchResult> SearchEngine::run(const SearchInput &input, CandidateReader &reader,
                                               const ExecutionContext &context, SearchProgress progress) const
    try {
    auto available = preflightSearchMethod(input.method);
    if (!available) return Result<PreparedSearchResult>{available.error()};
    if (context.cancellation.stop_requested())
        return Result<PreparedSearchResult>{cancelled()};
    if (!input.source)
        return Result<PreparedSearchResult>{
            Error{ErrorCode::missing_data, "search.source", "Missing search snapshot"}};
    auto bound = validateBinding(*input.source);
    if (!bound) return Result<PreparedSearchResult>{bound.error()};
    if (input.method != input.source->algorithms->spec.method())
        return Result<PreparedSearchResult>{incompatible("Search method disagrees with the source")};
    if (input.operation != SearchOperation::metadata_browse) {
        auto history = validateHistory(*input.source);
        if (!history) return Result<PreparedSearchResult>{history.error()};
    }
    if (input.source->editing && input.operation != SearchOperation::metadata_browse)
        return Result<PreparedSearchResult>{Error{ErrorCode::invalid_input, "session.edit",
                                                  "Finish the preprocessing edit before searching"}};
    const bool hasAnalysis=input.source->pfsm || !input.source->candidates.empty() || !input.source->phases.empty();
    if(input.operation!=SearchOperation::metadata_browse && hasAnalysis &&
       ((!input.source->analysis_database.empty() && input.source->analysis_database!=input.databaseIdentity) ||
        (input.source->restored_project && input.source->analysis_database.empty())))
        return Result<PreparedSearchResult>{Error{ErrorCode::stale_snapshot,"project.database",
            "The saved database identity does not match. Restart the analysis before retrieving new references.","project_database_mismatch"}};
    if (input.method == SearchMethod::pfsm)
        return runPfsm(input, reader, context, std::move(progress));
    const auto algorithms = algorithmsFor(*input.source);
    if (!algorithms) return Result<PreparedSearchResult>{Error{ErrorCode::invalid_input, "algorithms", "Missing algorithm dependencies"}};
    const SearchEngine engine(algorithms);
    const auto stage=[&](const char *name) {if(input.workflowProgress)input.workflowProgress(name);};
    PreparedSearchResult result{input.source, input.taskId, input.databaseVersion, *input.source};
    auto &next = result.next;
    if (input.operation != SearchOperation::metadata_browse) next.analysis_database = input.databaseIdentity;
    const AlgorithmRunSource origin{algorithms, input.parameters, input.source, input.source->revision,
        input.taskId, input.databaseVersion};
    const auto recordScores = [&](std::vector<ProductCard> &cards, bool preserveScale) {
        for (auto &card : cards) {
            if (!card.fomCalculated) continue;
            const auto scale = preserveScale && card.algorithmSource ? card.algorithmSource->scale : origin;
            card.algorithmSource = std::make_shared<const CardAlgorithmSource>(CardAlgorithmSource{origin, scale});
        }
    };
    switch (input.operation) {
    case SearchOperation::remove_phase:
        return Result<PreparedSearchResult>{Error{ErrorCode::unsupported, "search.operation", "Joint removal requires PFSM"}};
    case SearchOperation::residual:
    case SearchOperation::accept_phase: {
        if (!input.phase)
            return Result<PreparedSearchResult>{
                Error{ErrorCode::missing_data, "search.phase", "Missing phase"}};
        auto scale = validateScale(next, *input.phase);
        if (!scale) return Result<PreparedSearchResult>{scale.error()};
        if (input.operation == SearchOperation::accept_phase) {
            if (std::any_of(next.phases.begin(), next.phases.end(),
                            [&](const auto &p) { return p.id == input.phase->id; })) {
                result.changed = false;
                break;
            }
            next.phases.push_back(*input.phase);
            next.candidates.erase(
                std::remove_if(next.candidates.begin(), next.candidates.end(),
                               [&](const auto &card) { return card.id == input.phase->id; }),
                next.candidates.end());
        }
        if (input.subtractResidual && !next.peaks.tth.empty()) {
            stage("Residual");
            auto peaks = engine.residual(next, *input.phase, input.parameters, context);
            if (!peaks)
                return Result<PreparedSearchResult>{peaks.error()};
            next.peaks = std::move(peaks).value();
            next.provenance.residualHistory.push_back(origin);
            stage("Matching / FOM");
            auto cards = engine.rescore(next, input.parameters, context, progress);
            if (!cards)
                return Result<PreparedSearchResult>{cards.error()};
            next.candidates = std::move(cards).value();
            recordScores(next.candidates, true);
        }
        break;
    }
    case SearchOperation::load_card: {
        auto card = reader.findById(
            input.cardId, next.peaks.wave > 0 ? next.peaks.wave : radiation::default_wavelength_angstrom,
            context);
        if (context.cancellation.stop_requested())
            return Result<PreparedSearchResult>{cancelled()};
        if (!card)
            return Result<PreparedSearchResult>{card.error()};
        if (next.peaks.valid && !next.peaks.tth.empty() && !card.value().tth.empty()) {
            auto outcome = engine.score(next, card.value(), input.parameters, context);
            if (!outcome)
                return Result<PreparedSearchResult>{outcome.error()};
            const auto &value = outcome.value();
            card.value().fom = value.total;
            card.value().fomCalculated = true;
            card.value().fomPeakPos = value.position;
            card.value().fomIntensity = value.intensity;
            card.value().scale = value.scale;
            card.value().algorithmSource = std::make_shared<const CardAlgorithmSource>(CardAlgorithmSource{origin, origin});
        }
        if (std::any_of(next.candidates.begin(), next.candidates.end(),
                        [&](const auto &old) { return old.id == card.value().id; }))
            result.changed = false;
        else
            next.candidates.push_back(std::move(card).value());
        break;
    }
    case SearchOperation::rescore: {
        stage("Matching / FOM");
            auto cards = engine.rescore(next, input.parameters, context, progress);
        if (!cards)
            return Result<PreparedSearchResult>{cards.error()};
        next.candidates = std::move(cards).value();
        recordScores(next.candidates, true);
        break;
    }
    case SearchOperation::initial:
    case SearchOperation::cli_match:
    case SearchOperation::scored_browse:
    case SearchOperation::metadata_browse: {
        if (input.operation != SearchOperation::metadata_browse) {
            auto spectrum = validateClassicalSpectrum(next.pattern);
            if (!spectrum) return Result<PreparedSearchResult>{spectrum.error()};
            auto peaks = productSearchPeaks(next.peaks);
            if (!peaks) return Result<PreparedSearchResult>{peaks.error()};
        }
        stage("Candidate Screening");
        auto planned = planCandidateQuery(input.query, input.operation != SearchOperation::metadata_browse, context);
        if (planned) planned = prepareClassicalQuery(std::move(planned).value(), next.peaks, input.parameters,
            input.operation != SearchOperation::metadata_browse);
        if (!planned) return Result<PreparedSearchResult>{planned.error()};
        auto query = std::move(planned).value();
        if (query.route == CandidateQuery::Route::stored_strongest && !query.preselectedIds) {
            auto indexes = reader.storedTop(context, progress);
            if (!indexes) return Result<PreparedSearchResult>{indexes.error()};
            query.preselectedIds.emplace();
            for (const auto &index : indexes.value()) {
                auto accepted = search::passes_stored_strongest(query.d, query.deltaD, index.strongest, context);
                if (!accepted) return Result<PreparedSearchResult>{accepted.error()};
                if (accepted.value()) query.preselectedIds->push_back(index.id);
            }
        }
        auto candidates = reader.read(query, context, progress);
        if (context.cancellation.stop_requested())
            return Result<PreparedSearchResult>{cancelled()};
        if (!candidates)
            return Result<PreparedSearchResult>{candidates.error()};
        std::vector<ProductCard> cards;
        const double minimum = input.threshold.value_or(input.parameters.minimum_score);
        const CandidateRankingRequest selection{
            input.operation == SearchOperation::initial ? CandidateRankingRequest::Pass::initial :
            input.operation == SearchOperation::cli_match ? CandidateRankingRequest::Pass::cli_match :
            input.operation == SearchOperation::metadata_browse ? CandidateRankingRequest::Pass::metadata_browse :
                CandidateRankingRequest::Pass::scored_browse,
            input.parameters.maximum_entries};
        if (input.operation == SearchOperation::metadata_browse) {
            const auto total = candidates.value().size();
            for (auto &card : candidates.value()) {
                if (context.cancellation.stop_requested())
                    return Result<PreparedSearchResult>{cancelled()};
                ++result.evaluated;
                if (progress) progress(result.evaluated, total);
                if (context.cancellation.stop_requested())
                    return Result<PreparedSearchResult>{cancelled()};
                cards.push_back(std::move(card));
            }
        } else {
            stage("Matching / FOM");
            auto evaluated = ClassicalWorkflow{algorithms}.evaluateInitial(next, std::move(candidates).value(),
                input.parameters, query.excludeDeleted, minimum, context, progress, result.evaluated);
            if (!evaluated) return Result<PreparedSearchResult>{evaluated.error()};
            cards = std::move(evaluated).value();
        }
        stage("Results");
        auto selected = rankCandidates(std::move(cards), selection, context);
        if (!selected) return Result<PreparedSearchResult>{selected.error()};
        cards = std::move(selected).value();
        recordScores(cards, false);
        if (input.merge) {
            for (auto &card : cards)
                if (std::none_of(next.candidates.begin(), next.candidates.end(),
                                 [&](const auto &old) { return old.id == card.id; }))
                    next.candidates.push_back(std::move(card));
        } else {
            next.candidates = std::move(cards);
        }
        break;
    }
    default:
        return Result<PreparedSearchResult>{
            Error{ErrorCode::unsupported, "search.operation", "Unknown search operation"}};
    }
    if (context.cancellation.stop_requested())
        return Result<PreparedSearchResult>{cancelled()};
    if (next.projectSettings && result.changed && input.operation != SearchOperation::metadata_browse)
        next.workflow.completed_target = input.operation == SearchOperation::accept_phase || input.operation == SearchOperation::residual ?
            WorkflowStep::phases : WorkflowStep::search;
    return Result<PreparedSearchResult>{std::move(result)};
} catch (const std::exception &error) {
    return Result<PreparedSearchResult>{
        Error{ErrorCode::numerical_failure, "search.execution", error.what(), "unexpected_exception"}};
} catch (...) {
    return Result<PreparedSearchResult>{Error{ErrorCode::numerical_failure, "search.execution",
                                              "Search execution failed", "unexpected_exception"}};
}
} // namespace qualpf::application
