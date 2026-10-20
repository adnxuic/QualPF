#pragma once
#include "qualpf/application/product_session.h"
#include "qualpf/preprocessing/geometric_fwhm.h"

namespace qualpf::application {
struct GeometricFwhmRequest {
    preprocessing::GeometricFwhmInput input;
    preprocessing::GeometricFwhmOptions options;
};
struct GeometricFwhmEvaluation {
    ProductHandle source;
    preprocessing::GeometricFwhmResult measurement;
};
// Pure, synchronous evaluation of explicitly supplied owned data (also suitable
// for private drafts). Retains its immutable source, never reads a live session,
// estimates background, smooths, selects an implicit signal or publishes state.
Result<GeometricFwhmEvaluation> evaluateGeometricFwhm(
    ProductHandle, const GeometricFwhmRequest &, const ExecutionContext &);
} // namespace qualpf::application
