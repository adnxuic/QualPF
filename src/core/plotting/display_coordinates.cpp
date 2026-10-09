#include "qualpf/numerics/display_coordinates.h"
#include "binary32.h"
#include <bit>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace qualpf::numerics::display {
// Relocated GUI expressions. Keep operation order and the former default FP
// compilation mode; certifiedAsin64 retains its independently approved options.
double dvalue(double ttheta, double wave) {
    const double DTOR = M_PI / 180;
    return wave / (2.0 * sin(ttheta * 0.5 * DTOR));
}
double tthvalue(double d, double wave) {
    const double RTOD = 180 / M_PI;
    const auto angle = io::powder_cif::binary32::certifiedAsin64(std::bit_cast<std::uint64_t>(0.5 * wave / d));
    return 2.0 * std::bit_cast<double>(angle) * RTOD;
}
double tthvalue(double x, double wave, Axis axis) {
    switch (axis) {
    case Axis::two_theta: return x;
    case Axis::d_spacing: return tthvalue(x, wave);
    case Axis::inverse_d: return tthvalue(1 / x, wave);
    case Axis::inverse_d2: return tthvalue(sqrt(1 / x), wave);
    }
    return dvalue(x, wave);
}
double spacing(double x, Axis axis) {
    return axis == Axis::d_spacing ? x : axis == Axis::inverse_d ? 1 / x : std::sqrt(1 / x);
}
double fromTwoTheta(double x, double wave, Axis axis) {
    x = dvalue(x, wave);
    if (axis == Axis::inverse_d) x = 1 / x;
    else if (axis == Axis::inverse_d2) x = 1 / (x * x);
    return x;
}
double fromSpacing(double x, double wave, Axis axis) {
    if (axis == Axis::inverse_d) x = 1 / x;
    else if (axis == Axis::inverse_d2) x = sqrt(1 / x);
    return tthvalue(x, wave);
}
double betweenSpacings(double x, Axis from, Axis to) {
    if (from == Axis::d_spacing) {
        x = 1 / x;
        if (to == Axis::inverse_d2) x *= x;
    } else if (from == Axis::inverse_d) {
        if (to == Axis::d_spacing) x = 1 / x;
        else if (to == Axis::inverse_d2) x *= x;
    } else if (from == Axis::inverse_d2) {
        if (to == Axis::d_spacing) x = sqrt(1 / x);
        else if (to == Axis::inverse_d) x = sqrt(x);
    }
    return x;
}
} // namespace qualpf::numerics::display
