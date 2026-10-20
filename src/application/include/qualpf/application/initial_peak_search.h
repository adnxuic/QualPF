#pragma once
#include "qualpf/application/product_session.h"

namespace qualpf::application {
struct InitialPeakEvaluation {
    ProductHandle source;
    InitialPeakConfiguration configuration;
    std::shared_ptr<const InitialPeakUsage> usage;
    ProductHandle preparation;
};
// The existing Classical analysis convention, including private missing-background preparation.
Result<AnalysisSpectrum> prepareAnalysisSpectrum(ProductHandle, Pattern, const ExecutionContext &);
Result<InitialPeakEvaluation> evaluateInitialPeakSearch(
    ProductHandle, InitialPeakConfiguration, const ExecutionContext &);
Status applyInitialPeakSearch(ProductState &, const InitialPeakEvaluation &);
void invalidateInitialPeaksAfterEdit(const ProductState &, ProductState &);
} // namespace qualpf::application
