#pragma once
// Baseline f12870d78f407921d93f694ac5b17f216233e1f6 GUI scalar expressions.
#include "core/io/powder_cif/binary32.h"
#include <bit>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
namespace prior_plot {
enum xAbscissaType {TTHETA, DVALUE, ONE_OVER_DVALUE, ONE_OVER_DVALUE2};
double dvalue(double ttheta, double wave) {
    const double DTOR = M_PI / 180;
    return wave / (2.0 * sin(ttheta * 0.5 * DTOR));
}

double tthvalue(double d, double wave) {
    const double RTOD = 180 / M_PI;
    const auto angle = qualpf::io::powder_cif::binary32::certifiedAsin64(
        std::bit_cast<std::uint64_t>(0.5 * wave / d));
    return 2.0 * std::bit_cast<double>(angle) * RTOD;
}

double tthvalue(double d, double wave, xAbscissaType dtype) {
    switch (dtype) {
    case TTHETA: return d; //dvalue(d, wave);
    case DVALUE: return tthvalue(d, wave);
    case ONE_OVER_DVALUE: return tthvalue(1/d, wave);
    case ONE_OVER_DVALUE2: return tthvalue(sqrt(1/d), wave);
    }
    return dvalue(d, wave);
}


double fromTwoTheta(double x, double wave, xAbscissaType axis) {
    x = dvalue(x, wave);
    if (axis == ONE_OVER_DVALUE) x = 1/x;
    else if (axis == ONE_OVER_DVALUE2) x = 1/(x*x);
    return x;
}
double fromSpacing(double x, double wave, xAbscissaType axis) {
    if (axis == ONE_OVER_DVALUE) x=1/x;
    else if (axis == ONE_OVER_DVALUE2) x=sqrt(1/x);
    return tthvalue(x,wave);
}
double between(double x, xAbscissaType from, xAbscissaType to) {
    if (from == DVALUE) { x=1/x; if(to==ONE_OVER_DVALUE2) x*=x; }
    else if(from==ONE_OVER_DVALUE) { if(to==DVALUE) x=1/x; else if(to==ONE_OVER_DVALUE2) x*=x; }
    else if(from==ONE_OVER_DVALUE2) { if(to==DVALUE) x=sqrt(1/x); else if(to==ONE_OVER_DVALUE) x=sqrt(x); }
    return x;
}
} // namespace prior_plot
