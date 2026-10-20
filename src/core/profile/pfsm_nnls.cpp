#include "pfsm_internal.h"

namespace qualpf::profile::detail {
LinearSolution nnls(const Matrix &a, const Vector &z, std::size_t budget, NnlsWorkspace &workspace,
                    const ExecutionContext &context) {
    const auto m = a.cols(), n = a.rows();
    LinearSolution out;
    out.scales = Vector::Zero(m);
    out.zero.resize(static_cast<std::size_t>(m));
    if (!m) { out.residual = z; return out; }
    auto &normalized = workspace.normalized;
    auto &norms = workspace.norms;
    for (Eigen::Index j = 0; j < m; ++j) {
        if (!workspace.valid[static_cast<std::size_t>(j)]) {
            checkpoint(context);
            normalized.col(j) = a.col(j);
            norms[j] = a.col(j).stableNorm();
            if (!std::isfinite(norms[j])) fail("pfsm.nnls", "Nonfinite template norm", ErrorCode::numerical_failure);
            if (norms[j] > 0) normalized.col(j) /= norms[j];
            workspace.valid[static_cast<std::size_t>(j)] = true;
        }
        out.zero[static_cast<std::size_t>(j)] = norms[j] == 0;
    }
    const double eps = std::numeric_limits<double>::epsilon();
    const double tolerance = 64 * eps * double(std::max(n, m)) * std::max(1., z.norm());
    auto &x = workspace.x;
    x.setZero(m);
    auto &passive = workspace.passive;
    passive.assign(static_cast<std::size_t>(m), false);
    if (!budget) budget = std::max<std::size_t>(100, 30 * std::size_t(m) * std::size_t(m));
    if (m == 1) {
        if (norms[0] > 0) x[0] = std::max(0., normalized.col(0).dot(z));
        passive[0] = x[0] > 0;
    } else for (;;) {
        checkpoint(context);
        auto &dual = workspace.dual;
        dual = normalized.transpose() * (z - normalized * x);
        Eigen::Index entering = -1;
        double best = tolerance;
        for (Eigen::Index j = 0; j < m; ++j)
            if (!passive[std::size_t(j)] && !out.zero[std::size_t(j)] && dual[j] > best) {
                entering = j; best = dual[j];
            }
        if (entering < 0) break;
        passive[std::size_t(entering)] = true;
        for (;;) {
            checkpoint(context);
            if (++out.iterations > budget)
                fail("pfsm.nnls", "NNLS active-set budget exhausted", ErrorCode::numerical_failure);
            auto &indices = workspace.indices;
            indices.clear();
            for (Eigen::Index j = 0; j < m; ++j) if (passive[std::size_t(j)]) indices.push_back(j);
            if (indices.empty()) break;
            auto &ap = workspace.passive_columns;
            ap.resize(n, static_cast<Eigen::Index>(indices.size()));
            for (std::size_t j = 0; j < indices.size(); ++j) ap.col(static_cast<Eigen::Index>(j)) = normalized.col(indices[j]);
            auto &qr = workspace.qr;
            qr.compute(ap);
            qr.setThreshold(eps * double(std::max(ap.rows(), ap.cols())));
            auto &solution = workspace.solution;
            if (qr.rank() < ap.cols()) {
                auto &svd = workspace.svd;
                svd.compute(ap, Eigen::ComputeThinU | Eigen::ComputeThinV);
                svd.setThreshold(eps * double(std::max(ap.rows(), ap.cols())));
                solution = svd.solve(z);
                out.deficient = true;
            } else solution = qr.solve(z);
            if (!solution.allFinite()) fail("pfsm.nnls", "Nonfinite free-set solution", ErrorCode::numerical_failure);
            auto &trial = workspace.trial;
            trial.setZero(m);
            bool positive = true;
            for (std::size_t j = 0; j < indices.size(); ++j) {
                trial[indices[j]] = solution[static_cast<Eigen::Index>(j)];
                positive = positive && trial[indices[j]] > 0;
            }
            if (positive) { x = trial; break; }
            double alpha = 1;
            for (auto j : indices) if (trial[j] <= 0) {
                const double denominator = x[j] - trial[j];
                alpha = std::min(alpha, denominator > 0 ? x[j] / denominator : 0.);
            }
            x += alpha * (trial - x);
            const double feasibility = eps * 8 * std::max(1., x.lpNorm<Eigen::Infinity>());
            for (auto j : indices) if (x[j] <= feasibility) { x[j] = 0; passive[std::size_t(j)] = false; }
        }
    }
    auto &gradient = workspace.gradient;
    gradient = normalized.transpose() * (normalized * x - z);
    for (Eigen::Index j = 0; j < m; ++j) {
        const double violation = x[j] > 0 ? std::abs(gradient[j]) : std::max(0., -gradient[j]);
        out.kkt = std::max(out.kkt, violation);
        out.scales[j] = norms[j] > 0 ? x[j] / norms[j] : 0;
    }
    if (!out.scales.allFinite() || out.kkt > tolerance * 4)
        fail("pfsm.nnls", "NNLS optimality check failed", ErrorCode::numerical_failure);
    out.residual = z - a * out.scales;
    return out;
}
std::size_t numericalRank(const Matrix &a, const ExecutionContext &context) {
    checkpoint(context);
    const auto m = a.cols(), n = a.rows();
    if (!m) return 0;
    const double threshold = std::numeric_limits<double>::epsilon() * double(std::max(n, m));
    // Full-matrix rank is reporting metadata. Free-set QR/SVD above still runs
    // on every NNLS solve, but only a returned fit needs this separate diagnostic.
    if (m == 1) return a.col(0).stableNorm() > 0 && threshold < 1 ? 1 : 0;
    Matrix normalized = a;
    for (Eigen::Index j = 0; j < m; ++j) {
        checkpoint(context);
        const double norm = a.col(j).stableNorm();
        if (norm > 0) normalized.col(j) /= norm;
    }
    Eigen::JacobiSVD<Matrix> rank_svd(normalized);
    rank_svd.setThreshold(threshold);
    return static_cast<std::size_t>(rank_svd.rank());
}
} // namespace qualpf::profile::detail
