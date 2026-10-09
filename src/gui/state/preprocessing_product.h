#include "gui/state/workspace_context.h"
#pragma once
#include "gui/state/preprocessing_edit.h"

namespace qualpf::gui {
using namespace application;

// C++ is the product default; QUALPF_PREPROCESSING_BACKEND=legacy opts into fallback.
// A failed C++ transaction never automatically falls back.
bool cppPreprocessingProduct();
// Background compatibility has one explicit selector; QA for other P3 domains
// never selects a native background editor, gesture, or subtraction implicitly.
Status peakEditEntryStatus(qualpf::gui::WorkspaceContext &context);
// An aligned zero transport after subtraction is not an active background.

Status applyPreprocessing(SessionView &, PreprocessingAction);
Status editBackgroundPoint(SessionView &, bool add, double x, double y, bool redetect);
} // namespace qualpf::gui
