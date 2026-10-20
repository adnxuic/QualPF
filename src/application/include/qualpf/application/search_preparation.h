#pragma once
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::application {
struct SearchPreparation {
    enum class State { ready, needs_peak_preparation, needs_pfsm_preparation };
    State state = State::ready;
    ProductHandle source;
    std::optional<PreprocessingRequest> peaks;
};
// Method admission precedes Classical-specific input checks. No computation or publication.
Result<SearchPreparation> prepareSearch(ProductHandle, SearchMethod);
} // namespace qualpf::application
