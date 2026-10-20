#pragma once
#include "qualpf/application/search_engine.h"
namespace qualpf::application {
struct CandidateRankingRequest {
    enum class Pass { initial, scored_browse, metadata_browse, rescore, cli_match };
    Pass pass = Pass::initial;
    std::size_t maximum = 0;
    enum class Order { classical, descending_all };
    Order order = Order::classical;
};
// Receives method-computed scores; contains no method evidence or thresholds.
Result<std::vector<ProductCard>> rankCandidates(std::vector<ProductCard>, const CandidateRankingRequest &,
    const ExecutionContext & = {});
// An absent intent denotes an already accepted or unscored candidate.
Result<std::optional<SearchInput>> phaseAcceptanceInput(ProductHandle, const std::string &candidateId,
    const SearchParameters &, bool subtractResidual);
Result<SearchInput> phaseRemovalInput(ProductHandle, const std::string &phaseId);
} // namespace qualpf::application
