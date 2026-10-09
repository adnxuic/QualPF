#pragma once
#include "gui/plotting/xpdutils.h"
#include "qualpf/application/plot_coordinates.h"
namespace qualpf::gui {
inline Result<double> peakCanonicalCoordinate(double x, xpdutils::xAbscissaType axis,
    std::optional<WavelengthAngstrom> wave) {
    return application::peakCanonicalCoordinate(x, static_cast<application::PlotAxis>(axis), wave);
}
} // namespace qualpf::gui
