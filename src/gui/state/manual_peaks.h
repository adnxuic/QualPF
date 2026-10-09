#pragma once
#include "gui/state/preprocessing_edit.h"

namespace qualpf::gui {
using namespace application;
// Mutates only the transaction's private draft.
Status prepareManualPeaks(SessionStateView &, const ManualPeakChange &, WavelengthAngstrom, AngularWidthDegrees);
} // namespace qualpf::gui
