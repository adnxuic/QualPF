#pragma once

#include "numerical_regression.h"
#include "qualpf/search/classic.h"

namespace qualpf::testing {
// Exact identities/order/decisions are separate from quantities with units.
struct ClassicObservation {
    NumericalObservation numerical;
    std::map<std::string, std::vector<std::string>> discrete;
};
inline void observe_score(ClassicObservation &out, const ScoreValues &score, const std::string &prefix) {
    out.numerical.fields[prefix + "total"] = {score.total};
    out.numerical.fields[prefix + "position"] = {score.position};
    out.numerical.fields[prefix + "intensity"] = {score.intensity};
    out.numerical.fields[prefix + "scale"] = {score.scale};
}
inline ClassicObservation observe(const Result<search::ScoreTrace> &result) {
    ClassicObservation out;
    if (!result) {
        out.numerical.error = result.error().code;
        out.discrete["error.field"] = {result.error().field};
        return out;
    }
    const auto &t = result.value();
    observe_score(out, t.score, "");
    out.numerical.fields["merged_intensities"] = t.merged_intensities;
    out.numerical.fields["scaled_intensities"] = t.scaled_intensities;
    out.numerical.fields["position_difference_sum"] = {t.position_difference_sum};
    out.numerical.fields["associated_sample_sum"] = {t.associated_sample_sum};
    out.numerical.fields["associated_card_sum"] = {t.associated_card_sum};
    out.numerical.fields["total_sample_sum"] = {t.total_sample_sum};
    out.numerical.fields["total_card_sum"] = {t.total_card_sum};
    out.numerical.fields["database_fraction"] = {t.database_fraction};
    out.numerical.fields["sample_fraction"] = {t.sample_fraction};
    out.numerical.fields["sqrt_database"] = {t.sqrt_database};
    out.numerical.fields["sqrt_sample"] = {t.sqrt_sample};
    out.discrete["disposition"] = {std::to_string(static_cast<int>(t.disposition))};
    out.discrete["counts"] = {std::to_string(t.associated_reflections), std::to_string(t.associated_peaks),
                              std::to_string(t.unassociated_below_observed),
                              std::to_string(t.expected_peaks_per_phase)};
    out.discrete["reflection_indices"] = {};
    out.discrete["experimental_indices"] = {};
    for (auto i : t.reflection_indices)
        out.discrete["reflection_indices"].push_back(std::to_string(i));
    for (auto i : t.experimental_indices)
        out.discrete["experimental_indices"].push_back(i ? std::to_string(*i) : "none");
    return out;
}
struct ClassicReference {
    NumericalRegressionCase numerical;
    std::map<std::string, std::vector<std::string>> discrete;
};
inline Status compare_classic_reference(const ClassicReference &reference, const ClassicObservation &actual) {
    auto checked = compare_numerical_reference(reference.numerical, actual.numerical);
    if (!checked)
        return checked;
    if (reference.discrete != actual.discrete)
        return Status{Error{ErrorCode::invalid_input, "discrete",
                            "Candidate order, association or other decision differs"}};
    return Status{std::monostate{}};
}
} // namespace qualpf::testing
