#pragma once

#include "qualpf/application/product_session.h"
#include <array>
#include <functional>
#include <limits>
#include <string_view>

namespace qualpf::application {

// Owned criteria, never a SQL statement or a connection. Infrastructure compiles
// these values using the existing SQL/filter/order contract.
struct CandidateQuery {
    enum class Route { constrained_browse, matching, stored_strongest };
    enum class ElementOperator { and_or, only, any };
    Route route = Route::matching;
    ElementOperator elementOperator = ElementOperator::and_or;
    std::string names, elements;
    std::vector<std::string> subfiles, crystalSystems, spaceGroups, colors, ids;
    std::array<double, 6> cellMinimum{-1, -1, -1, -1, -1, -1}, cellMaximum{-1, -1, -1, -1, -1, -1};
    double calculatedDensityMinimum = -1, calculatedDensityMaximum = -1;
    double measuredDensityMinimum = -1, measuredDensityMaximum = -1;
    std::vector<double> d, deltaD;
    double wavelength = 0;
    bool excludeDeleted = false, decodeReflections = true, print = false;
};
using SearchProgress = std::function<void(std::size_t current, std::size_t total)>;
class CandidateReader {
  public:
    virtual ~CandidateReader() = default;
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
    load_card
};
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
};
struct PreparedSearchResult {
    ProductHandle source;
    std::uint64_t taskId = 0, databaseVersion = 0;
    ProductState next;
    bool changed = true;
    std::size_t evaluated = 0;
};

// Synchronous and thread-agnostic: immutable source, private draft and no Session
// access, scheduling or event pumping. The caller owns publication admission.
class SearchEngine {
  public:
    Result<PreparedSearchResult> run(const SearchInput &, CandidateReader &, const ExecutionContext &,
                                     SearchProgress = {}) const;
    Result<ScoreValues> score(const ProductState &, const ProductCard &, const SearchParameters &,
                              const ExecutionContext &) const;
    Result<std::vector<ProductCard>> rescore(const ProductState &, const SearchParameters &,
                                             const ExecutionContext &, SearchProgress = {}) const;
    Result<ProductPeaks> residual(const ProductState &, const ProductCard &, const SearchParameters &,
                                  const ExecutionContext &) const;
};
ReferenceCard productSearchCard(const ProductCard &);
Result<ProductPeaks> productPeaksWithTolerance(const ProductPeaks &, double);
} // namespace qualpf::application
