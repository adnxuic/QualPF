#include "qualpf/application/state/presentation_workspace.h"

namespace qualpf::application {

void PresentationWorkspace::setVisibility(PlotLayer layer, Visibility visibility) noexcept {
    const auto index = static_cast<std::size_t>(layer);
    if (index < state_.layers.size()) state_.layers[index] = visibility;
}

void PresentationWorkspace::applyPlotAction(int action) noexcept {
    if (action < 1 || action > 8) return;
    // Frozen display commands from set_plot_style, not numerical plot kernels.
    // Layer order: background, background points, calculated, difference,
    // cumulative, peaks, intervals, smoothing. Other layer choices persist.
    constexpr int presets[8][8] = {
        { 0, 0,-1,-1,-1,0,-1,0}, {-1,-1,-1,-1,-1,0, 1,0},
        { 1, 1,-1,-1,-1,0, 1,0}, { 1, 0, 1, 1, 0,0,-1,0},
        { 1, 1,-1,-1,-1,1,-1,0}, { 1, 0, 1, 1, 0,1,-1,0},
        { 1, 0, 1, 1, 0,0,-1,0}, { 1, 0, 1, 1, 0,0,-1,0}
    };
    constexpr PlotLayer layers[] = {PlotLayer::background, PlotLayer::background_points,
        PlotLayer::calculated, PlotLayer::difference, PlotLayer::cumulative,
        PlotLayer::peaks, PlotLayer::intervals, PlotLayer::smoothing};
    for (std::size_t i = 0; i < std::size(layers); ++i)
        setVisibility(layers[i], static_cast<Visibility>(presets[action - 1][i]));
    state_.observed = state_.phase = Visibility::visible;
}

} // namespace qualpf::application
