#pragma once

#include "qualpf/domain/search.h"

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace qualpf::testing {

enum class EvidenceState { awaiting_reference, captured, reviewed };
struct ArtifactIdentity {
    std::string relative_path;
    std::string sha256;
    std::string source_and_usage_terms;
};
struct ReferenceEnvironment {
    std::string source_commit;
    std::string compiler;
    std::string flags;
    std::string platform;
    std::string real_kind;
    std::string thread_configuration;
    std::vector<ArtifactIdentity> inputs_and_tables;
};
struct FieldTolerance {
    FieldTolerance(double atol, double rtol, std::string units, std::string evidence)
        : absolute(atol), relative(rtol), unit(std::move(units)), approval_evidence(std::move(evidence)) {}
    double absolute;
    double relative;
    std::string unit;
    std::string approval_evidence; // No blanket default tolerance or auto-relaxation.
};
struct ScoreRegressionCase {
    std::string id;
    EvidenceState evidence = EvidenceState::awaiting_reference;
    ReferenceEnvironment environment;
    Pattern pattern;
    PeakSet peaks;
    ReferenceCard candidate;
    SearchParameters parameters;
    std::optional<Result<ScoreValues>> expected; // Pending data has no invented answer.
    std::map<std::string, FieldTolerance> tolerances;
    std::vector<std::string> expected_candidate_order;

    [[nodiscard]] ScoreInput input() const & { return {pattern, peaks, candidate, parameters}; }
    ScoreInput input() const && = delete;
};
class IRegressionCaseStore {
public:
    virtual ~IRegressionCaseStore() = default;
    virtual Result<ScoreRegressionCase> load_score_case(const std::string& id) = 0;
};

// No serializer, comparator or Fortran oracle is implemented in this phase.
// Future comparators must check structural IDs/order separately from numeric tolerances.

} // namespace qualpf::testing
