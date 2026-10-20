#pragma once
#include "qualpf/application/search_engine.h"
namespace qualpf::application {
Status validateClassicalSpectrum(const Pattern &);
Result<CandidateQuery> prepareClassicalQuery(CandidateQuery, const ProductPeaks &, const SearchParameters &, bool scored);
// Concrete method subworkflow. No scheduling, database access or publication.
class ClassicalWorkflow {
  public:
    explicit ClassicalWorkflow(AlgorithmHandle algorithms) : algorithms_(std::move(algorithms)) {}
    Result<ScoreValues> score(const ProductState &, const ProductCard &, const SearchParameters &, const ExecutionContext &) const;
    Result<std::vector<ProductCard>> rescore(const ProductState &, const SearchParameters &, const ExecutionContext &, SearchProgress) const;
    Result<ProductPeaks> residual(const ProductState &, const ProductCard &, const SearchParameters &, const ExecutionContext &) const;
    Result<std::vector<ProductCard>> evaluateInitial(const ProductState &, std::vector<ProductCard>,
        const SearchParameters &, bool excludeDeleted, double minimum, const ExecutionContext &,
        SearchProgress, std::size_t &evaluated) const;
  private:
    AlgorithmHandle algorithms_;
};
} // namespace qualpf::application
