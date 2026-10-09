#pragma once
#include "qualpf/application/product_session.h"
#include "qualpf/application/presentation/plot_snapshot.h"
namespace qualpf::application {
using PlotAxis = presentation::Abscissa;
double plotDSpacing(double twoTheta, double wavelength);
double plotTwoThetaFromD(double d, double wavelength);
double plotTwoTheta(double x, double wavelength, PlotAxis);
double plotFromTwoTheta(double x, double wavelength, PlotAxis);
double plotFromSpacing(double x, double wavelength, PlotAxis);
double plotBetweenSpacings(double x, PlotAxis from, PlotAxis to);
Result<double> peakCanonicalCoordinate(double, PlotAxis, std::optional<WavelengthAngstrom>);
Result<double> backgroundCanonicalCoordinate(double, PlotAxis, std::optional<WavelengthAngstrom>);
} // namespace qualpf::application
