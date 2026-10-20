#include "pfsm_simd.h"
#include <algorithm>
#include <cmath>
#include <immintrin.h>

namespace qualpf::profile::detail {
SimdResult ilAvx2(std::span<const double> x, std::span<const ProfileLine> lines, double shift,
                  double factor, std::span<double> out, std::stop_token stop) {
    if (x.size() % 4) return SimdResult::fallback;
    const auto one = _mm256_set1_pd(1.), factors = _mm256_set1_pd(factor), delta = _mm256_set1_pd(shift);
    for (const auto &line : lines) {
        const auto position = _mm256_set1_pd(line.position), strength = _mm256_set1_pd(line.intensity);
        std::size_t i = 0;
        for (; i + 3 < x.size(); i += 4) {
            if ((i & 255) == 0 && stop.stop_requested()) return SimdResult::cancelled;
            const auto v = _mm256_mul_pd(
                _mm256_sub_pd(_mm256_sub_pd(_mm256_loadu_pd(x.data() + i), position), delta), factors);
            const auto inverse = _mm256_div_pd(one, _mm256_sqrt_pd(_mm256_add_pd(one, _mm256_mul_pd(v, v))));
            const auto kernel = _mm256_mul_pd(_mm256_mul_pd(inverse, inverse), inverse);
            _mm256_storeu_pd(out.data() + i,
                _mm256_add_pd(_mm256_loadu_pd(out.data() + i), _mm256_mul_pd(strength, kernel)));
        }
    }
    return SimdResult::complete;
}
SimdResult cachedIlAvx2(std::span<const double> x, double lower, double step, double shift,
                        std::span<const IlCoefficients> coefficients, std::span<double> out,
                        std::stop_token stop) {
    const auto cellAt = [&](double coordinate) {
        const double location = (coordinate - lower) - shift;
        const auto index = std::min(coefficients.size() - 1, static_cast<std::size_t>(location / step));
        const double t = 2 * (location - double(index) * step) / step - 1;
        return std::pair{index, t};
    };
    std::size_t i = 0;
    for (; i + 3 < x.size(); i += 4) {
        if ((i & 255) == 0 && stop.stop_requested()) return SimdResult::cancelled;
        const auto c0 = cellAt(x[i]), c1 = cellAt(x[i + 1]), c2 = cellAt(x[i + 2]), c3 = cellAt(x[i + 3]);
        const auto &p0 = coefficients[c0.first], &p1 = coefficients[c1.first],
                   &p2 = coefficients[c2.first], &p3 = coefficients[c3.first];
        const auto t = _mm256_set_pd(c3.second, c2.second, c1.second, c0.second);
        const auto twice = _mm256_mul_pd(_mm256_set1_pd(2.), t);
        auto a = _mm256_setzero_pd(), b = _mm256_setzero_pd();
        for (std::size_t k = 31; k > 0; --k) {
            const auto next = _mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(twice, a), b),
                                           _mm256_set_pd(p3[k], p2[k], p1[k], p0[k]));
            b = a;
            a = next;
        }
        _mm256_storeu_pd(out.data() + i, _mm256_add_pd(_mm256_sub_pd(_mm256_mul_pd(t, a), b),
                                                     _mm256_set_pd(p3[0], p2[0], p1[0], p0[0])));
        for (std::size_t j = 0; j < 4; ++j)
            if (!std::isfinite(out[i + j]) || out[i + j] < 0) return SimdResult::fallback;
    }
    for (; i < x.size(); ++i) {
        if ((i & 255) == 0 && stop.stop_requested()) return SimdResult::cancelled;
        const auto [index, t] = cellAt(x[i]);
        const auto &c = coefficients[index];
        double a = 0, b = 0;
        for (std::size_t k = 31; k > 0; --k) {
            const double next = (2 * t * a - b) + c[k];
            b = a;
            a = next;
        }
        out[i] = (t * a - b) + c[0];
        if (!std::isfinite(out[i]) || out[i] < 0) return SimdResult::fallback;
    }
    return SimdResult::complete;
}
} // namespace qualpf::profile::detail
