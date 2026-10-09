#pragma once
#include "qualpf/application/presentation/plot_snapshot.h"
#include "gui/state/search_session.h"

namespace qualpf::gui::presentation {
using namespace application; using namespace qualpf::gui;
using namespace application::presentation;
// Defined legacy filename transport: fixed 256 local bytes, '/' separator,
// extension removal only up to four bytes, then the view's UTF-8 decoding.
// Pattern::source_id remains the original UTF-8 identity.
std::string compatibility_plot_name(const std::string &source_id);
// GUI projection adapter. Reads a const snapshot;
// callers may pass PreprocessingEdit::preview() while the edit stays private.
PlotInput make_plot_input(const SessionStateView &, const PresentationWorkspace &, double card_display_wavelength);
} // namespace qualpf::gui::presentation
