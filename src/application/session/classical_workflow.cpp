#include "classical_workflow.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/initial_peak_search.h"
#include <algorithm>
#include <cmath>
#include "qualpf/domain/validation.h"
namespace qualpf::application {
namespace {
Error cancelled() { return {ErrorCode::cancelled, "search", "Search canceled"}; }
}
Status validateClassicalSpectrum(const Pattern &pattern) {
    if (pattern.observed.empty() || pattern.two_theta.empty())
        return Status{Error{ErrorCode::missing_data, "pattern", "Classical SM requires a real continuous spectrum", "classical_spectrum_required"}};
    if (!pattern.calculation_range)
        return Status{Error{ErrorCode::missing_data, "calculation_range", "Classical SM requires a calculation range", "classical_range_required"}};
    const auto issues = validate(pattern);
    if (!issues.empty()) return Status{issues.front()};
    return Status{std::monostate{}};
}
Result<CandidateQuery> prepareClassicalQuery(CandidateQuery query, const ProductPeaks &peaks,
    const SearchParameters &parameters, bool scored) {
    if (query.route == CandidateQuery::Route::stored_strongest && scored) {
        auto windows = productPeaksWithTolerance(peaks, parameters.delta_two_theta.value);
        if (!windows) return Result<CandidateQuery>{windows.error()};
        query.d = std::move(windows.value().d);
        query.deltaD = std::move(windows.value().deltaD);
    }
    return Result<CandidateQuery>{std::move(query)};
}
Result<ScoreValues> ClassicalWorkflow::score(const ProductState &state, const ProductCard &card,
                                        const SearchParameters &parameters,
                                        const ExecutionContext &context) const {
    if (context.cancellation.stop_requested())
        return Result<ScoreValues>{cancelled()};
    if (state.editing)
        return Result<ScoreValues>{Error{ErrorCode::invalid_input, "session.edit",
                                         "Finish the preprocessing edit before searching"}};
    auto spectrum = validateClassicalSpectrum(state.pattern);
    if (!spectrum) return Result<ScoreValues>{spectrum.error()};
    auto peaks = productSearchPeaks(state.peaks);
    if (!peaks)
        return Result<ScoreValues>{peaks.error()};
    if (card.tth.size() != card.intensity.size())
        return Result<ScoreValues>{Error{ErrorCode::invalid_input, "card", "Card columns disagree"}};
    const auto reference = productSearchCard(card);
    const auto algorithms = algorithms_;
    auto prepared = state.pattern.background ? AnalysisSpectrum::create(state.pattern) :
        prepareAnalysisSpectrum(std::make_shared<const ProductState>(state), state.pattern, context);
    if (!prepared) return Result<ScoreValues>{prepared.error()};
    auto spectrumInput = AnalysisSpectrum::create(prepared.value().pattern(), AnalysisPurpose::classical_search);
    if (!spectrumInput) return Result<ScoreValues>{spectrumInput.error()};
    auto score = search::score_classic({spectrumInput.value(), peaks.value(), reference, parameters},
        *algorithms->modules.matcher, context);
    if (!score)
        return Result<ScoreValues>{score.error()};
    const auto &s = score.value().score;
    for (double value : {s.total, s.position, s.intensity, s.scale})
        if (!std::isfinite(value)) return Result<ScoreValues>{Error{ErrorCode::invalid_input,
            "algorithm.output.score", "FOM returned a nonfinite score", "invalid_algorithm_output"}};
    return Result<ScoreValues>{score.value().score};
}
Result<std::vector<ProductCard>> ClassicalWorkflow::rescore(const ProductState &state,
                                                       const SearchParameters &parameters,
                                                       const ExecutionContext &context,
                                                       SearchProgress progress) const {
    if (context.cancellation.stop_requested())
        return Result<std::vector<ProductCard>>{cancelled()};
    auto spectrum = validateClassicalSpectrum(state.pattern);
    if (!spectrum) return Result<std::vector<ProductCard>>{spectrum.error()};
    std::vector<ProductCard> cards;
    if (state.peaks.tth.empty())
        return Result<std::vector<ProductCard>>{cards};
    const auto algorithms = algorithms_;

    std::size_t evaluated = 0;
    for (auto card : state.candidates) {
        if (context.cancellation.stop_requested())
            return Result<std::vector<ProductCard>>{cancelled()};
        if (progress)
            progress(++evaluated, state.candidates.size());
        if (card.tth.empty())
            continue;
        auto score = this->score(state, card, parameters, context);
        if (!score)
            return Result<std::vector<ProductCard>>{score.error()};
        const auto &s = score.value();
        if (!(s.total > 0 && s.total >= parameters.minimum_score))
            continue;
        card.fom = s.total;
        card.fomCalculated = true;
        card.fomPeakPos = s.position;
        card.fomIntensity = s.intensity;
        // Frozen contract: rescore retains previous/display scale.
        const AlgorithmRunSource origin{algorithms, parameters, {}, state.revision};
        card.algorithmSource = std::make_shared<const CardAlgorithmSource>(CardAlgorithmSource{
            origin, card.algorithmSource ? card.algorithmSource->scale : origin});
        cards.push_back(card);
    }
    auto selected = rankCandidates(std::move(cards), {CandidateRankingRequest::Pass::rescore, parameters.maximum_entries}, context);
    if (!selected) return selected;
    cards = std::move(selected).value();
    if (context.cancellation.stop_requested())
        return Result<std::vector<ProductCard>>{cancelled()};
    return Result<std::vector<ProductCard>>{std::move(cards)};
}
Result<ProductPeaks> ClassicalWorkflow::residual(const ProductState &state, const ProductCard &card,
                                            const SearchParameters &parameters,
                                            const ExecutionContext &context) const {
    if (context.cancellation.stop_requested())
        return Result<ProductPeaks>{cancelled()};
    if (state.editing || card.tth.size() != card.intensity.size())
        return Result<ProductPeaks>{
            Error{ErrorCode::invalid_input, "residual", "Unfinished edit or inconsistent card"}};
    auto peaks = productSearchPeaks(state.peaks);
    if (!peaks)
        return Result<ProductPeaks>{peaks.error()};
    auto residual = search::update_classic_residual(peaks.value(), productSearchCard(card), card.scale,
        parameters, context);
    if (!residual)
        return Result<ProductPeaks>{residual.error()};
    if (residual.value().peaks.dataset != peaks.value().dataset)
        return Result<ProductPeaks>{Error{ErrorCode::invalid_input, "algorithm.output.dataset",
            "Residual peaks must belong to the input peak set", "invalid_algorithm_output"}};
    auto measured = productMeasuredPeaks(residual.value().peaks, state.peaks.integratedIntensity.has_value(),
        WavelengthAngstrom{state.peaks.wave});
    if (!measured) return measured;
    auto result = std::move(measured).value();
    // The existing product residual retains absence of an area column.
    if (!state.peaks.integratedIntensity) result.integratedIntensity.reset();
    if (context.cancellation.stop_requested())
        return Result<ProductPeaks>{cancelled()};
    return Result<ProductPeaks>{std::move(result)};
}
Result<std::vector<ProductCard>> ClassicalWorkflow::evaluateInitial(const ProductState &state,
    std::vector<ProductCard> candidates, const SearchParameters &parameters, bool excludeDeleted,
    double minimum, const ExecutionContext &context, SearchProgress progress,
    std::size_t &evaluated) const {
    std::vector<ProductCard> cards;
    const auto total = candidates.size();
    for (auto card : std::move(candidates)) {
        if (context.cancellation.stop_requested())
            return Result<std::vector<ProductCard>>{cancelled()};
        ++evaluated;
        if (progress)
            progress(evaluated, total);
        if (context.cancellation.stop_requested())
            return Result<std::vector<ProductCard>>{cancelled()};
        {
            auto accepted = search::accepts_record_quality(card.quality, !excludeDeleted, context);
            if (!accepted) return Result<std::vector<ProductCard>>{accepted.error()};
            if (!accepted.value()) continue;
            if (card.tth.empty())
                continue;
            auto outcome = score(state, card, parameters, context);
            if (!outcome)
                return Result<std::vector<ProductCard>>{outcome.error()};
            const auto &s = outcome.value();
            if (!(s.total > minimum))
                continue;
            card.fom = s.total;
            card.fomCalculated = true;
            card.fomPeakPos = s.position;
            card.fomIntensity = s.intensity;
            card.scale = s.scale;
        }
        cards.push_back(std::move(card));
    }
    return Result<std::vector<ProductCard>>{std::move(cards)};
}
} // namespace qualpf::application
