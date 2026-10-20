#include "qualpf/search/algorithms.h"
#include "../preprocessing/analysis_signal.h"

#include "qualpf/domain/validation.h"
#include "qualpf/numerics/basic.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace qualpf::search {
namespace {
static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);

Error numeric_error(const char *field, const char *reason = "nonfinite_fom_arithmetic") {
    return {ErrorCode::numerical_failure, field,
            std::string("Classic scoring failed: ") + reason, reason};
}
// clocate permits repeated values (including values collapsed by REAL
// rounding). S4 nearest_index rejects duplicates, so it cannot implement this
// contract.
std::size_t closest(const std::vector<float> &x, float target) {
    if (target <= x.front())
        return 0;
    if (target >= x.back())
        return x.size() - 1;
    std::size_t low = 0, high = x.size() - 1;
    while (high - low > 1) {
        const auto mid = (low + high) / 2;
        if (target >= x[mid])
            low = mid;
        else
            high = mid;
    }
    return target - x[low] < x[high] - target ? low : high;
}
} // namespace

static Result<ClassicMatch> match_kernel(const ScoreInput &input, const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<ClassicMatch>{Error{ErrorCode::cancelled, "score", "Cancelled before scoring"}};
    if (context.numerical_policy != NumericalPolicy::legacy_mixed_precision)
        return Result<ClassicMatch>{Error{ErrorCode::unsupported, "numerical_policy",
                                        "Select the explicit binary32/binary64 legacy policy"}};
    const auto issues = validate(input);
    if (!issues.empty())
        return Result<ClassicMatch>{issues.front()};
    const auto &p = input.parameters;
    if (p.weight_position > 1 || p.weight_intensity > 1 || p.weight_phases > 1)
        return Result<ClassicMatch>{
            Error{ErrorCode::invalid_input, "search.weight", "Legacy slider weights must be in [0,1]"}};
    const auto &peaks = input.experimental_peaks.peaks;
    const auto &card = input.candidate.reflections;
    if (peaks.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        card.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return Result<ClassicMatch>{
            Error{ErrorCode::invalid_input, "score.size", "Exceeds legacy integer count"}};
    for (const auto &peak : peaks)
        if (peak.intensity < 0)
            return Result<ClassicMatch>{Error{ErrorCode::invalid_input, "peak.intensity",
                                            "Negative peak heights are outside the compatibility contract"}};
    for (const auto &r : card)
        if (r.intensity < 0)
            return Result<ClassicMatch>{Error{ErrorCode::invalid_input, "reflection.intensity",
                                            "Negative card heights are outside the compatibility contract"}};

    // Explicitly reproduce the REAL storage boundary of dataset(1).
    const auto range = *input.pattern.calculation_range;
    const auto minimum = numerics::to_float32(range.minimum.value);
    const auto maximum = numerics::to_float32(range.maximum.value);
    if (!minimum)
        return Result<ClassicMatch>{minimum.error()};
    if (!maximum)
        return Result<ClassicMatch>{maximum.error()};
    const auto lower = numerics::to_float32(double(minimum.value()) - p.delta_two_theta.value);
    const auto upper = numerics::to_float32(double(maximum.value()) + p.delta_two_theta.value);
    if (!lower)
        return Result<ClassicMatch>{lower.error()};
    if (!upper)
        return Result<ClassicMatch>{upper.error()};
    std::vector<float> x, y, exp_x, card_x, card_y;
    for (std::size_t i = 0; i < input.pattern.two_theta.size(); ++i) {
        auto a = numerics::to_float32(input.pattern.two_theta[i].value);
        auto b = numerics::to_float32(input.pattern.observed[i]);
        if (!a)
            return Result<ClassicMatch>{a.error()};
        if (!b)
            return Result<ClassicMatch>{b.error()};
        x.push_back(a.value());
        y.push_back(b.value());
    }
    for (const auto &peak : peaks) {
        auto a = numerics::to_float32(peak.two_theta.value);
        if (!a)
            return Result<ClassicMatch>{a.error()};
        exp_x.push_back(a.value());
    }
    ClassicMatch out;
    out.sample_x = x;
    out.sample_y = y;
    out.merged.resize(peaks.size());
    out.associated.resize(peaks.size());
    for (std::size_t i = 0; i < card.size(); ++i) {
        // Compare the original ABI double against rounded REAL bounds BEFORE
        // narrowing.
        const double angle = card[i].two_theta->value;
        if (angle < lower.value() || angle > upper.value())
            continue;
        auto a = numerics::to_float32(angle);
        auto b = numerics::to_float32(card[i].intensity);
        if (!a)
            return Result<ClassicMatch>{a.error()};
        if (!b)
            return Result<ClassicMatch>{b.error()};
        out.reflection_indices.push_back(i);
        card_x.push_back(a.value());
        card_y.push_back(b.value());
    }
    out.card_x = card_x;
    out.card_y = card_y;
    const auto nd = card_x.size();
    if (nd == 0)
        return Result<ClassicMatch>{std::move(out)};
    out.disposition = ScoreDisposition::no_associations;
    out.experimental_indices.resize(nd);
    auto &merged = out.merged;
    auto &associated = out.associated;
    float distance_sum = 0.0f;
    for (std::size_t j = 0; j < nd; ++j) {
        if (context.cancellation.stop_requested())
            return Result<ClassicMatch>{Error{ErrorCode::cancelled, "score", "Cancelled during association"}};
        const auto i = closest(exp_x, card_x[j]);
        const auto distance = numerics::to_float32(std::abs(peaks[i].two_theta.value - card_x[j]));
        if (!distance)
            return Result<ClassicMatch>{distance.error()};
        if (distance.value() < p.delta_two_theta.value) {
            out.experimental_indices[j] = i;
            ++out.associated_reflections;
            distance_sum += distance.value();
            merged[i] += card_y[j];
            associated[i] = true;
        }
    }
    if (out.associated_reflections == 0)
        return Result<ClassicMatch>{std::move(out)};
    out.associated_peaks = static_cast<std::size_t>(std::count(associated.begin(), associated.end(), true));
    out.position_difference_sum = distance_sum;
    return Result<ClassicMatch>{std::move(out)};
}

Result<ScoreTrace> calculate_classic_fom(const ScoreInput &input, const ClassicMatch &match,
                                                    const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<ScoreTrace>{Error{ErrorCode::cancelled, "score", "Cancelled before scoring"}};
    if (match.contract != classic_match_contract())
        return Result<ScoreTrace>{Error{ErrorCode::unsupported, "algorithms.match_contract",
                                        "Incompatible classic match evidence", "incompatible_match_contract"}};
    if (context.numerical_policy != NumericalPolicy::legacy_mixed_precision)
        return Result<ScoreTrace>{Error{ErrorCode::unsupported, "numerical_policy",
                                        "Select the explicit binary32/binary64 legacy policy"}};
    const auto issues = validate(input);
    if (!issues.empty())
        return Result<ScoreTrace>{issues.front()};
    const auto &p = input.parameters;
    const auto &peaks = input.experimental_peaks.peaks;
    const auto nd = match.card_x.size();
    const auto finite = [](const auto &values) {
        return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
    };
    if (match.card_y.size() != nd || match.reflection_indices.size() != nd ||
        match.experimental_indices.size() != nd || match.merged.size() != peaks.size() ||
        match.associated.size() != peaks.size() || match.sample_x.size() != input.pattern.observed.size() ||
        match.sample_y.size() != match.sample_x.size() ||
        match.associated_reflections > nd || match.associated_peaks > peaks.size() ||
        (match.associated_reflections > 0 && match.associated_peaks == 0) ||
        std::any_of(match.experimental_indices.begin(), match.experimental_indices.end(),
                    [&](auto index) { return index && *index >= peaks.size(); }) ||
        std::any_of(match.reflection_indices.begin(), match.reflection_indices.end(),
                    [&](auto index) { return index >= input.candidate.reflections.size(); }) ||
        match.associated_reflections != std::size_t(std::count_if(match.experimental_indices.begin(),
            match.experimental_indices.end(), [](auto index) { return index.has_value(); })) ||
        match.associated_peaks != std::size_t(std::count(match.associated.begin(), match.associated.end(), true)) ||
        !finite(match.sample_x) || !finite(match.sample_y) || !finite(match.card_x) || !finite(match.card_y) ||
        !std::is_sorted(match.sample_x.begin(), match.sample_x.end()) ||
        !std::isfinite(match.position_difference_sum) || match.position_difference_sum < 0 ||
        std::any_of(match.card_y.begin(), match.card_y.end(), [](float y) { return y < 0; }) ||
        std::any_of(match.merged.begin(), match.merged.end(), [](float y) { return y < 0; }))
        return Result<ScoreTrace>{Error{ErrorCode::invalid_input, "algorithms.match_evidence",
                                        "Malformed classic match evidence", "invalid_match_evidence"}};
    ScoreTrace out;
    out.disposition = match.disposition;
    out.reflection_indices = match.reflection_indices;
    out.experimental_indices = match.experimental_indices;
    out.merged_intensities.resize(peaks.size());
    out.scaled_intensities.resize(peaks.size());
    if (nd == 0 || match.associated_reflections == 0)
        return Result<ScoreTrace>{std::move(out)};
    out.associated_reflections = match.associated_reflections;
    out.associated_peaks = match.associated_peaks;
    out.position_difference_sum = match.position_difference_sum;
    auto merged = match.merged;
    const auto &associated = match.associated;
    const auto &x = match.sample_x, &y = match.sample_y;
    const auto &card_x = match.card_x, &card_y = match.card_y;
    const float distance_sum = match.position_difference_sum;
    float sum_squared = 0.0f, sum_product = 0.0f;
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        out.merged_intensities[i] = merged[i];
        if (associated[i]) {
            sum_squared += merged[i] * merged[i];
            // exp_intensity is c_double; assignment rounds the whole expression.
            const auto product = numerics::to_float32(double(sum_product) + peaks[i].intensity * merged[i]);
            if (!product)
                return Result<ScoreTrace>{numeric_error("scale.product")};
            sum_product = product.value();
        }
    }
    if (!std::isfinite(sum_squared))
        return Result<ScoreTrace>{numeric_error("scale.squared")};
    const float scale = sum_squared > 0.0f ? sum_product / sum_squared : 1.0f;
    if (!std::isfinite(scale))
        return Result<ScoreTrace>{numeric_error("scale")};
    const float position = static_cast<float>(
        1.0 - distance_sum / (double(out.associated_reflections) * p.delta_two_theta.value));
    float intensity = 0.0f;
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        merged[i] *= scale;
        if (!std::isfinite(merged[i]))
            return Result<ScoreTrace>{numeric_error("scaled_intensity")};
        out.scaled_intensities[i] = merged[i];
        if (associated[i] && scale > 0.0f) {
            const double denom = std::max(peaks[i].intensity, double(merged[i]));
            if (denom == 0.0)
                return Result<ScoreTrace>{numeric_error("intensity.denominator", "zero_intensity_denominator")};
            intensity =
                static_cast<float>(double(intensity) + std::abs(peaks[i].intensity - merged[i]) / denom);
        }
    }
    if (scale > 0.0f)
        intensity = 1.0f - intensity / static_cast<float>(out.associated_peaks);
    for (std::size_t j = 0; j < nd; ++j) {
        if (!out.experimental_indices[j]) {
            const float difference = card_y[j] * scale - y[closest(x, card_x[j])];
            if (!std::isfinite(difference))
                return Result<ScoreTrace>{numeric_error("unassociated.difference")};
            if (difference < 0.0f)
                ++out.unassociated_below_observed;
        }
    }
    const auto phases = numerics::nearest_integer(-9.0 * p.weight_phases + 10.0);
    if (!phases || phases.value() <= 0)
        return Result<ScoreTrace>{numeric_error("phases")};
    out.expected_peaks_per_phase =
        std::max(std::size_t{1}, peaks.size() / static_cast<std::size_t>(phases.value()));
    const float nda = static_cast<float>(out.associated_reflections);
    float database_fraction = nda / static_cast<float>(nd - out.unassociated_below_observed);
    if (out.associated_reflections < out.expected_peaks_per_phase)
        database_fraction *= std::sqrt(nda / static_cast<float>(out.expected_peaks_per_phase));
    const float sample_fraction = static_cast<float>(out.associated_peaks) / static_cast<float>(peaks.size());
    float sum_sample = 0.0f, sum_card = 0.0f, total_card = 0.0f;
    double sample_double_sum = 0.0;
    for (std::size_t i = 0; i < peaks.size(); ++i) {
        sample_double_sum += peaks[i].intensity;
        if (associated[i]) {
            auto next = numerics::to_float32(double(sum_sample) + peaks[i].intensity);
            if (!next)
                return Result<ScoreTrace>{numeric_error("associated_sample_sum")};
            sum_sample = next.value();
            sum_card += merged[i];
        }
    }
    // Deliberately sequential source-order REAL reduction; not S4 compensated
    // sum.
    for (float value : card_y)
        total_card += value * scale;
    auto total_sample_result = numerics::to_float32(sample_double_sum);
    if (!total_sample_result)
        return Result<ScoreTrace>{numeric_error("total_sample_sum")};
    const float total_sample = total_sample_result.value();
    if (!std::isfinite(sum_card) || !std::isfinite(total_card))
        return Result<ScoreTrace>{numeric_error("intensity_sums")};
    if (total_card <= 0.0f || total_sample <= 0.0f)
        return Result<ScoreTrace>{numeric_error("intensity_sums", "zero_total_intensity")};
    const float sqrt_sample = std::sqrt((sum_sample / total_sample) * sample_fraction);
    const float sqrt_database = std::sqrt((sum_card / total_card) * database_fraction);
    const bool multiple = out.associated_peaks > 1;
    const double denominator = multiple ? p.weight_position + p.weight_intensity + p.weight_phases
                                        : p.weight_position + p.weight_phases;
    if (denominator <= 0.0)
        return Result<ScoreTrace>{numeric_error("weights.denominator", "zero_effective_weight")};
    const double numerator = multiple ? p.weight_position * position + p.weight_intensity * intensity +
                                            p.weight_phases * sqrt_sample
                                      : p.weight_position * position + p.weight_phases * sqrt_sample;
    float total = static_cast<float>(sqrt_database * numerator / denominator);
    total = std::sqrt(total);
    if (!std::isfinite(total) || !std::isfinite(position) || !std::isfinite(intensity))
        return Result<ScoreTrace>{numeric_error("score")};
    if (context.cancellation.stop_requested())
        return Result<ScoreTrace>{Error{ErrorCode::cancelled, "score", "Cancelled before publishing"}};
    out.score = {total, position, intensity, scale};
    out.disposition = ScoreDisposition::scored;
    out.associated_sample_sum = sum_sample;
    out.associated_card_sum = sum_card;
    out.total_sample_sum = total_sample;
    out.total_card_sum = total_card;
    out.database_fraction = database_fraction;
    out.sample_fraction = sample_fraction;
    out.sqrt_database = sqrt_database;
    out.sqrt_sample = sqrt_sample;
    return Result<ScoreTrace>{std::move(out)};
}

