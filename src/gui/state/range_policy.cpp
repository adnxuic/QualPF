#include "gui/state/range_policy.h"
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::gui {
using namespace application;
void finalizeRangeState(SessionStateView &next,const SessionStateView &opening) {
    auto draft=productState(next);finalizeRangeState(draft,productState(opening));
    next=searchView(std::make_shared<const ProductState>(std::move(draft)));
}
}
