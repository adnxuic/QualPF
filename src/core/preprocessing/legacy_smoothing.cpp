#include "legacy_internal.h"

namespace qualpf::preprocessing {
namespace detail {
Vector average(const Vector &y, int w) {
    const int n = int(y.size());
    if (n < 2 * w + 1)
        return y;
    Vector out = y;
    const float factor = 1.f / (2.f * w + 1);
    for (int i = 1; i < w; ++i)
        out[i] = sum(std::span(y).first(2 * i + 1)) / float(2 * i + 1);
    for (int i = w; i < n - w; ++i)
        out[i] = factor * sum(std::span(y).subspan(i - w, 2 * w + 1));
    for (int i = n - w; i < n - 1; ++i)
        out[i] = sum(std::span(y).subspan(2 * i - n + 1)) / float(2 * n - 2 * i - 1);
    return out;
}

// Small scaled-pivot LU preserves the legacy SG normal-equation evaluation.
// Matrix, permutation and RHS are owned by this invocation.
Result<Vector> coefficients(int left, int right, int derivative, int degree) {
    const int n = degree + 1;
    Vector matrix(n * n), scale(n), rhs(n);
    std::vector<int> pivot(n);
    for (int p = 0; p <= 2 * degree; ++p) {
        float positive = 0, negative = 0;
        for (int k = 1; k <= right; ++k)
            positive += power(float(k), p);
        for (int k = 1; k <= left; ++k)
            negative += power(float(-k), p);
        float s = positive + negative;
        if (p == 0)
            s += 1;
        for (int row = 0; row < n; ++row) {
            int col = p - row;
            if (col >= 0 && col < n)
                matrix[row * n + col] = s;
        }
    }
    for (int row = 0; row < n; ++row) {
        float m = 0;
        for (int col = 0; col < n; ++col)
            m = std::max(m, std::abs(matrix[row * n + col]));
        if (!std::isfinite(m) || m == 0)
            return Result<Vector>{numerical("smoothing.sg", "Singular or overflowing SG matrix")};
        scale[row] = float(1.0 / double(m));
    }
    for (int j = 0; j < n; ++j) {
        int best = j;
        float largest = -1;
        for (int row = j; row < n; ++row) {
            float v = scale[row] * std::abs(matrix[row * n + j]);
            if (v > largest) {
                largest = v;
                best = row;
            }
        }
        if (best != j) {
            for (int k = 0; k < n; ++k)
                std::swap(matrix[best * n + k], matrix[j * n + k]);
            scale[best] = scale[j];
        }
        pivot[j] = best;
        if (matrix[j * n + j] == 0)
            matrix[j * n + j] = 1.e-20f;
        for (int row = j + 1; row < n; ++row)
            matrix[row * n + j] /= matrix[j * n + j];
        for (int row = j + 1; row < n; ++row)
            for (int k = j + 1; k < n; ++k)
                matrix[row * n + k] -= matrix[row * n + j] * matrix[j * n + k];
    }
    rhs[derivative] = 1;
    int first = -1;
    for (int i = 0; i < n; ++i) {
        float value = rhs[pivot[i]];
        rhs[pivot[i]] = rhs[i];
        if (first >= 0) {
            float dot = 0;
            for (int j = first; j < i; ++j)
                dot += matrix[i * n + j] * rhs[j];
            value -= dot;
        } else if (value != 0)
            first = i;
        rhs[i] = value;
    }
    for (int i = n - 1; i >= 0; --i) {
        float dot = 0;
        for (int j = i + 1; j < n; ++j)
            dot += matrix[i * n + j] * rhs[j];
        rhs[i] = (rhs[i] - dot) / matrix[i * n + i];
    }
    Vector c;
    for (int k = -left; k <= right; ++k) {
        float v;
        // nrutil::poly_rrv chooses pairwise polynomial evaluation when the
        // number of coefficients fills the window and reaches NPAR_POLY=8.
        // Horner at this boundary changes near-zero third-derivative signs.
        if (n >= left + right + 1 && n >= 8) {
            Vector terms = rhs;
            float power = float(k);
            while (terms.size() > 1) {
                if (terms.size() % 2)
                    terms.push_back(0.f);
                Vector next;
                for (std::size_t j = 0; j < terms.size(); j += 2)
                    next.push_back(terms[j] + power * terms[j + 1]);
                terms = std::move(next);
                power *= power;
            }
            v = terms.front();
        } else {
            v = rhs.back();
            for (int j = n - 2; j >= 0; --j)
                v = float(k) * v + rhs[j];
        }
        if (!std::isfinite(v))
            return Result<Vector>{numerical("smoothing.sg", "Nonfinite SG coefficient")};
        c.push_back(v);
    }
    return Result<Vector>{std::move(c)};
}
Result<Vector> sg(const Vector &y, int w, int degree, int derivative) {
    const int n = int(y.size());
    if (w < 1 || w > 100 || degree < 0 || degree > 2 * w || degree > 20 || derivative < 0 ||
        derivative > degree || n < 2 * w + 1)
        return Result<Vector>{invalid("smoothing.sg", "Unsupported or unsafe SG window/order")};
    auto full = coefficients(w, w, derivative, degree);
    if (!full)
        return Result<Vector>{full.error()};
    Vector out(n);
    for (int i = 0; i < n; ++i) {
        int left = w, right = w;
        const Vector *c = &full.value();
        Vector edge;
        if (derivative == 0 && (i < w || i >= n - w)) {
            if (i == 0 || i == n - 1) {
                out[i] = y[i];
                continue;
            }
            left = std::min(w, i);
            right = std::min(w, n - i - 1);
            if (left + right < degree) {
                out[i] = y[i];
                continue;
            }
            auto v = coefficients(left, right, derivative, degree);
            if (!v)
                return Result<Vector>{v.error()};
            edge = std::move(v).value();
            c = &edge;
        }
        float s = 0;
        if (derivative != 0 && i < w) {
            float padding = 0;
            for (int k = -w; k < -i; ++k)
                padding += (*c)[k + w];
            for (int k = -i; k <= w; ++k)
                s += (*c)[k + w] * y[i + k];
            s = y.front() * padding + s;
        } else if (derivative != 0 && i >= n - w) {
            float padding = 0;
            for (int k = n - i; k <= w; ++k)
                padding += (*c)[k + w];
            for (int k = -w; k < n - i; ++k)
                s += (*c)[k + w] * y[i + k];
            s = y.back() * padding + s;
        } else {
            for (int k = -left; k <= right; ++k)
                s += (*c)[k + left] * y[i + k];
        }
        if (!std::isfinite(s))
            return Result<Vector>{numerical("smoothing", "Nonfinite smoothed output")};
        out[i] = s;
    }
    return Result<Vector>{std::move(out)};
}
} // namespace detail
Result<std::vector<double>> smooth(std::span<const double> values, const SmoothingOptions &o) {
    if (values.empty() || values.size() > std::size_t(std::numeric_limits<int>::max()) || o.half_window < 0 ||
        o.half_window > 100000)
        return Result<std::vector<double>>{detail::invalid("smoothing", "Invalid input length/window")};
    auto y = detail::narrow(values);
    if (!y)
        return Result<std::vector<double>>{y.error()};
    if (o.method == SmoothingMethod::averaging) {
        if (o.derivative != 0)
            return Result<std::vector<double>>{
                detail::invalid("smoothing.derivative", "Averaging has no derivative")};
        auto out = detail::average(y.value(), o.half_window);
        if (!std::all_of(out.begin(), out.end(), [](float v) { return std::isfinite(v); }))
            return Result<std::vector<double>>{detail::numerical("smoothing", "Averaging overflow")};
        return Result<std::vector<double>>{detail::widen(out)};
    }
    if (o.method != SmoothingMethod::savitzky_golay)
        return Result<std::vector<double>>{detail::invalid("smoothing.method", "Unknown method")};
    auto out = detail::sg(y.value(), o.half_window, o.polynomial_degree, o.derivative);
    if (!out)
        return Result<std::vector<double>>{out.error()};
    return Result<std::vector<double>>{detail::widen(out.value())};
}
} // namespace qualpf::preprocessing
