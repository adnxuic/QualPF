#include "qualpf/numerics/linear_algebra.h"

#include <Eigen/Core>
#include <Eigen/LU>
#include <Eigen/QR>
#include <Eigen/SVD>

#include <algorithm>
#include <cmath>
#include <limits>

namespace qualpf::numerics {
namespace {
using Dense = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>;
using Vector = Eigen::VectorXd;
using Map = Eigen::Map<const Dense>;

Error invalid(const char *field, const char *message) { return {ErrorCode::invalid_input, field, message}; }
Error failure() {
    return {ErrorCode::numerical_failure, "linear_algebra", "Non-finite result or decomposition failure"};
}
Error deficient() {
    return {ErrorCode::rank_deficient, "matrix", "Matrix rank is insufficient at the requested tolerance"};
}
Status validate(MatrixView a) {
    const auto limit = static_cast<std::size_t>(std::numeric_limits<Eigen::Index>::max());
    if (a.rows == 0 || a.columns == 0 || a.rows > limit || a.columns > limit / a.rows ||
        a.values.size() != a.rows * a.columns)
        return Status{
            invalid("matrix", "Require nonzero dimensions and exactly rows*columns values within index range")};
    if (!std::all_of(a.values.begin(), a.values.end(), [](double x) { return std::isfinite(x); }))
        return Status{invalid("matrix", "Matrix entries must be finite")};
    return Status{std::monostate{}};
}
Status validate_rhs(MatrixView a, std::span<const double> b) {
    auto status = validate(a);
    if (!status)
        return status;
    if (b.size() != a.rows || !std::all_of(b.begin(), b.end(), [](double x) { return std::isfinite(x); }))
        return Status{invalid("rhs", "Require one finite RHS value per row")};
    return Status{std::monostate{}};
}
Result<double> threshold(MatrixView a, RankTolerance tolerance) {
    const double value = tolerance.relative.value_or(std::numeric_limits<double>::epsilon() *
                                                     static_cast<double>(std::max(a.rows, a.columns)));
    if (!std::isfinite(value) || value < 0 || value >= 1)
        return Result<double>{invalid("rank_tolerance", "Require 0 <= relative tolerance < 1")};
    return Result<double>{value};
}
Map map(MatrixView a) {
    return Map(a.values.data(), static_cast<Eigen::Index>(a.rows), static_cast<Eigen::Index>(a.columns));
}
Matrix owned(const Dense &a) {
    return {static_cast<std::size_t>(a.rows()), static_cast<std::size_t>(a.cols()),
            std::vector<double>(a.data(), a.data() + a.size())};
}
double scale_of(const Dense &a) {
    const double scale = a.cwiseAbs().maxCoeff();
    return scale == 0 ? 1 : scale;
}
Result<LinearSolution> solution(MatrixView a, std::span<const double> b, const Vector &x, std::size_t rank,
                                std::span<const double> sigma = {}) {
    if (!x.allFinite())
        return Result<LinearSolution>{failure()};
    Vector residual = map(a) * x - Eigen::Map<const Vector>(b.data(), static_cast<Eigen::Index>(b.size()));
    if (!residual.allFinite())
        return Result<LinearSolution>{failure()};
    const double residual_norm = residual.stableNorm();
    if (!sigma.empty())
        for (Eigen::Index i = 0; i < residual.size(); ++i)
            residual[i] /= sigma[static_cast<std::size_t>(i)];
    const double objective_norm = residual.stableNorm();
    if (!std::isfinite(residual_norm) || !residual.allFinite() || !std::isfinite(objective_norm))
        return Result<LinearSolution>{failure()};
    return Result<LinearSolution>{LinearSolution{{x.data(), x.data() + x.size()}, rank, residual_norm, objective_norm}};
}
} // namespace

Result<Matrix> multiply(MatrixView a, MatrixView b) {
    auto valid = validate(a);
    if (!valid)
        return Result<Matrix>{valid.error()};
    valid = validate(b);
    if (!valid)
        return Result<Matrix>{valid.error()};
    if (a.columns != b.rows || a.rows > static_cast<std::size_t>(std::numeric_limits<Eigen::Index>::max()) / b.columns)
        return Result<Matrix>{invalid("matrix", "Product dimensions are incompatible or exceed index range")};
    const Dense result = map(a) * map(b);
    if (!result.allFinite())
        return Result<Matrix>{failure()};
    return Result<Matrix>{owned(result)};
}
Result<QrDecomposition> qr(MatrixView a, RankTolerance tolerance) {
    auto valid = validate(a);
    if (!valid)
        return Result<QrDecomposition>{valid.error()};
    const auto cutoff = threshold(a, tolerance);
    if (!cutoff)
        return Result<QrDecomposition>{cutoff.error()};
    Dense scaled = map(a);
    const double scale = scale_of(scaled);
    scaled /= scale;
    Eigen::ColPivHouseholderQR<Dense> factor(scaled);
    factor.setThreshold(cutoff.value());
    const Eigen::Index k = std::min(scaled.rows(), scaled.cols());
    const Dense q = factor.householderQ() * Dense::Identity(scaled.rows(), k);
    Dense r = factor.matrixR().topRows(k).template triangularView<Eigen::Upper>();
    r *= scale;
    if (!q.allFinite() || !r.allFinite())
        return Result<QrDecomposition>{failure()};
    std::vector<std::size_t> permutation(a.columns);
    for (std::size_t i = 0; i < a.columns; ++i)
        permutation[i] = static_cast<std::size_t>(factor.colsPermutation().indices()[static_cast<Eigen::Index>(i)]);
    return Result<QrDecomposition>{
        QrDecomposition{owned(q), owned(r), std::move(permutation), static_cast<std::size_t>(factor.rank())}};
}
Result<SvdDecomposition> svd(MatrixView a, RankTolerance tolerance) {
    auto valid = validate(a);
    if (!valid)
        return Result<SvdDecomposition>{valid.error()};
    const auto cutoff = threshold(a, tolerance);
    if (!cutoff)
        return Result<SvdDecomposition>{cutoff.error()};
    Dense scaled = map(a);
    const double scale = scale_of(scaled);
    scaled /= scale;
    Eigen::JacobiSVD<Dense> factor(scaled, Eigen::ComputeThinU | Eigen::ComputeThinV);
    factor.setThreshold(cutoff.value());
    const Vector singular = factor.singularValues() * scale;
    if (factor.info() != Eigen::Success || !singular.allFinite() || !factor.matrixU().allFinite() ||
        !factor.matrixV().allFinite())
        return Result<SvdDecomposition>{failure()};
    return Result<SvdDecomposition>{SvdDecomposition{owned(factor.matrixU()),
                                                     {singular.data(), singular.data() + singular.size()},
                                                     owned(factor.matrixV()),
                                                     static_cast<std::size_t>(factor.rank())}};
}
Result<LinearSolution> solve_linear(MatrixView a, std::span<const double> b, RankTolerance tolerance) {
    auto valid = validate_rhs(a, b);
    if (!valid)
        return Result<LinearSolution>{valid.error()};
    if (a.rows != a.columns)
        return Result<LinearSolution>{invalid("matrix", "LU solve requires a square matrix")};
    const auto cutoff = threshold(a, tolerance);
    if (!cutoff)
        return Result<LinearSolution>{cutoff.error()};
    Dense scaled = map(a);
    const double scale = scale_of(scaled);
    scaled /= scale;
    Eigen::FullPivLU<Dense> factor(scaled);
    factor.setThreshold(cutoff.value());
    if (static_cast<std::size_t>(factor.rank()) != a.columns)
        return Result<LinearSolution>{deficient()};
    const Vector rhs = Eigen::Map<const Vector>(b.data(), static_cast<Eigen::Index>(b.size())) / scale;
    if (!rhs.allFinite())
        return Result<LinearSolution>{failure()};
    const Vector x = factor.solve(rhs);
    return solution(a, b, x, a.columns);
}
Result<Matrix> inverse(MatrixView a, RankTolerance tolerance) {
    auto valid = validate(a);
    if (!valid)
        return Result<Matrix>{valid.error()};
    if (a.rows != a.columns)
        return Result<Matrix>{invalid("matrix", "Inverse requires a square matrix")};
    const auto cutoff = threshold(a, tolerance);
    if (!cutoff)
        return Result<Matrix>{cutoff.error()};
    Dense scaled = map(a);
    const double scale = scale_of(scaled);
    scaled /= scale;
    Eigen::FullPivLU<Dense> factor(scaled);
    factor.setThreshold(cutoff.value());
    if (static_cast<std::size_t>(factor.rank()) != a.columns)
        return Result<Matrix>{deficient()};
    const Dense result = factor.inverse() / scale;
    if (!result.allFinite())
        return Result<Matrix>{failure()};
    return Result<Matrix>{owned(result)};
}
Result<double> determinant(MatrixView a) {
    auto valid = validate(a);
    if (!valid)
        return Result<double>{valid.error()};
    if (a.rows != a.columns)
        return Result<double>{invalid("matrix", "Determinant requires a square matrix")};
    // Full pivoting supplies permutation signs. Accumulate diagonal magnitude
    // logarithmically so intermediate products do not overflow unnecessarily.
    const Eigen::FullPivLU<Dense> factor(map(a));
    if (!factor.matrixLU().allFinite())
        return Result<double>{failure()};
    double sign = static_cast<double>(factor.permutationP().determinant() * factor.permutationQ().determinant());
    double logarithm = 0;
    for (Eigen::Index i = 0; i < factor.matrixLU().rows(); ++i) {
        const double pivot = factor.matrixLU()(i, i);
        if (pivot == 0)
            return Result<double>{0.0};
        if (pivot < 0)
            sign = -sign;
        logarithm += std::log(std::abs(pivot));
    }
    const double result = sign * std::exp(logarithm);
    if (!std::isfinite(result))
        return Result<double>{failure()};
    return Result<double>{result};
}
Result<LinearSolution> least_squares(MatrixView a, std::span<const double> b, LeastSquaresOptions options,
                                     std::span<const double> sigma) {
    auto valid = validate_rhs(a, b);
    if (!valid)
        return Result<LinearSolution>{valid.error()};
    const auto cutoff = threshold(a, options.tolerance);
    if (!cutoff)
        return Result<LinearSolution>{cutoff.error()};
    if ((options.method != LeastSquaresMethod::pivoted_qr && options.method != LeastSquaresMethod::svd) ||
        (options.method == LeastSquaresMethod::pivoted_qr && (a.rows < a.columns || options.allow_rank_deficient)))
        return Result<LinearSolution>{invalid("method", "Use SVD for underdetermined or rank-deficient solutions")};
    if (!sigma.empty() && (sigma.size() != a.rows || !std::all_of(sigma.begin(), sigma.end(),
                                                                  [](double x) { return std::isfinite(x) && x > 0; })))
        return Result<LinearSolution>{invalid("sigma", "Require one positive finite standard deviation per row")};
    Dense weighted = map(a);
    Vector rhs = Eigen::Map<const Vector>(b.data(), static_cast<Eigen::Index>(b.size()));
    if (!sigma.empty()) {
        for (std::size_t i = 0; i < a.rows; ++i) {
            weighted.row(static_cast<Eigen::Index>(i)) /= sigma[i];
            rhs[static_cast<Eigen::Index>(i)] /= sigma[i];
        }
    }
    if (!weighted.allFinite() || !rhs.allFinite())
        return Result<LinearSolution>{failure()};
    const double scale = scale_of(weighted);
    weighted /= scale;
    rhs /= scale;
    if (!rhs.allFinite())
        return Result<LinearSolution>{failure()};
    Vector x;
    std::size_t rank = 0;
    if (options.method == LeastSquaresMethod::pivoted_qr) {
        Eigen::ColPivHouseholderQR<Dense> factor(weighted);
        factor.setThreshold(cutoff.value());
        rank = static_cast<std::size_t>(factor.rank());
        if (rank != a.columns)
            return Result<LinearSolution>{deficient()};
        x = factor.solve(rhs);
    } else {
        Eigen::JacobiSVD<Dense> factor(weighted, Eigen::ComputeThinU | Eigen::ComputeThinV);
        factor.setThreshold(cutoff.value());
        if (factor.info() != Eigen::Success)
            return Result<LinearSolution>{failure()};
        rank = static_cast<std::size_t>(factor.rank());
        if (rank < std::min(a.rows, a.columns) && !options.allow_rank_deficient)
            return Result<LinearSolution>{deficient()};
        x = factor.solve(rhs);
    }
    return solution(a, b, x, rank, sigma);
}
} // namespace qualpf::numerics
