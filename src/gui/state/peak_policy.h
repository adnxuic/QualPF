#pragma once
#include "gui/state/search_session.h"
#include "qualpf/application/preprocessing_evaluation.h"

namespace qualpf::gui {
using namespace application;

bool peakMatchingEquivalent(const SessionStateView &, const SessionStateView &);
bool peakEditorEquivalent(const SessionStateView &, const SessionStateView &);
// Application policy only. Numerical producers remain independent of Session.
Status finalizePeakEdit(SessionStateView &, const SessionStateView &opening,
                        PeakEditOrigin);
} // namespace qualpf::gui
