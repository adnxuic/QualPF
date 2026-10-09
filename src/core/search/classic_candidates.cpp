#include "qualpf/search/classic.h"

#include "qualpf/domain/validation.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

namespace qualpf::search {
namespace {
bool nearby(const PeakSet &peaks, double target) {
    // Preserve SearchUtil's descending binary search + three-neighbour probe.
    // With nonuniform tolerances this is NOT equivalent to testing every peak.
    std::ptrdiff_t left = 0, right = static_cast<std::ptrdiff_t>(peaks.peaks.size()) - 1;
    const auto matches = [&](std::ptrdiff_t i) {
        const auto &p = peaks.peaks[static_cast<std::size_t>(i)];
        return std::abs(target - p.d_spacing->value) <= p.delta_d->value;
    };
    while (left <= right) {
        const auto mid = (left + right) / 2;
        if (matches(mid))
            return true;
        if (peaks.peaks[static_cast<std::size_t>(mid)].d_spacing->value < target)
            right = mid - 1;
        else
            left = mid + 1;
    }
    const auto last = static_cast<std::ptrdiff_t>(peaks.peaks.size()) - 1;
    for (auto k = std::max(std::ptrdiff_t{0}, left - 1); k <= std::min(last, left + 1); ++k)
        if (matches(k))
            return true;
    return false;
}
bool deleted_quality(const std::string &quality) {
    // SQL trim(quality) removes ASCII spaces by default, not general whitespace.
    const auto first = quality.find_first_not_of(' ');
    return first != std::string::npos &&
           quality.substr(first, quality.find_last_not_of(' ') - first + 1) == "D";
}
} // namespace

Result<bool> passes_strongest(const PeakSet &peaks, std::span<const DSpacingAngstrom> stored_top) {
    const auto issues = validate(peaks);
    if (!issues.empty())
        return Result<bool>{issues.front()};
    for (std::size_t i = 0; i < peaks.peaks.size(); ++i) {
        const auto &peak = peaks.peaks[i];
        if (!peak.d_spacing || !peak.delta_d)
            return Result<bool>{
                Error{ErrorCode::missing_data, "prefilter.d", "Explicit d and delta_d are required"}};
        if (i && peaks.peaks[i - 1].d_spacing->value < peak.d_spacing->value)
            return Result<bool>{
                Error{ErrorCode::invalid_input, "prefilter.d", "Expected nonincreasing d order"}};
    }
    const auto count = std::min(std::size_t{3}, stored_top.size());
    for (const auto value : stored_top.first(count))
        if (!std::isfinite(value.value) || value.value <= 0)
            return Result<bool>{
                Error{ErrorCode::invalid_input, "prefilter.top", "Expected finite positive stored d"}};
    if (count > peaks.peaks.size())
        return Result<bool>{false};
    for (const auto value : stored_top.first(count))
        if (!nearby(peaks, value.value))
            return Result<bool>{false};
    return Result<bool>{true};
}

Result<BatchEvaluation> evaluate_candidates(const Pattern &pattern, const PeakSet &peaks,
                                            std::span<const CandidateInput> candidates,
                                            const SearchParameters &parameters, SearchPass pass,
                                            const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<BatchEvaluation>{Error{ErrorCode::cancelled, "search", "Cancelled before batch"}};
    if (pass != SearchPass::initial && pass != SearchPass::rescore)
        return Result<BatchEvaluation>{Error{ErrorCode::invalid_input, "search.pass", "Unknown search pass"}};
    if (context.numerical_policy != NumericalPolicy::legacy_mixed_precision)
        return Result<BatchEvaluation>{
            Error{ErrorCode::unsupported, "numerical_policy", "Select the legacy precision policy"}};
    auto issues = validate(parameters);
    if (!issues.empty())
        return Result<BatchEvaluation>{issues.front()};
    issues = validate(pattern);
    if (!issues.empty())
        return Result<BatchEvaluation>{issues.front()};
    issues = validate(peaks);
    if (!issues.empty())
        return Result<BatchEvaluation>{issues.front()};
    if (pattern.id != peaks.dataset || pattern.observed.empty() || !pattern.calculation_range)
        return Result<BatchEvaluation>{Error{ErrorCode::invalid_input, "search.pattern",
                                             "Matching dataset and explicit spectrum/range are required"}};
    if (pass == SearchPass::initial && peaks.peaks.empty())
        return Result<BatchEvaluation>{
            Error{ErrorCode::missing_data, "search.peaks", "Initial search requires experimental peaks"}};
    for (std::size_t i = 0; i < peaks.peaks.size(); ++i)
        if (peaks.peaks[i].intensity < 0 ||
            (i && peaks.peaks[i - 1].two_theta.value > peaks.peaks[i].two_theta.value))
            return Result<BatchEvaluation>{Error{ErrorCode::invalid_input, "search.peaks",
                                                 "Expected ordered peaks with nonnegative heights"}};
    if (parameters.weight_position > 1 || parameters.weight_intensity > 1 || parameters.weight_phases > 1 ||
        (parameters.filter_strongest && parameters.strongest_count != 3))
        return Result<BatchEvaluation>{Error{ErrorCode::unsupported, "search.parameters",
                                             "Legacy weights are [0,1] and strongest count is 3"}};
    std::set<std::pair<std::string, std::string>> identities;
    for (const auto &candidate : candidates) {
        issues = validate(candidate.card);
        if (!issues.empty())
            return Result<BatchEvaluation>{issues.front()};
        if (!identities.emplace(candidate.card.database_id, candidate.card.card_id).second)
            return Result<BatchEvaluation>{
                Error{ErrorCode::invalid_input, "search.candidate", "Duplicate qualified card identity"}};
        if (pass == SearchPass::rescore &&
            (!candidate.previous_scale || !std::isfinite(*candidate.previous_scale) ||
             *candidate.previous_scale < 0))
            return Result<BatchEvaluation>{Error{ErrorCode::missing_data, "search.previous_scale",
                                                 "Rescore requires a finite nonnegative previous scale"}};
    }
    BatchEvaluation out;
    out.results.dataset = pattern.id;
    out.results.provenance = ComputationProvenance{"qualpf.classic", "s5-source-contract-1",
                                                   context.numerical_policy, context.reference_data_revision};
    for (const auto &candidate : candidates) {
        if (context.cancellation.stop_requested())
            return Result<BatchEvaluation>{Error{ErrorCode::cancelled, "search",
                                                 "Cancelled during batch; no partial results published"}};
        bool accepted = true;
        // Recalculation does not query the database or reapply its prefilters.
        if (pass == SearchPass::initial) {
            accepted = parameters.include_deleted_cards || !deleted_quality(candidate.card.quality);
            if (accepted && parameters.filter_strongest) {
                if (!candidate.stored_top)
                    return Result<BatchEvaluation>{
                        Error{ErrorCode::missing_data, "search.stored_top",
                              "Decoded top index required; it must not be reconstructed"}};
                auto filter = passes_strongest(peaks, *candidate.stored_top);
                if (!filter)
                    return Result<BatchEvaluation>{filter.error()};
                accepted = filter.value();
            }
        }
        out.prefilter_passed.push_back(accepted);
        if (!accepted)
            continue;
        // Legacy rescore explicitly handles the fully exhausted residual peak list.
        Result<ScoreValues> score = Result<ScoreValues>{ScoreValues{0, 0, 0, 1}};
        if (!peaks.peaks.empty()) {
            ClassicScorer scorer;
            score = scorer.score({pattern, peaks, candidate.card, parameters}, context);
        } else if (pass == SearchPass::initial) {
            return Result<BatchEvaluation>{
                Error{ErrorCode::missing_data, "search.peaks", "Initial search requires experimental peaks"}};
        }
        if (!score && score.error().code == ErrorCode::cancelled)
            return Result<BatchEvaluation>{score.error()};
        out.evaluated.push_back({candidate.card.database_id, candidate.card.card_id, score});
        if (!score)
            continue; // Preserve a diagnostic, never turn failure into a ranked zero.
        const auto total = score.value().total;
        const bool keep = pass == SearchPass::initial ? total > parameters.minimum_score
                                                      : total > 0 && total >= parameters.minimum_score;
        if (keep) {
            if (pass == SearchPass::rescore)
                score.value().scale = *candidate.previous_scale;
            out.results.candidates.push_back({candidate.card.database_id, candidate.card.card_id, score});
        }
    }
    auto &retained = out.results.candidates;
    if (pass == SearchPass::rescore || retained.size() > parameters.maximum_entries)
        std::stable_sort(retained.begin(), retained.end(), [](const auto &a, const auto &b) {
            return a.outcome.value().total > b.outcome.value().total;
        });
    if (retained.size() > parameters.maximum_entries)
        retained.erase(retained.begin() + static_cast<std::ptrdiff_t>(parameters.maximum_entries),
                       retained.end());
    if (context.cancellation.stop_requested())
        return Result<BatchEvaluation>{Error{ErrorCode::cancelled, "search", "Cancelled before publishing"}};
    out.results.complete = true;
    return Result<BatchEvaluation>{std::move(out)};
}
} // namespace qualpf::search
