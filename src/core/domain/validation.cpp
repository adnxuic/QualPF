#include "qualpf/domain/validation.h"

#include <cmath>
#include <set>
#include <utility>

namespace qualpf {
namespace {
void check(ValidationIssues& issues, bool condition, std::string field, std::string message)
{
    if (!condition) issues.push_back({ErrorCode::invalid_input, std::move(field), std::move(message)});
}
void finite(ValidationIssues& issues, double value, const std::string& field)
{
    check(issues, std::isfinite(value), field, "Expected a finite value");
}
void positive(ValidationIssues& issues, double value, const std::string& field)
{
    check(issues, std::isfinite(value) && value > 0.0, field, "Expected a positive finite value");
}
void append(ValidationIssues& to, ValidationIssues from)
{
    to.insert(to.end(), from.begin(), from.end());
}
void range(ValidationIssues& issues, const AngularRange& value, const std::string& field)
{
    finite(issues, value.minimum.value, field + ".minimum");
    finite(issues, value.maximum.value, field + ".maximum");
    check(issues, value.minimum.value <= value.maximum.value, field, "Range is reversed");
}
void signal(ValidationIssues& issues, const std::optional<SampledSignal>& value,
            std::size_t points, const std::string& field)
{
    if (!value) return;
    for (double v : value->values) finite(issues, v, field);
    if (value->window) {
        const auto first = value->window->first;
        check(issues, first <= points && value->values.size() <= points - first,
              field, "Sample window exceeds the pattern grid");
    }
}
} // namespace

ValidationIssues validate(const Pattern& value)
{
    ValidationIssues issues;
    check(issues, value.id.value != 0, "pattern.id", "Dataset ID must be nonzero");
    const auto count = value.two_theta.size();
    check(issues, count == value.observed.size(), "pattern.observed", "Coordinate/intensity size mismatch");
    for (std::size_t i = 0; i < count; ++i) {
        finite(issues, value.two_theta[i].value, "pattern.two_theta");
        if (i) check(issues, value.two_theta[i-1].value < value.two_theta[i].value,
                     "pattern.two_theta", "Grid must be strictly increasing; import never sorts");
    }
    for (double v : value.observed) finite(issues, v, "pattern.observed");
    if (value.corrected_two_theta) {
        check(issues, value.corrected_two_theta->size() == count, "pattern.corrected_two_theta", "Size mismatch");
        for (auto v : *value.corrected_two_theta) finite(issues, v.value, "pattern.corrected_two_theta");
    }
    for (const auto* values : {&value.original_observed, &value.weights}) {
        if (*values) {
            check(issues, (*values)->size() == count, "pattern.optional_array", "Size mismatch");
            for (double v : **values) finite(issues, v, "pattern.optional_array");
        }
    }
    signal(issues, value.background, count, "pattern.background");
    signal(issues, value.calculated, count, "pattern.calculated");
    signal(issues, value.background_subtracted_signal, count, "pattern.background_subtracted_signal");
    for (const auto& line : value.radiation.lines) {
        positive(issues, line.wavelength.value, "radiation.wavelength");
        check(issues, std::isfinite(line.relative_weight) && line.relative_weight >= 0.0,
              "radiation.relative_weight", "Expected a nonnegative finite weight");
    }
    if (value.calculation_range) range(issues, *value.calculation_range, "pattern.calculation_range");
    return issues;
}

ValidationIssues validate(const PeakSet& value)
{
    ValidationIssues issues;
    check(issues, value.dataset.value != 0, "peaks.dataset", "Dataset ID must be nonzero");
    std::set<std::uint64_t> ids;
    for (const auto& peak : value.peaks) {
        check(issues, peak.id.value != 0 && ids.insert(peak.id.value).second, "peak.id", "Invalid or duplicate peak ID");
        finite(issues, peak.two_theta.value, "peak.two_theta");
        finite(issues, peak.intensity, "peak.intensity");
        if (peak.d_spacing) positive(issues, peak.d_spacing->value, "peak.d_spacing");
        if (peak.delta_d) check(issues, std::isfinite(peak.delta_d->value) && peak.delta_d->value >= 0.0,
                                "peak.delta_d", "Expected a nonnegative finite tolerance");
        if (peak.original_intensity) finite(issues, *peak.original_intensity, "peak.original_intensity");
        if (peak.integrated_intensity) finite(issues, *peak.integrated_intensity, "peak.integrated_intensity");
        if (peak.fwhm) check(issues, std::isfinite(peak.fwhm->value) && peak.fwhm->value >= 0.0,
                             "peak.fwhm", "Expected a nonnegative finite width");
    }
    if (value.effective_wavelength) positive(issues, value.effective_wavelength->value, "peaks.wavelength");
    return issues;
}

ValidationIssues validate(const ReferenceCard& value)
{
    ValidationIssues issues;
    check(issues, !value.database_id.empty() && !value.card_id.empty(), "card.identity", "Qualified card identity is required");
    std::set<std::uint64_t> ids;
    for (const auto& reflection : value.reflections) {
        check(issues, reflection.id.value != 0 && ids.insert(reflection.id.value).second,
              "reflection.id", "Invalid or duplicate reflection ID");
        check(issues, reflection.d_spacing.has_value() || reflection.two_theta.has_value(),
              "reflection.position", "At least one measured position is required");
        if (reflection.d_spacing) positive(issues, reflection.d_spacing->value, "reflection.d_spacing");
        if (reflection.two_theta) finite(issues, reflection.two_theta->value, "reflection.two_theta");
        if (reflection.multiplicity) check(issues, *reflection.multiplicity > 0, "reflection.multiplicity", "Must be positive");
        finite(issues, reflection.intensity, "reflection.intensity");
        if (reflection.lorentz_polarization) finite(issues, *reflection.lorentz_polarization, "reflection.lp");
        if (reflection.structure_factor_squared) finite(issues, *reflection.structure_factor_squared, "reflection.fc2");
    }
    if (value.rir) finite(issues, *value.rir, "card.rir");
    if (value.effective_wavelength) positive(issues, value.effective_wavelength->value, "card.wavelength");
    return issues;
}

ValidationIssues validate(const SearchParameters& value)
{
    ValidationIssues issues;
    positive(issues, value.delta_two_theta.value, "search.delta_two_theta");
    for (double weight : {value.weight_position, value.weight_intensity, value.weight_phases})
        check(issues, std::isfinite(weight) && weight >= 0.0, "search.weight", "Expected a nonnegative finite weight");
    finite(issues, value.minimum_score, "search.minimum_score");
    check(issues, !value.filter_strongest || value.strongest_count > 0, "search.strongest_count", "Enabled filtering requires a count");
    return issues;
}

ValidationIssues validate(const SearchResults& value)
{
    ValidationIssues issues;
    check(issues, value.dataset.value != 0, "results.dataset", "Dataset ID must be nonzero");
    check(issues, !value.complete || (value.provenance && !value.provenance->implementation_id.empty() &&
          !value.provenance->implementation_revision.empty() &&
          value.provenance->numerical_policy != NumericalPolicy::unspecified),
          "results.provenance", "Completed results require implementation and numerical-policy provenance");
    std::set<std::pair<std::string, std::string>> ids;
    for (const auto& candidate : value.candidates) {
        check(issues, !candidate.database_id.empty() && !candidate.card_id.empty() &&
              ids.emplace(candidate.database_id, candidate.card_id).second,
              "results.candidate", "Invalid or duplicate qualified card identity");
        if (candidate.outcome) {
            const auto& score = candidate.outcome.value();
            for (double v : {score.total, score.position, score.intensity, score.scale}) finite(issues, v, "results.score");
        }
    }
    for (const auto& phase : value.accepted_phases) {
        check(issues, !phase.database_id.empty() && !phase.card_id.empty(), "results.phase", "Qualified phase identity is required");
        finite(issues, phase.scale, "results.phase.scale");
        if (phase.rir) finite(issues, *phase.rir, "results.phase.rir");
        if (phase.fraction) check(issues, std::isfinite(*phase.fraction) && *phase.fraction >= 0.0 && *phase.fraction <= 1.0,
                                 "results.phase.fraction", "Expected a fraction in [0,1]");
    }
    return issues;
}

ValidationIssues validate(const ScoreInput& value)
{
    auto issues = validate(value.pattern);
    append(issues, validate(value.experimental_peaks));
    append(issues, validate(value.candidate));
    append(issues, validate(value.parameters));
    check(issues, value.pattern.id == value.experimental_peaks.dataset, "score.dataset", "Peaks belong to another dataset");
    check(issues, !value.pattern.observed.empty() && value.pattern.calculation_range.has_value(),
          "score.pattern", "Explicit observed spectrum and calculation range are required");
    check(issues, !value.experimental_peaks.peaks.empty(), "score.peaks", "No experimental peaks");
    const auto& peaks = value.experimental_peaks.peaks;
    for (std::size_t i = 1; i < peaks.size(); ++i)
        check(issues, peaks[i-1].two_theta.value <= peaks[i].two_theta.value, "score.peaks", "Peak order must be nondecreasing");
    for (const auto& reflection : value.candidate.reflections)
        check(issues, reflection.two_theta.has_value(), "score.candidate.two_theta", "Convert coordinates explicitly before scoring");
    return issues;
}

} // namespace qualpf
