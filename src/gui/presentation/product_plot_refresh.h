#pragma once

#include "gui/presentation/search_plot_input.h"
#include <functional>
#include "gui/state/workspace_context.h"

namespace qualpf::gui::presentation {
using namespace application; using namespace qualpf::gui;
using namespace application::presentation;

struct ProductPlotFrame {
    int action = 8;
    PlotUpdate update;
};

// The former data transport emitted two primary-only frames, then one frame
// per appended comparison. Prepare every value before executing any command.
Result<std::vector<ProductPlotFrame>> project_product_refresh(
    const SessionStateView &, const PresentationWorkspace &, PlotRequest = {});

// Application output port for retained IO operations whose frozen schedule
// includes a refresh. The GUI owns the binding and removes it before destruction.
// No native callback, input import, Session change or event processing is involved.
void bind_product_plot_refresh(WorkspaceContext &, QObject *owner, std::function<Status()> refresh);
void unbind_product_plot_refresh(WorkspaceContext &, const QObject *owner);
Status request_product_plot_refresh(WorkspaceContext &);

} // namespace qualpf::gui::presentation
