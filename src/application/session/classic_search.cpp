#include "qualpf/application/classic_search.h"

#include <algorithm>

namespace qualpf::application {
namespace {
Result<const DatasetState *> active(const SnapshotHandle &input) {
    if (!input || !input->inputs.active_dataset || !input->inputs.search_parameters)
        return Result<const DatasetState *>{Error{ErrorCode::missing_data, "search.snapshot",
                                                  "Active dataset and explicit settings required"}};
    const auto &datasets = input->inputs.datasets;
    auto found = std::find_if(datasets.begin(), datasets.end(),
                              [&](const auto &d) { return d.pattern.id == *input->inputs.active_dataset; });
    if (found == datasets.end() || !found->peaks)
        return Result<const DatasetState *>{
            Error{ErrorCode::missing_data, "search.peaks", "Snapshot has no active peaks"}};
    return Result<const DatasetState *>{&*found};
}
Error cancelled() { return {ErrorCode::cancelled, "search", "Search was cancelled before publication"}; }
} // namespace

Result<PreparedClassicSearch> prepare_classic_search(SnapshotHandle input,
                                                     std::span<const search::CandidateInput> candidates,
                                                     search::SearchPass pass,
                                                     const ExecutionContext &context) {
    auto dataset = active(input);
    if (!dataset)
        return Result<PreparedClassicSearch>{dataset.error()};
    const auto &d = *dataset.value();
    auto evaluated = search::evaluate_candidates(d.pattern, *d.peaks, candidates,
                                                 *input->inputs.search_parameters, pass, context);
    if (!evaluated)
        return Result<PreparedClassicSearch>{evaluated.error()};
    for (const auto &candidate : evaluated.value().evaluated)
        if (!candidate.outcome)
            return Result<PreparedClassicSearch>{candidate.outcome.error()};
    return Result<PreparedClassicSearch>{
        PreparedClassicSearch{std::move(input), std::move(evaluated).value(), std::nullopt}};
}

Result<PreparedClassicSearch> prepare_classic_residual(SnapshotHandle input, const ReferenceCard &accepted,
                                                       double scale,
                                                       std::span<const search::CandidateInput> candidates,
                                                       const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<PreparedClassicSearch>{cancelled()};
    auto dataset = active(input);
    if (!dataset)
        return Result<PreparedClassicSearch>{dataset.error()};
    const auto &d = *dataset.value();
    auto residual = search::subtract_phase(*d.peaks, accepted, scale, *input->inputs.search_parameters);
    if (!residual)
        return Result<PreparedClassicSearch>{residual.error()};
    auto evaluated =
        search::evaluate_candidates(d.pattern, residual.value().peaks, candidates,
                                    *input->inputs.search_parameters, search::SearchPass::rescore, context);
    if (!evaluated)
        return Result<PreparedClassicSearch>{evaluated.error()};
    for (const auto &candidate : evaluated.value().evaluated)
        if (!candidate.outcome)
            return Result<PreparedClassicSearch>{candidate.outcome.error()};
    return Result<PreparedClassicSearch>{PreparedClassicSearch{std::move(input), std::move(evaluated).value(),
                                                               std::move(residual).value().peaks}};
}

Status PreparedClassicSearch::publish(Session &session, const ExecutionContext &context) const {
    if (context.cancellation.stop_requested())
        return Status{cancelled()};
    if (residual_)
        return session.publish_search_transition(input_, *residual_, evaluation_.results);
    return session.publish(input_, evaluation_.results);
}
} // namespace qualpf::application
