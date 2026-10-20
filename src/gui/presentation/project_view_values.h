#pragma once
#include <QColor>
#include "qualpf/application/state/presentation_workspace.h"
#include <string>
#include <cstdint>
namespace qualpf::gui {
inline constexpr std::pair<application::PlotLayer, const char *> projectLayers[]{
    {application::PlotLayer::background, "background"},
    {application::PlotLayer::calculated, "calculated"},
    {application::PlotLayer::difference, "difference"},
    {application::PlotLayer::cumulative, "cumulative"},
    {application::PlotLayer::peaks, "peaks"},
    {application::PlotLayer::axis, "axis"},
    {application::PlotLayer::background_points, "background_points"},
    {application::PlotLayer::intervals, "intervals"},
    {application::PlotLayer::canvas, "canvas"},
    {application::PlotLayer::legend, "legend"},
    {application::PlotLayer::unindexed_peaks, "unindexed_peaks"},
    {application::PlotLayer::smoothing, "smoothing"},
    {application::PlotLayer::absences, "absences"}};
inline std::string projectVisibility(application::Visibility value) {
    switch (value) {
    case application::Visibility::disabled:
        return "disabled";
    case application::Visibility::hidden:
        return "hidden";
    case application::Visibility::visible:
        return "visible";
    }
    return "disabled";
}
inline application::Visibility projectVisibility(const std::string &value) {
    if (value == "visible")
        return application::Visibility::visible;
    if (value == "hidden")
        return application::Visibility::hidden;
    return application::Visibility::disabled;
}
inline std::uint32_t projectRgba(const QColor &c) {
    return (std::uint32_t(c.red()) << 24) | (std::uint32_t(c.green()) << 16) |
           (std::uint32_t(c.blue()) << 8) | std::uint32_t(c.alpha());
}
inline QColor projectColor(std::uint32_t c) {
    return QColor(int(c >> 24), int((c >> 16) & 255), int((c >> 8) & 255), int(c & 255));
}
} // namespace qualpf::gui
