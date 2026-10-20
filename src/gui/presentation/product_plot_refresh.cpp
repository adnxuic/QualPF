#include "gui/presentation/product_plot_refresh.h"

namespace qualpf::gui::presentation {
using namespace application; using namespace qualpf::gui;
using namespace application::presentation;
void bind_product_plot_refresh(WorkspaceContext &context, QObject *owner, std::function<Status()> refresh) { context.bindPresenter(owner, std::move(refresh)); }
void unbind_product_plot_refresh(WorkspaceContext &context, const QObject *owner) { context.unbindPresenter(owner); }
Status request_product_plot_refresh(WorkspaceContext &context) { return context.refreshPresentation(); }
Result<std::vector<ProductPlotFrame>> project_product_refresh(
    const SessionStateView &state, const PresentationWorkspace &workspace, PlotRequest request) {
    std::vector<ProductPlotFrame> frames;
    if (!workspace.redrawEnabled() || state.pattern.two_theta.empty())
        return Result<std::vector<ProductPlotFrame>>{std::move(frames)};
    auto input = make_plot_input(state, workspace, 0);
    auto patterns = std::move(input.patterns);
    // set_diffraction_data never transported a calculated or smoothing buffer.
    // Its background argument contained only a complete, aligned background.
    // Partial backgrounds remain in the owning session and its double refresh.
    for (auto &p : patterns) {
        p.pattern.calculated.reset();
        p.smoothing_preview.reset();
        if (p.pattern.background && (!p.pattern.background->window ||
            p.pattern.background->window->first != 0 ||
            p.pattern.background->values.size() != p.pattern.observed.size()))
            p.pattern.background.reset();
    }
    input.patterns.push_back(patterns.front());
    input.patterns.front().background_points_x.clear();
    input.patterns.front().background_points_y.clear();
    auto presentation = workspace;
    auto append = [&](int action, int rescale) -> Status {
        presentation.applyPlotAction(action);
        input.presentation = presentation.snapshot();
        input.presentation.layers[std::size_t(PlotLayer::background)] = Visibility::hidden;
        input.presentation.layers[std::size_t(PlotLayer::background_points)] = Visibility::visible;
        auto projected = project_compatibility_frame(input, {rescale, request.abscissa});
        if (!projected) return Status{projected.error()};
        frames.push_back({action, std::move(projected).value()});
        return Status{std::monostate{}};
    };
    auto status = append(8, request.rescale);
    input.patterns.front().background_points_x = patterns.front().background_points_x;
    input.patterns.front().background_points_y = patterns.front().background_points_y;
    if (status) status = append(5, 0);
    if (!status) return Result<std::vector<ProductPlotFrame>>{status.error()};
    input.patterns.front().background_points_x = patterns.front().background_points_x;
    input.patterns.front().background_points_y = patterns.front().background_points_y;
    for (std::size_t i = 1; i < patterns.size(); ++i) {
        // Comparison transport supplied observed data and radiation only.
        patterns[i].pattern.background.reset();
        input.patterns.push_back(std::move(patterns[i]));
        status = append(8, request.rescale);
        if (!status) return Result<std::vector<ProductPlotFrame>>{status.error()};
    }
    return Result<std::vector<ProductPlotFrame>>{std::move(frames)};
}
} // namespace qualpf::gui::presentation
