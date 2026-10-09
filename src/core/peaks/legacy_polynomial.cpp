#include "legacy_polynomial.h"
#include "../preprocessing/legacy_reduction.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace qualpf::preprocessing::detail {
namespace {
float magnitude(float a, float b) {
    a = std::abs(a);
    b = std::abs(b);
    if (a > b) {
        float t = b / a;
        return a * std::sqrt(1.f + t * t);
    }
    if (b == 0)
        return 0;
    float t = a / b;
    return b * std::sqrt(1.f + t * t);
}
using Row = std::array<float, 3>;
Row failure() {
    Row r;
    r.fill(std::numeric_limits<float>::quiet_NaN());
    return r;
}
} // namespace
Row fit_log_quadratic(std::vector<Row> a, std::span<const float> rhs) {
    // Arithmetic follows recipes::svdcmp_sp/svbksb_sp; loops implement the
    // original array expressions using owned, dimension-limited storage.
    const int m = int(a.size()), n = 3;
    if (m < 3 || rhs.size() != a.size())
        return failure();
    Row w{}, rv{}, temp{};
    std::array<Row, 3> v{};
    std::vector<float> work(m);
    float g = 0, scale = 0;
    for (int i = 0; i < n; ++i) {
        int l = i + 1;
        rv[i] = scale * g;
        g = 0;
        scale = 0;
        for (int k = i; k < m; ++k)
            scale += std::abs(a[k][i]);
        if (scale != 0) {
            float s = 0;
            for (int k = i; k < m; ++k) {
                a[k][i] /= scale;
                s += a[k][i] * a[k][i];
            }
            float f = a[i][i];
            g = -std::copysign(std::sqrt(s), f);
            float h = f * g - s;
            a[i][i] = f - g;
            for (int j = l; j < n; ++j) {
                float dot = contiguous_matmul_dot<float>(m - i, [&](auto k) { return a[i + k][i]; },
                                                        [&](auto k) { return a[i + k][j]; });
                temp[j] = dot / h;
            }
            for (int k = i; k < m; ++k)
                for (int j = l; j < n; ++j)
                    a[k][j] += a[k][i] * temp[j];
            for (int k = i; k < m; ++k)
                a[k][i] = scale * a[k][i];
        }
        w[i] = scale * g;
        g = 0;
        scale = 0;
        if (i != n - 1) {
            for (int k = l; k < n; ++k)
                scale += std::abs(a[i][k]);
            if (scale != 0) {
                float s = 0;
                for (int k = l; k < n; ++k) {
                    a[i][k] /= scale;
                    s += a[i][k] * a[i][k];
                }
                float f = a[i][l];
                g = -std::copysign(std::sqrt(s), f);
                float h = f * g - s;
                a[i][l] = f - g;
                for (int j = l; j < n; ++j)
                    rv[j] = a[i][j] / h;
                for (int k = l; k < m; ++k) {
                    float dot = 0;
                    for (int j = l; j < n; ++j)
                        dot = reference_fma(a[k][j], a[i][j], dot);
                    work[k] = dot;
                }
                for (int k = l; k < m; ++k)
                    for (int j = l; j < n; ++j)
                        a[k][j] += work[k] * rv[j];
                for (int j = l; j < n; ++j)
                    a[i][j] = scale * a[i][j];
            }
        }
    }
    float norm = 0;
    for (int i = 0; i < n; ++i)
        norm = std::max(norm, std::abs(w[i]) + std::abs(rv[i]));
    for (int i = n - 1; i >= 0; --i) {
        int l = i + 1;
        if (i < n - 1) {
            if (g != 0) {
                for (int j = l; j < n; ++j)
                    v[j][i] = (a[i][j] / a[i][l]) / g;
                for (int j = l; j < n; ++j) {
                    float dot = 0;
                    for (int k = l; k < n; ++k)
                        dot = reference_fma(a[i][k], v[k][j], dot);
                    temp[j] = dot;
                }
                for (int k = l; k < n; ++k)
                    for (int j = l; j < n; ++j)
                        v[k][j] += v[k][i] * temp[j];
            }
            for (int j = l; j < n; ++j) {
                v[i][j] = 0;
                v[j][i] = 0;
            }
        }
        v[i][i] = 1;
        g = rv[i];
    }
    for (int i = n - 1; i >= 0; --i) {
        int l = i + 1;
        g = w[i];
        for (int j = l; j < n; ++j)
            a[i][j] = 0;
        if (g != 0) {
            g = 1.f / g;
            for (int j = l; j < n; ++j) {
                float dot = contiguous_matmul_dot<float>(m - l, [&](auto k) { return a[l + k][i]; },
                                                        [&](auto k) { return a[l + k][j]; });
                temp[j] = (dot / a[i][i]) * g;
            }
            for (int k = i; k < m; ++k)
                for (int j = l; j < n; ++j)
                    a[k][j] += a[k][i] * temp[j];
            for (int k = i; k < m; ++k)
                a[k][i] *= g;
        } else
            for (int k = i; k < m; ++k)
                a[k][i] = 0;
        a[i][i] += 1.f;
    }
    for (int k = n - 1; k >= 0; --k) {
        bool converged = false;
        for (int iteration = 0; iteration < 30; ++iteration) {
            int l = k;
            for (; l >= 0; --l) {
                int previous = l - 1;
                if (std::abs(rv[l]) + norm == norm)
                    break;
                if (previous < 0)
                    return failure();
                if (std::abs(w[previous]) + norm == norm) {
                    float c = 0, s = 1;
                    for (int i = l; i <= k; ++i) {
                        float f = s * rv[i];
                        rv[i] = c * rv[i];
                        if (std::abs(f) + norm == norm)
                            break;
                        g = w[i];
                        float h = magnitude(f, g);
                        w[i] = h;
                        h = 1.f / h;
                        c = g * h;
                        s = -(f * h);
                        for (int row = 0; row < m; ++row) {
                            float old = a[row][previous];
                            a[row][previous] = a[row][previous] * c + a[row][i] * s;
                            a[row][i] = -old * s + a[row][i] * c;
                        }
                    }
                    break;
                }
            }
            float z = w[k];
            if (l == k) {
                if (z < 0) {
                    w[k] = -z;
                    for (int row = 0; row < n; ++row)
                        v[row][k] = -v[row][k];
                }
                converged = true;
                break;
            }
            if (iteration == 29 || l < 0)
                return failure();
            float x = w[l], y = w[k - 1];
            g = rv[k - 1];
            float h = rv[k];
            float f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.f * h * y);
            g = magnitude(f, 1.f);
            f = ((x - z) * (x + z) + h * ((y / (f + std::copysign(g, f))) - h)) / x;
            float c = 1, s = 1;
            for (int j = l; j < k; ++j) {
                int i = j + 1;
                g = rv[i];
                y = w[i];
                h = s * g;
                g = c * g;
                z = magnitude(f, h);
                rv[j] = z;
                c = f / z;
                s = h / z;
                f = x * c + g * s;
                g = -(x * s) + g * c;
                h = y * s;
                y *= c;
                for (int row = 0; row < n; ++row) {
                    float old = v[row][j];
                    v[row][j] = v[row][j] * c + v[row][i] * s;
                    v[row][i] = -old * s + v[row][i] * c;
                }
                z = magnitude(f, h);
                w[j] = z;
                if (z != 0) {
                    z = 1.f / z;
                    c = f * z;
                    s = h * z;
                }
                f = c * g + s * y;
                x = -(s * g) + c * y;
                for (int row = 0; row < m; ++row) {
                    float old = a[row][j];
                    a[row][j] = a[row][j] * c + a[row][i] * s;
                    a[row][i] = -old * s + a[row][i] * c;
                }
            }
            rv[l] = 0;
            rv[k] = f;
            w[k] = x;
        }
        if (!converged)
            return failure();
    }
    float cutoff = 1.e-5f * (*std::max_element(w.begin(), w.end()));
    for (int j = 0; j < n; ++j) {
        float dot = contiguous_matmul_dot<float>(m, [&](auto k) { return rhs[k]; },
                                                [&](auto k) { return a[k][j]; });
        temp[j] = w[j] < cutoff || w[j] == 0 ? 0 : dot / w[j];
    }
    Row result{};
    for (int i = 0; i < n; ++i)
        for (int j = 0; j < n; ++j)
            result[i] = reference_fma(v[i][j], temp[j], result[i]);
    return result;
}
} // namespace qualpf::preprocessing::detail
