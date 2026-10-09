#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace qualpf::preprocessing::detail {
// Float products are exact in double. Keep the addition's residual as well,
// so a double-rounding tie is resolved in the direction of the exact sum.
// MinGW 13.1's software fmaf does not reliably give single-rounding results.
inline float reference_fma(float a, float b, float c) {
    const double product = double(a) * double(b);
    const double sum = product + double(c);
    const double z = sum - product;
    const double error = (product - (sum - z)) + (double(c) - z);
    float rounded = float(sum);
    if (error != 0 && std::isfinite(rounded)) {
        const float neighbor = std::nextafter(rounded, error > 0 ? INFINITY : -INFINITY);
        const double midpoint = (double(rounded) + double(neighbor)) * .5;
        if (sum == midpoint)
            rounded = neighbor;
    }
    return rounded;
}
inline double reference_fma(double a, double b, double c) {
    // These callers multiply widened binary32 basis/observations: their
    // products are exact in binary64, so a separate multiply is sufficient.
    return a * b + c;
}
// Error-free product/sum expansions for the normal, bounded binary64 values
// in Cholesky. Using std::fma here would inherit MinGW's software fma rounding
// defect. No change to the normal equations, pivots or solve is intended.
inline double cholesky_multiply_add(double a, double b, double c) {
    const double product = a * b;
    constexpr double splitter = 134217729.; // 2^27 + 1 (Dekker split).
    const double as = splitter * a, bs = splitter * b;
    const double ah = as - (as - a), bh = bs - (bs - b);
    const double al = a - ah, bl = b - bh;
    const double product_error = ((ah * bh - product) + ah * bl + al * bh) + al * bl;
    auto two_sum = [](double x, double y) {
        const double sum = x + y, z = sum - x;
        return std::array<double, 2>{sum, (x - (sum - z)) + (y - z)};
    };
    const auto sum = two_sum(product, c);
    const auto low = two_sum(sum[1], product_error);
    const auto high = two_sum(sum[0], low[0]);
    const auto tail = two_sum(high[1], low[1]);
    const double neighbor = std::nextafter(high[0], tail[0] > 0 ? INFINITY : -INFINITY);
    if (tail[1] != 0 && std::abs(tail[0]) == .5 * std::abs(neighbor - high[0]))
        return (tail[0] > 0) == (tail[1] > 0) ? neighbor : high[0];
    return high[0] + tail[0];
}
// Scalar emulation of the GNU 13.1 reference runtime's contiguous MATMUL
// reduction. The oracle's -O0 does not disable reduction/FMA inside libgfortran.
// These independent accumulators specify rounding order, not SIMD or a faster
// kernel. DOT_PRODUCT/SUM and strided MATMUL have different arithmetic contracts.
template <class T, class Left, class Right>
T contiguous_matmul_dot(std::size_t count, Left left, Right right) {
    constexpr std::size_t lanes = std::is_same_v<T, float> ? 8 : 4;
    std::array<T, lanes> sums{};
    std::size_t at = 0;
    for (; at + lanes <= count; at += lanes)
        for (std::size_t j = 0; j < lanes; ++j)
            sums[j] = reference_fma(left(at + j), right(at + j), sums[j]);
    for (std::size_t j = 0; j < lanes / 2; ++j)
        sums[j] += sums[j + lanes / 2];
    if (at + lanes / 2 <= count) {
        for (std::size_t j = 0; j < lanes / 2; ++j)
            sums[j] = reference_fma(left(at + j), right(at + j), sums[j]);
        at += lanes / 2;
    }
    T result;
    if constexpr (lanes == 8)
        result = (sums[0] + sums[2]) + (sums[1] + sums[3]);
    else
        result = sums[0] + sums[1];
    for (; at < count; ++at)
        result = reference_fma(left(at), right(at), result);
    return result;
}
} // namespace qualpf::preprocessing::detail
