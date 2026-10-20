#pragma once
#include "qualpf/application/product_values.h"
#include "qualpf/domain/search.h"
#include "qualpf/migration/backends.h"
#include <array>
#include <functional>
#include <optional>

namespace qualpf::application {
// Owned criteria, never a SQL statement or a connection. Infrastructure compiles
// these values using the existing SQL/filter/order contract.
struct CandidateQuery {
    enum class Route { constrained_browse, matching, stored_strongest };
    enum class ElementOperator { and_or, only, any };
    Route route = Route::matching;
    ElementOperator elementOperator = ElementOperator::and_or;
    std::string names, elements;
    int minimumSpecies = 0, maximumSpecies = 0; // 0: unconstrained; distinct chemical elements.
    std::vector<std::string> subfiles, crystalSystems, spaceGroups, colors, ids;
    std::array<double, 6> cellMinimum{-1, -1, -1, -1, -1, -1}, cellMaximum{-1, -1, -1, -1, -1, -1};
    double calculatedDensityMinimum = -1, calculatedDensityMaximum = -1;
    double measuredDensityMinimum = -1, measuredDensityMaximum = -1;
    std::vector<double> d, deltaD;
    double wavelength = 0;
    bool excludeDeleted = false, decodeReflections = true, print = false;
    bool strictPairedReflections = false;
    // Owned IDs after Application's in-memory filter, before SQL restraints.
    // A present empty list means no candidates; absence requests normal routing.
    std::optional<std::vector<std::string>> preselectedIds;
};
// Common constraints and read requirements; no Classical peak-table dependency.
Result<CandidateQuery> planCandidateQuery(const CandidateQuery &, bool scored, const ExecutionContext &);
using SearchProgress = std::function<void(std::size_t current, std::size_t total)>;
struct StoredCandidateIndex {
    std::string id;
    std::vector<DSpacingAngstrom> strongest;
};
} // namespace qualpf::application
