#pragma once
#include "qualpf/application/search_coordinator.h"

namespace qualpf::application {
// Frozen-data operations belong to their analysis library, independently of
// which (possibly unavailable) database is selected in the current workspace.
// Actual reference reads retain the provider identity and its existing guard.
inline bool projectOperationReadsReferences(SearchOperation operation) {
    return operation == SearchOperation::initial || operation == SearchOperation::scored_browse ||
           operation == SearchOperation::metadata_browse || operation == SearchOperation::load_card;
}
inline ReferenceDatabase projectOperationDatabase(const SearchInput &input, ReferenceDatabase selected) {
    if (!projectOperationReadsReferences(input.operation) && input.source &&
        !input.source->analysis_database.empty())
        selected.identity = input.source->analysis_database;
    return selected;
}
} // namespace qualpf::application
