#pragma once
#include "gui/state/search_session.h"

namespace qualpf::gui {
using namespace application;
// R2: ordinary exact numerical equality; provenance/dirty/barrier are separate.

// R3/R4: interpret the validated RP1 construction without changing its values.
void finalizeRangeState(SessionStateView &, const SessionStateView &opening);
} // namespace qualpf::gui
