#pragma once
#include "qualpf/profile/pfsm.h"
#include "pfsm_work.h"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <limits>
#include <bit>
#include <cfenv>

namespace qualpf::profile {
struct PreparedProfileObservations {
    std::shared_ptr<const ProfileObservations> observations;
    Eigen::VectorXd normalized_net, sqrt_weights;
    double net_denominator = 0, observed_denominator = 0, normalization = 0;
    bool frozen = false;
    int rounding = std::fegetround();
    unsigned denormals = 0;
};
}

namespace qualpf::profile::detail {
using Vector = Eigen::VectorXd;
using Matrix = Eigen::MatrixXd;
struct Failure {
    Error error;
};
inline void fail(const char *field, const char *message, ErrorCode code = ErrorCode::invalid_input) {
    throw Failure{{code, field, message}};
}
inline void checkpoint(const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        fail("pfsm", "Profile fitting canceled", ErrorCode::cancelled);
}
struct LinearSolution {
    Vector scales, residual;
    std::size_t iterations = 0;
    double kkt = 0;
    bool deficient = false;
    std::vector<bool> zero;
};
// Owned by one reduced problem. Reuse storage, never an active-set solution.
struct NnlsWorkspace {
    Matrix normalized, passive_columns;
    Vector norms, x, dual, solution, trial, gradient;
    std::vector<bool> valid, passive;
    std::vector<Eigen::Index> indices;
    Eigen::ColPivHouseholderQR<Matrix> qr;
    Eigen::JacobiSVD<Matrix> svd;
    void resize(Eigen::Index n, Eigen::Index m) {
        normalized.resize(n, m);
        norms.resize(m);
        valid.assign(static_cast<std::size_t>(m), false);
        indices.reserve(static_cast<std::size_t>(m));
    }
};
LinearSolution nnls(const Matrix &, const Vector &, std::size_t, NnlsWorkspace &, const ExecutionContext &);
std::size_t numericalRank(const Matrix &, const ExecutionContext &);
struct Evaluation {
    Vector residual, scales;
    double cost = 0;
    LinearSolution linear;
};
class ReducedProblem {
    PreparedProfileObservationsHandle prepared_;

  public:
    ReducedProblem(const ProfileFitInput &, const ExecutionContext &);
    Evaluation evaluate(const Vector &shifts, bool difference = false);
    Evaluation explicitParameters(const ProfileFitParameters &);
    ProfileFitResult result(const Evaluation &, const Vector &, ProfileFitTermination, double optimality);
    double scanObjective(const Evaluation &, const Vector &, std::vector<double> &calculated);
    void anchorColumns(const Vector &);
    void clearAnchor() { anchor_.valid = false; }
    void enableExact() {
        exact_mode = true;
        last_shifts.setConstant(std::numeric_limits<double>::quiet_NaN());
        clearAnchor();
    }
    bool approximated = false, exact_mode = false;
    const ProfileFitInput &input;
    const ExecutionContext &context;
    const Vector &z, &sqrt_weights;
    Matrix columns, unweighted_columns;
    double net_denominator = 0, observed_denominator = 0, normalization = 0, width = 1;
    std::vector<std::size_t> free;
    ProfileFitDiagnostics counts;

  private:
    struct AnchorColumns {
        Vector shifts;
        Matrix weighted, unweighted;
        bool valid = false;
    } anchor_;
    Vector last_shifts;
    NnlsWorkspace linear_workspace;
    void update(const Vector &);
};
struct OuterResult {
    Evaluation evaluation;
    Vector shifts;
    ProfileFitTermination termination;
    double optimality = 0;
};
OuterResult trf(ReducedProblem &, const Vector &);
inline Vector vectorOf(std::span<const double> v) {
    return Eigen::Map<const Vector>(v.data(), static_cast<Eigen::Index>(v.size()));
}
inline std::vector<double> valuesOf(const Vector &v) {
    if (!v.size())
        return {};
    return {v.data(), v.data() + v.size()};
}
} // namespace qualpf::profile::detail
