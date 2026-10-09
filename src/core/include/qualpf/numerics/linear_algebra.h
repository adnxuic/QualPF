#pragma once

#include "qualpf/domain/result.h"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace qualpf::numerics {

// Row-major, contiguous binary64 values. All entry points validate dimensions,
// storage length and finite values before accessing data. Views are borrowed
// only during a call; all results own their storage. No library types escape.
struct MatrixView {
    std::size_t rows;
    std::size_t columns;
    std::span<const double> values;
};
struct Matrix {
    std::size_t rows = 0;
    std::size_t columns = 0;
    std::vector<double> values;
    MatrixView view() const & { return {rows, columns, values}; }
    MatrixView view() const && = delete;
};

struct RankTolerance {
    // Relative to largest pivot (QR/LU) or singular value (SVD).
    // nullopt = epsilon * max(rows, columns); require 0 <= value < 1.
    std::optional<double> relative;
};
enum class LeastSquaresMethod { pivoted_qr, svd };
struct LeastSquaresOptions {
    LeastSquaresMethod method = LeastSquaresMethod::pivoted_qr;
    RankTolerance tolerance;
    // SVD only: allow truncated minimum-norm solutions for deficient rank.
    // QR deliberately reports deficient rank instead of choosing a basic solution.
    bool allow_rank_deficient = false;
};
struct LinearSolution {
    std::vector<double> x;
    std::size_t rank;
    double residual_norm;  // ||A*x-b||, original unweighted input units
    double objective_norm; // ||(A*x-b)/sigma||; same as residual when unweighted
};
struct QrDecomposition {
    Matrix q;                             // thin Q, m x min(m,n)
    Matrix r;                             // min(m,n) x n
    std::vector<std::size_t> permutation; // A[:, permutation] = Q * R
    std::size_t rank;
};
struct SvdDecomposition {
    Matrix u;                            // thin U, m x min(m,n)
    std::vector<double> singular_values; // nonnegative, descending
    Matrix v;                            // thin V, n x min(m,n); A = U * diag(s) * transpose(V)
    std::size_t rank;
};

Result<Matrix> multiply(MatrixView a, MatrixView b);
Result<QrDecomposition> qr(MatrixView a, RankTolerance tolerance = {});
Result<SvdDecomposition> svd(MatrixView a, RankTolerance tolerance = {});
// Full-pivot LU; square nonsingular systems only. Never substitutes tiny pivots.
Result<LinearSolution> solve_linear(MatrixView a, std::span<const double> b, RankTolerance tolerance = {});
Result<Matrix> inverse(MatrixView a, RankTolerance tolerance = {});
Result<double> determinant(MatrixView a);
// Empty sigma means unweighted. Otherwise every sigma must be finite and > 0.
// Minimizes sum(((A*x-b)/sigma)^2), without forming normal equations.
Result<LinearSolution> least_squares(MatrixView a, std::span<const double> b, LeastSquaresOptions options = {},
                                     std::span<const double> sigma = {});

} // namespace qualpf::numerics
