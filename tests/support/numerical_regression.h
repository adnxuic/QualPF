#pragma once

#include "regression_case.h"

#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace qualpf::testing {

// Named fields permit different units and tolerances for coefficients, angles,
// variances and residuals. Factors are compared by reconstruction, never by
// arbitrary QR/SVD vector signs. Indices and rank remain exact/discrete.
struct NumericalObservation {
    std::map<std::string, std::vector<double>> fields;
    std::vector<std::size_t> indices;
    std::optional<std::size_t> rank;
    std::optional<ErrorCode> error;
};
struct NumericalRegressionCase {
    std::string id;
    EvidenceState evidence = EvidenceState::awaiting_reference;
    ReferenceEnvironment environment;
    std::optional<NumericalObservation> expected;
    std::map<std::string, FieldTolerance> tolerances;
};

inline Status compare_numerical_reference(const NumericalRegressionCase &reference,
                                          const NumericalObservation &actual) {
    if (reference.evidence != EvidenceState::reviewed || !reference.expected)
        return Status{Error{ErrorCode::missing_data, "reference", "A reviewed numerical reference is required"}};
    const auto &environment = reference.environment;
    if (environment.source_commit.empty() || environment.compiler.empty() || environment.flags.empty() ||
        environment.platform.empty() || environment.real_kind.empty() || environment.thread_configuration.empty() ||
        environment.inputs_and_tables.empty())
        return Status{
            Error{ErrorCode::missing_data, "environment", "Reference environment and hashed input are required"}};
    for (const auto &input : environment.inputs_and_tables)
        if (input.relative_path.empty() || input.sha256.size() != 64 || input.source_and_usage_terms.empty())
            return Status{Error{ErrorCode::missing_data, "input", "Reference input identity is incomplete"}};
    const auto &expected = *reference.expected;
    if (actual.indices != expected.indices || actual.rank != expected.rank || actual.error != expected.error ||
        actual.fields.size() != expected.fields.size())
        return Status{Error{ErrorCode::invalid_input, "structure", "Numerical observation structure differs"}};
    if (expected.error && (!expected.fields.empty() || !expected.indices.empty() || expected.rank))
        return Status{
            Error{ErrorCode::invalid_input, "reference", "Error observations must not contain successful outputs"}};
    for (const auto &[field, values] : expected.fields) {
        const auto found = actual.fields.find(field);
        if (found == actual.fields.end() || found->second.size() != values.size())
            return Status{Error{ErrorCode::invalid_input, field, "Field is absent or has a different length"}};
        const auto tolerance = reference.tolerances.find(field);
        if (tolerance == reference.tolerances.end())
            return Status{Error{ErrorCode::missing_data, field, "No field tolerance is recorded"}};
        const auto &t = tolerance->second;
        if (!std::isfinite(t.absolute) || !std::isfinite(t.relative) || t.absolute < 0 || t.relative < 0 ||
            t.unit.empty() || t.approval_evidence.empty())
            return Status{Error{ErrorCode::invalid_input, field, "Invalid or undocumented tolerance"}};
        for (std::size_t i = 0; i < values.size(); ++i) {
            const double bound = t.absolute + t.relative * std::abs(values[i]);
            if (!std::isfinite(values[i]) || !std::isfinite(found->second[i]) || !std::isfinite(bound) ||
                std::abs(found->second[i] - values[i]) > bound)
                return Status{
                    Error{ErrorCode::invalid_input, field, "Numerical difference exceeds the frozen tolerance"}};
        }
    }
    return Status{std::monostate{}};
}

} // namespace qualpf::testing
