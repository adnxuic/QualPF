#pragma once

#include "qualpf/application/product_session.h"
#include "qualpf/application/candidate_query.h"
#include <array>
#include <functional>
#include <limits>
#include <string_view>

namespace qualpf::application {

class CandidateReader {
  public:
    virtual ~CandidateReader() = default;
    virtual std::optional<CandidateReadStatistics> readStatistics() const { return {}; }
    virtual Result<std::vector<StoredCandidateIndex>> storedTop(const ExecutionContext &, SearchProgress) {
        return Result<std::vector<StoredCandidateIndex>>{Error{ErrorCode::unsupported, "search.stored_top",
            "Stored strongest index access is unavailable"}};
    }
    virtual Result<ProductCard> findById(std::string_view, double, const ExecutionContext &) {
        return Result<ProductCard>{
            Error{ErrorCode::unsupported, "search.card", "Card lookup is unavailable"}};
    }
    virtual Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                                  SearchProgress) = 0;
};
enum class SearchOperation {
    initial,
    scored_browse,
    metadata_browse,
    rescore,
    residual,
    accept_phase,
    remove_phase,
    load_card,
    cli_match
};
struct PfsmRequest;
struct SearchInput {
    ProductHandle source;
    CandidateQuery query;
    SearchParameters parameters;
    SearchOperation operation = SearchOperation::initial;
    std::optional<ProductCard> phase;
    std::string cardId;
    bool subtractResidual = true;
    bool merge = false;
    std::uint64_t taskId = 0, databaseVersion = 0;
    // SQL browse uses -1 to retain defined zero scores, with no cap/sort.
    std::optional<double> threshold;
    SearchMethod method = SearchMethod::classic_sm;
    std::shared_ptr<const PfsmRequest> pfsm;
    std::string databaseIdentity;
    // Runtime-only stage notification; never part of the scientific result.
    std::function<void(std::string)> workflowProgress;
};
struct ConfiguredFwhmEvaluation;
struct PfsmPlot;
struct PreparedSearchResult {
    ProductHandle source;
    std::uint64_t taskId = 0, databaseVersion = 0;
    ProductState next;
    bool changed = true;
    std::size_t evaluated = 0;
    // Owned nonpublishing diagnostic delivery, including a normal no-eligible result.
    std::shared_ptr<const ConfiguredFwhmEvaluation> fwhm;
    std::shared_ptr<const PfsmPlot> pfsmPlot;
};

// Synchronous and thread-agnostic: immutable source, private draft and no Session
// access, scheduling or event pumping. The caller owns publication admission.
class SearchEngine {
  public:
    explicit SearchEngine(AlgorithmHandle algorithms = {}) : algorithms_(std::move(algorithms)) {}
    Result<PreparedSearchResult> run(const SearchInput &, CandidateReader &, const ExecutionContext &,
                                     SearchProgress = {}) const;
    Result<ScoreValues> score(const ProductState &, const ProductCard &, const SearchParameters &,
                              const ExecutionContext &) const;
    Result<std::vector<ProductCard>> rescore(const ProductState &, const SearchParameters &,
                                             const ExecutionContext &, SearchProgress = {}) const;
    Result<ProductPeaks> residual(const ProductState &, const ProductCard &, const SearchParameters &,
                                  const ExecutionContext &) const;
  private:
    Status validateBinding(const ProductState &) const;
    Status validateHistory(const ProductState &) const;
    Status validateScale(const ProductState &, const ProductCard &) const;
    AlgorithmHandle algorithmsFor(const ProductState &state) const {
        return algorithms_ ? algorithms_ : state.algorithms;
    }
    AlgorithmHandle algorithms_;
};
ReferenceCard productSearchCard(const ProductCard &);
Result<ProductPeaks> productPeaksWithTolerance(const ProductPeaks &, double);
} // namespace qualpf::application
