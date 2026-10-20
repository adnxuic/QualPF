#include "qualpf/application/result_management.h"
#include <algorithm>
namespace qualpf::application {
Result<std::vector<ProductCard>> rankCandidates(std::vector<ProductCard> cards,
    const CandidateRankingRequest &request, const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<std::vector<ProductCard>>{Error{ErrorCode::cancelled, "search", "Search canceled"}};
    const bool rescore = request.pass == CandidateRankingRequest::Pass::rescore;
    const bool initial = request.pass == CandidateRankingRequest::Pass::initial ||
        request.pass == CandidateRankingRequest::Pass::cli_match;
    if (request.order == CandidateRankingRequest::Order::descending_all || rescore || (initial && cards.size() > request.maximum))
        std::stable_sort(cards.begin(), cards.end(), [](const auto &a, const auto &b) { return a.fom > b.fom; });
    if (request.order != CandidateRankingRequest::Order::descending_all && (initial || rescore) && cards.size() > request.maximum)
        cards.resize(request.maximum);
    return Result<std::vector<ProductCard>>{std::move(cards)};
}
Result<std::optional<SearchInput>> phaseAcceptanceInput(ProductHandle source, const std::string &id,
    const SearchParameters &parameters, bool subtractResidual) {
    if (!source || !source->algorithms)
        return Result<std::optional<SearchInput>>{Error{ErrorCode::invalid_input, "session", "Missing source"}};
    auto method = preflightSearchMethod(source->algorithms->spec.method());
    if (!method) return Result<std::optional<SearchInput>>{method.error()};
    if (source->editing)
        return Result<std::optional<SearchInput>>{Error{ErrorCode::invalid_input, "session.edit", "Finish the preprocessing edit before searching"}};
    const auto found = std::find_if(source->candidates.begin(), source->candidates.end(),
        [&](const auto &card) { return card.id == id; });
    if (found == source->candidates.end())
        return Result<std::optional<SearchInput>>{Error{ErrorCode::invalid_input, "candidate.id", "Candidate is no longer present", "card_not_found"}};
    if (!found->fomCalculated || std::any_of(source->phases.begin(), source->phases.end(),
        [&](const auto &card) { return card.id == id; }))
        return Result<std::optional<SearchInput>>{std::optional<SearchInput>{}};
    SearchInput input;
    input.source = source;
    input.method = source->algorithms->spec.method();
    input.operation = SearchOperation::accept_phase;
    input.phase = *found;
    input.parameters = parameters;
    input.subtractResidual = subtractResidual;
    return Result<std::optional<SearchInput>>{std::move(input)};
}
Result<SearchInput> phaseRemovalInput(ProductHandle source, const std::string &id) {
    if (!source || !source->pfsm || source->editing)
        return Result<SearchInput>{Error{ErrorCode::invalid_input, "pfsm.phase", "No closed PFSM analysis"}};
    if (std::none_of(source->phases.begin(), source->phases.end(), [&](const auto &p) { return p.id == id; }))
        return Result<SearchInput>{Error{ErrorCode::missing_data, "pfsm.phase", "Selected phase is no longer present"}};
    SearchInput input;
    input.source = std::move(source);
    input.method = SearchMethod::pfsm;
    input.operation = SearchOperation::remove_phase;
    input.cardId = id;
    return Result<SearchInput>{std::move(input)};
}
} // namespace qualpf::application
