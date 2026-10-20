#include "qualpf/application/geometric_fwhm.h"

namespace qualpf::application {
Result<GeometricFwhmEvaluation> evaluateGeometricFwhm(ProductHandle source,
    const GeometricFwhmRequest &request, const ExecutionContext &context) {
    using Return = Result<GeometricFwhmEvaluation>;
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "fwhm.geometric", "FWHM estimation canceled"}};
    if (!source)
        return Return{Error{ErrorCode::missing_data, "fwhm.source", "Missing source"}};
    if (request.input.dataset != source->pattern.id)
        return Return{Error{ErrorCode::stale_snapshot, "fwhm.source", "Input belongs to another pattern"}};
    if (request.input.processing.background_subtracted != false)
        return Return{Error{ErrorCode::missing_data,"background.state",
            "Geometric FWHM requires explicitly unsubtracted observations and background",
            request.input.processing.background_subtracted ? "subtracted_input_refused" : "subtraction_state_unknown"}};
    auto result = preprocessing::estimate_geometric_fwhm(request.input, request.options, context);
    if (!result) return Return{result.error()};
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "fwhm.geometric", "FWHM estimation canceled"}};
    return Return{GeometricFwhmEvaluation{std::move(source), std::move(result).value()}};
}
} // namespace qualpf::application
