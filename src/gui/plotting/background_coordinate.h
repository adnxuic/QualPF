#pragma once
#include "gui/plotting/xpdutils.h"
#include "qualpf/application/plot_coordinates.h"
namespace qualpf::gui {
inline Result<double> backgroundCanonicalCoordinate(double x, xpdutils::xAbscissaType axis,
    std::optional<WavelengthAngstrom> wave) {
    return application::backgroundCanonicalCoordinate(x, static_cast<application::PlotAxis>(axis), wave);
}
} // namespace qualpf::gui
