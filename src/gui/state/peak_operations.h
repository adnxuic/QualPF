#pragma once
#include "gui/state/preprocessing_edit.h"

namespace qualpf::gui {
using namespace application;
Result<Pattern> preparePeakMeasurementContext(const SessionStateView &,
                                              WavelengthAngstrom);
Status preparePeakOperation(SessionStateView &, const PreprocessingRequest &);
} // namespace qualpf::gui
