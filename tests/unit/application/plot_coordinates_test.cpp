#include "qualpf/application/plot_coordinates.h"
#include "tests/support/plot_coordinate_reference.h"
#include <iostream>
#include <limits>
#include <stdexcept>
void same(double actual, double expected) {
    if (std::isnan(expected) ? !std::isnan(actual) : std::bit_cast<std::uint64_t>(actual) != std::bit_cast<std::uint64_t>(expected))
        throw std::runtime_error("Relocated plot expression differs from the baseline");
}
int main() {
    using namespace qualpf::application;
    try {
        const double values[]{-1, 0, 1e-9, .25, .770296535, 1, 6, 25, 90, 179.999, 180,
                              std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()};
        for (const auto wave : {1.54059307, .71073}) for (const auto x : values) {
            same(plotDSpacing(x, wave), prior_plot::dvalue(x, wave));
            same(plotTwoThetaFromD(x, wave), prior_plot::tthvalue(x, wave));
            for (int from = 0; from < 5; ++from) {
                const auto axis = static_cast<PlotAxis>(from);
                const auto old = static_cast<prior_plot::xAbscissaType>(from);
                same(plotTwoTheta(x, wave, axis), prior_plot::tthvalue(x, wave, old));
                same(plotFromTwoTheta(x, wave, axis), prior_plot::fromTwoTheta(x, wave, old));
                same(plotFromSpacing(x, wave, axis), prior_plot::fromSpacing(x, wave, old));
                for (int to = 0; to < 5; ++to)
                    same(plotBetweenSpacings(x, axis, static_cast<PlotAxis>(to)), prior_plot::between(x, old, static_cast<prior_plot::xAbscissaType>(to)));
            }
        }
        if (peakCanonicalCoordinate(180, PlotAxis::two_theta, {}) ||
            !backgroundCanonicalCoordinate(180, PlotAxis::two_theta, {}) ||
            peakCanonicalCoordinate(1, PlotAxis::d_spacing, {}).error().reason != "missing_wavelength" ||
            backgroundCanonicalCoordinate(-1, PlotAxis::inverse_d, qualpf::WavelengthAngstrom{1}).error().reason != "nonpositive_display_coordinate")
            throw std::runtime_error("Coordinate admission contracts");
        std::cout << "PASS: baseline plot expression bits, axis mapping and gesture admission\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
