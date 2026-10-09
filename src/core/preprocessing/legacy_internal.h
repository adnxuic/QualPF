#pragma once
#include "qualpf/preprocessing/legacy.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qualpf::preprocessing::detail {
using Vector = std::vector<float>;
inline Error invalid(const char *field, const char *message) {
    return {ErrorCode::invalid_input, field, message};
}
inline Error numerical(const char *field, const char *message) {
    return {ErrorCode::numerical_failure, field, message, "undefined_or_nonfinite_result"};
}
inline float sum(std::span<const float> v) {
    float s = 0;
    for (float x : v)
        s += x;
    return s;
}
inline float power(float x, int p) {
    float result = 1;
    while (p) {
        if (p & 1)
            result *= x;
        p >>= 1;
        if (p)
            x *= x;
    }
    return result;
}
inline std::size_t nearest(std::span<const float> x, float value) {
    if (value <= x.front())
        return 0;
    if (value >= x.back())
        return x.size() - 1;
    auto upper = std::lower_bound(x.begin(), x.end(), value) - x.begin();
    return value - x[upper - 1] < x[upper] - value ? upper - 1 : upper;
}
inline Result<Vector> narrow(std::span<const double> values) {
    Vector out;
    out.reserve(values.size());
    for (double v : values) {
        if (!std::isfinite(v) || std::abs(v) > std::numeric_limits<float>::max())
            return Result<Vector>{invalid("pattern", "Input is not representable as finite binary32")};
        out.push_back(float(v));
    }
    return Result<Vector>{std::move(out)};
}
inline Result<Vector> grid(const Pattern &p) {
    if (p.two_theta.size() != p.observed.size() || p.observed.size() < 2)
        return Result<Vector>{invalid("pattern", "At least two aligned samples are required")};
    Vector x;
    for (auto t : p.two_theta) {
        if (!std::isfinite(t.value) || std::abs(t.value) > std::numeric_limits<float>::max() ||
            (!x.empty() && float(t.value) <= x.back()))
            return Result<Vector>{
                invalid("pattern.grid", "Grid must remain strictly increasing in binary32")};
        x.push_back(float(t.value));
    }
    return Result<Vector>{std::move(x)};
}
inline std::vector<double> widen(const Vector &v) { return {v.begin(), v.end()}; }
Vector average(const Vector &, int);
Result<Vector> sg(const Vector &, int, int, int);
} // namespace qualpf::preprocessing::detail
