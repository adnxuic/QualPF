#include "qualpf/application/search_engine.h"
#include "qualpf/domain/radiation.h"
#include <algorithm>
#include <cmath>
#include <exception>
namespace qualpf::application {
namespace {
Error cancelled() { return {ErrorCode::cancelled, "search", "Search canceled"}; }
} // namespace
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
    if (context.cancellation.stop_requested())
        return Result<ScoreValues>{cancelled()};
    if (state.editing)
        return Result<ScoreValues>{Error{ErrorCode::invalid_input, "session.edit",
                                         "Finish the preprocessing edit before searching"}};
    auto peaks = productSearchPeaks(state.peaks);
    if (!peaks)
        return Result<ScoreValues>{peaks.error()};
    if (card.tth.size() != card.intensity.size())
        return Result<ScoreValues>{Error{ErrorCode::invalid_input, "card", "Card columns disagree"}};
    const auto reference = productSearchCard(card);
    auto score = search::score_classic({state.pattern, peaks.value(), reference, parameters}, context);
    if (!score)
        return Result<ScoreValues>{score.error()};
    return Result<ScoreValues>{score.value().score};
}
Result<std::vector<ProductCard>> SearchEngine::rescore(const ProductState &state,
                                                       const SearchParameters &parameters,
                                                       const ExecutionContext &context,
                                                       SearchProgress progress) const {
    if (context.cancellation.stop_requested())
        return Result<std::vector<ProductCard>>{cancelled()};
    std::vector<ProductCard> cards;
    if (state.peaks.tth.empty())
        return Result<std::vector<ProductCard>>{cards};
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
        if (s.total <= 0 || s.total < parameters.minimum_score)
            continue;
        card.fom = s.total;
        card.fomCalculated = true;
        card.fomPeakPos = s.position;
        card.fomIntensity = s.intensity;
        // Frozen contract: rescore retains previous/display scale.
        cards.push_back(card);
    }
    std::stable_sort(cards.begin(), cards.end(), [](const auto &a, const auto &b) { return a.fom > b.fom; });
    if (std::size_t(cards.size()) > parameters.maximum_entries)
        cards.resize(parameters.maximum_entries);
    if (context.cancellation.stop_requested())
        return Result<std::vector<ProductCard>>{cancelled()};
    return Result<std::vector<ProductCard>>{std::move(cards)};
}
Result<ProductPeaks> SearchEngine::residual(const ProductState &state, const ProductCard &card,
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
    auto residual = search::subtract_phase(peaks.value(), productSearchCard(card), card.scale, parameters);
    if (!residual)
        return Result<ProductPeaks>{residual.error()};
    ProductPeaks result;
    result.valid = state.peaks.valid;
    result.wave = state.peaks.wave;
    if (state.peaks.integratedIntensity)
        result.integratedIntensity.emplace();
    for (const auto &peak : residual.value().peaks.peaks) {
        result.tth.push_back(peak.two_theta.value);
        result.intensity.push_back(peak.intensity);
        result.intensityOrig.push_back(*peak.original_intensity);
        result.d.push_back(peak.d_spacing->value);
        result.deltaD.push_back(peak.delta_d->value);
        result.fwhm.push_back(peak.fwhm->value);
        if (result.integratedIntensity)
            result.integratedIntensity->push_back(*peak.integrated_intensity);
    }
    if (context.cancellation.stop_requested())
        return Result<ProductPeaks>{cancelled()};
    return Result<ProductPeaks>{std::move(result)};
}
Result<PreparedSearchResult> SearchEngine::run(const SearchInput &input, CandidateReader &reader,
                                               const ExecutionContext &context, SearchProgress progress) const
    try {
    if (context.cancellation.stop_requested())
        return Result<PreparedSearchResult>{cancelled()};
    if (!input.source)
        return Result<PreparedSearchResult>{
            Error{ErrorCode::missing_data, "search.source", "Missing search snapshot"}};
    if (input.source->editing && input.operation != SearchOperation::metadata_browse)
        return Result<PreparedSearchResult>{Error{ErrorCode::invalid_input, "session.edit",
                                                  "Finish the preprocessing edit before searching"}};
    PreparedSearchResult result{input.source, input.taskId, input.databaseVersion, *input.source};
    auto &next = result.next;
    switch (input.operation) {
    case SearchOperation::residual:
    case SearchOperation::accept_phase: {
        if (!input.phase)
            return Result<PreparedSearchResult>{
                Error{ErrorCode::missing_data, "search.phase", "Missing phase"}};
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
            auto peaks = residual(next, *input.phase, input.parameters, context);
            if (!peaks)
                return Result<PreparedSearchResult>{peaks.error()};
            next.peaks = std::move(peaks).value();
            auto cards = rescore(next, input.parameters, context, progress);
            if (!cards)
                return Result<PreparedSearchResult>{cards.error()};
            next.candidates = std::move(cards).value();
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
            auto outcome = score(next, card.value(), input.parameters, context);
            if (!outcome)
                return Result<PreparedSearchResult>{outcome.error()};
            const auto &value = outcome.value();
            card.value().fom = value.total;
            card.value().fomCalculated = true;
            card.value().fomPeakPos = value.position;
            card.value().fomIntensity = value.intensity;
            card.value().scale = value.scale;
        }
        if (std::any_of(next.candidates.begin(), next.candidates.end(),
                        [&](const auto &old) { return old.id == card.value().id; }))
            result.changed = false;
        else
            next.candidates.push_back(std::move(card).value());
        break;
    }
    case SearchOperation::rescore: {
        auto cards = rescore(next, input.parameters, context, progress);
        if (!cards)
            return Result<PreparedSearchResult>{cards.error()};
        next.candidates = std::move(cards).value();
        break;
    }
    case SearchOperation::initial:
    case SearchOperation::scored_browse:
    case SearchOperation::metadata_browse: {
        auto query = input.query;
        query.decodeReflections = input.operation != SearchOperation::metadata_browse;
        if (query.route == CandidateQuery::Route::stored_strongest && query.decodeReflections) {
            auto windows = productPeaksWithTolerance(next.peaks, input.parameters.delta_two_theta.value);
            if (!windows)
                return Result<PreparedSearchResult>{windows.error()};
            query.d = std::move(windows.value().d);
            query.deltaD = std::move(windows.value().deltaD);
        }
        auto candidates = reader.read(query, context, progress);
        if (context.cancellation.stop_requested())
            return Result<PreparedSearchResult>{cancelled()};
        if (!candidates)
            return Result<PreparedSearchResult>{candidates.error()};
        std::vector<ProductCard> cards;
        const double minimum = input.threshold.value_or(input.parameters.minimum_score);
        const auto total = candidates.value().size();
        for (auto card : std::move(candidates).value()) {
            if (context.cancellation.stop_requested())
                return Result<PreparedSearchResult>{cancelled()};
            ++result.evaluated;
            if (progress)
                progress(result.evaluated, total);
            if (context.cancellation.stop_requested())
                return Result<PreparedSearchResult>{cancelled()};
            if (input.operation != SearchOperation::metadata_browse) {
                if (card.tth.empty())
                    continue;
                auto outcome = score(next, card, input.parameters, context);
                if (!outcome)
                    return Result<PreparedSearchResult>{outcome.error()};
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
        if (input.operation == SearchOperation::initial && cards.size() > input.parameters.maximum_entries) {
            std::stable_sort(cards.begin(), cards.end(),
                             [](const auto &a, const auto &b) { return a.fom > b.fom; });
            cards.resize(input.parameters.maximum_entries);
        }
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
    return Result<PreparedSearchResult>{std::move(result)};
} catch (const std::exception &error) {
    return Result<PreparedSearchResult>{
        Error{ErrorCode::numerical_failure, "search.execution", error.what(), "unexpected_exception"}};
} catch (...) {
    return Result<PreparedSearchResult>{Error{ErrorCode::numerical_failure, "search.execution",
                                              "Search execution failed", "unexpected_exception"}};
}
} // namespace qualpf::application
