#include "gui/state/peak_policy.h"
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::gui {
using namespace application;
bool peakMatchingEquivalent(const SessionStateView &a,const SessionStateView &b) {return peakMatchingEquivalent(productState(a),productState(b));}
bool peakEditorEquivalent(const SessionStateView &a,const SessionStateView &b) {return peakEditorEquivalent(productState(a),productState(b));}
Status finalizePeakEdit(SessionStateView &next,const SessionStateView &opening,PeakEditOrigin origin) {
    auto draft=productState(next); auto status=finalizePeakEdit(draft,productState(opening),origin);
    if (status) next=searchView(std::make_shared<const ProductState>(std::move(draft)));
    return status;
}
}
