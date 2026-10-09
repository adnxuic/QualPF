#pragma once
namespace qualpf::numerics::display {
enum class Axis { two_theta, d_spacing, inverse_d, inverse_d2 };
double dvalue(double twoTheta, double wavelength);
double tthvalue(double d, double wavelength);
double tthvalue(double x, double wavelength, Axis);
double spacing(double x, Axis);
double fromTwoTheta(double x, double wavelength, Axis);
double fromSpacing(double x, double wavelength, Axis);
double betweenSpacings(double x, Axis from, Axis to);
} // namespace qualpf::numerics::display
