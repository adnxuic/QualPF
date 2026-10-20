#include "qualpf/application/search_preparation.h"
#include "classical_workflow.h"
#include "qualpf/domain/validation.h"
#include "qualpf/application/workflow.h"
namespace qualpf::application {
Result<SearchPreparation> prepareSearch(ProductHandle source, SearchMethod method) {
    auto available = preflightSearchMethod(method);
    if (!available) return Result<SearchPreparation>{available.error()};
    if (!source || !source->algorithms)
        return Result<SearchPreparation>{Error{ErrorCode::missing_data, "search.source", "Missing search snapshot"}};
    if (method != source->algorithms->spec.method())
        return Result<SearchPreparation>{Error{ErrorCode::stale_snapshot, "search.method", "Search method disagrees with the source", "algorithm_provenance_mismatch"}};
    if (source->editing)
        return Result<SearchPreparation>{Error{ErrorCode::invalid_input, "session.edit", "Finish the preprocessing edit before searching"}};
    SearchPreparation prepared;
    prepared.source = source;
    if (method == SearchMethod::pfsm) {
        if (source->pattern.observed.empty()) return Result<SearchPreparation>{Error{ErrorCode::missing_data,"pfsm.observations","PFSM requires a continuous spectrum"}};
        const auto issues = validate(source->pattern);
        if (!issues.empty()) return Result<SearchPreparation>{issues.front()};
        if (source->projectSettings && !source->pfsm && !source->prepared_pfsm)
            prepared.state = SearchPreparation::State::needs_pfsm_preparation;
        return Result<SearchPreparation>{std::move(prepared)};
    }
    auto spectrum = validateClassicalSpectrum(source->pattern);
    if (!spectrum) return Result<SearchPreparation>{spectrum.error()};
    if (source->peaks.valid && (source->pattern.observed.empty() ||
        reusableWorkflowStep(*source, WorkflowId::csm, WorkflowStep::peaks))) {
        auto peaks = productSearchPeaks(source->peaks);
        if (!peaks) return Result<SearchPreparation>{peaks.error()};
        if (peaks.value().peaks.empty())
            return Result<SearchPreparation>{Error{ErrorCode::missing_data, "peaks", "No experimental peaks are available"}};
    } else {
        auto request = automaticPreprocessingRequest(source, PreprocessingAction::prepare_search, method);
        if (!request) return Result<SearchPreparation>{request.error()};
        prepared.state = SearchPreparation::State::needs_peak_preparation;
        prepared.peaks = std::move(request).value();
    }
    return Result<SearchPreparation>{std::move(prepared)};
}
} // namespace qualpf::application