Result<ClassicMatch> ClassicMatcher::match(const ClassicMatchInput &input, const ExecutionContext &context) const {
    if (context.cancellation.stop_requested()) return Result<ClassicMatch>{Error{ErrorCode::cancelled,"score","Cancelled before scoring"}};
    if (context.numerical_policy != NumericalPolicy::legacy_mixed_precision)
        return Result<ClassicMatch>{Error{ErrorCode::unsupported, "numerical_policy",
            "Select the explicit binary32/binary64 legacy policy"}};
    auto checked = AnalysisSpectrum::create(input.spectrum.pattern(), AnalysisPurpose::classical_search);
    if (!checked) return Result<ClassicMatch>{checked.error()};
    auto prepared = preprocessing::detail::prepare_analysis_signal(checked.value());
    if (!prepared) return Result<ClassicMatch>{prepared.error()};
    return match_kernel({prepared.value(), input.experimental_peaks, input.candidate, input.parameters}, context);
}
Result<ScoreTrace> score_classic(const ClassicMatchInput &input, const IClassicMatcher &matcher,
                                const ExecutionContext &context) {
    if (matcher.produces() != classic_match_contract())
        return Result<ScoreTrace>{Error{ErrorCode::unsupported, "algorithms.match_contract",
                                        "Incompatible matcher and FOM", "incompatible_match_contract"}};
    auto matched = matcher.match(input, context);
    if (!matched)
        return Result<ScoreTrace>{matched.error()};
    if (matched.value().contract != matcher.produces())
        return Result<ScoreTrace>{Error{ErrorCode::unsupported, "algorithms.match_contract",
                                        "Matcher produced an undeclared contract", "incompatible_match_contract"}};
    return calculate_classic_fom({input.spectrum.pattern(), input.experimental_peaks, input.candidate, input.parameters}, matched.value(), context);
}
Result<ScoreTrace> score_classic(const ScoreInput &input, const ExecutionContext &context) {
    auto matched = match_kernel(input, context);
    if (!matched) return Result<ScoreTrace>{matched.error()};
    return calculate_classic_fom(input, matched.value(), context);
}

Result<ScoreValues> ClassicScorer::score(const ScoreInput &input, const ExecutionContext &context) {
    auto result = score_classic(input, context);
    if (!result)
        return Result<ScoreValues>{result.error()};
    return Result<ScoreValues>{result.value().score};
}
} // namespace qualpf::search
