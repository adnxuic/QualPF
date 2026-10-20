#include "pfsm_internal.h"

namespace qualpf::profile::detail {
namespace {
// Dense positive-semidefinite trust-region subproblem, solved in its eigenbasis.
Vector trustStep(const Matrix &b, const Vector &g, double radius) {
    Eigen::SelfAdjointEigenSolver<Matrix> eig(b);
    if (eig.info() != Eigen::Success) fail("pfsm.trf", "Trust-region eigensolver failed", ErrorCode::numerical_failure);
    Vector lambda = eig.eigenvalues();
    const double tolerance = 64 * std::numeric_limits<double>::epsilon() * std::max(1., b.norm());
    if (lambda.minCoeff() < -tolerance) fail("pfsm.trf", "Indefinite Gauss-Newton model", ErrorCode::numerical_failure);
    lambda = lambda.cwiseMax(0);
    const Vector projected = eig.eigenvectors().transpose() * g;
    const auto step = [&](double damping) -> Vector {
        Vector p(g.size());
        for (Eigen::Index j = 0; j < g.size(); ++j) {
            const double d = lambda[j] + damping;
            p[j] = d > 0 ? -projected[j] / d : 0;
        }
        return p;
    };
    Vector p = step(0);
    bool null_gradient = false;
    for (Eigen::Index j = 0; j < g.size(); ++j)
        if (lambda[j] == 0 && projected[j] != 0) null_gradient = true;
    if (!null_gradient && p.stableNorm() <= radius) return eig.eigenvectors() * p;
    double lower = 0, upper = std::max(g.stableNorm() / radius, std::numeric_limits<double>::min());
    while (step(upper).stableNorm() > radius) {
        upper *= 2;
        if (!std::isfinite(upper)) fail("pfsm.trf", "Trust radius underflow", ErrorCode::numerical_failure);
    }
    for (int k = 0; k < 80; ++k) {
        const double middle = .5 * (lower + upper);
        if (step(middle).stableNorm() > radius) lower = middle; else upper = middle;
    }
    return eig.eigenvectors() * step(upper);
}
double toBound(const Vector &x, const Vector &p, const Vector &lower, const Vector &upper) {
    double stride = std::numeric_limits<double>::infinity();
    for (Eigen::Index j = 0; j < x.size(); ++j) {
        if (p[j] > 0) stride = std::min(stride, (upper[j] - x[j]) / p[j]);
        else if (p[j] < 0) stride = std::min(stride, (lower[j] - x[j]) / p[j]);
    }
    return std::max(0., stride);
}
double toSphere(const Vector &start, const Vector &direction, double radius) {
    const double a = direction.squaredNorm(), b = start.dot(direction);
    if (!a) return 0;
    const double remaining = std::max(0., radius * radius - start.squaredNorm());
    const double root = std::sqrt(b * b + a * remaining);
    return b > 0 ? remaining / (root + b) : (root - b) / a;
}
Vector selectStep(const Vector &x, const Vector &lower, const Vector &upper,
    const Vector &d, const Matrix &b, const Vector &g, double radius, double theta) {
    const auto model = [&](const Vector &p) { return g.dot(p) + .5 * p.dot(b * p); };
    Vector p = trustStep(b, g, radius);
    const Vector physical = d.cwiseProduct(p);
    const double hit = toBound(x, physical, lower, upper);
    if (hit >= 1) return p;
    Vector best = (theta * hit) * p;
    double best_model = model(best);
    const auto consider = [&](const Vector &start, const Vector &direction, double limit) {
        if (!(limit > 0) || !std::isfinite(limit)) return;
        const double curvature = direction.dot(b * direction);
        const double slope = direction.dot(g + b * start);
        const double length = curvature > 0 ? std::clamp(-slope / curvature, 0., limit) : (slope < 0 ? limit : 0.);
        const Vector candidate = start + length * direction;
        const double value = model(candidate);
        if (value < best_model) { best = candidate; best_model = value; }
    };
    Vector reflected = p;
    const Vector on_bound = x + hit * physical;
    for (Eigen::Index j = 0; j < x.size(); ++j) {
        const double individual = physical[j] > 0 ? (upper[j] - x[j]) / physical[j] :
            physical[j] < 0 ? (lower[j] - x[j]) / physical[j] : std::numeric_limits<double>::infinity();
        if (individual <= hit * (1 + 16 * std::numeric_limits<double>::epsilon())) reflected[j] = -reflected[j];
    }
    const Vector boundary_step = hit * p;
    const double reflected_limit = std::min(toSphere(boundary_step, reflected, radius),
        toBound(on_bound, d.cwiseProduct(reflected), lower, upper));
    // Retract the entire reflected path slightly to retain strict feasibility.
    consider(theta * boundary_step, theta * reflected, reflected_limit);
    const Vector cauchy = -g;
    const double cauchy_limit = std::min(radius / std::max(cauchy.norm(), std::numeric_limits<double>::min()),
        theta * toBound(x, d.cwiseProduct(cauchy), lower, upper));
    consider(Vector::Zero(x.size()), cauchy, cauchy_limit);
    return best;
}
}
OuterResult trf(ReducedProblem &problem, const Vector &start) {
    auto current = problem.evaluate(start); // Validates all coordinates, including fixed bounds.
    const auto n = static_cast<Eigen::Index>(problem.free.size());
    if (!n) return {std::move(current), start, ProfileFitTermination::fixed_shifts, 0};
    Vector x(n), lower(n), upper(n);
    for (Eigen::Index j = 0; j < n; ++j) {
        const auto index = problem.free[static_cast<std::size_t>(j)];
        const auto &p = problem.input.phases[index];
        lower[j] = p.lower_shift / problem.width; upper[j] = p.upper_shift / problem.width;
        x[j] = start[static_cast<Eigen::Index>(index)] / problem.width;
        if (!std::isfinite(lower[j]) || !std::isfinite(upper[j])) fail("pfsm.bounds", "Dimensionless bounds overflow");
        if (x[j] <= lower[j]) x[j] = std::nextafter(lower[j], upper[j]);
        if (x[j] >= upper[j]) x[j] = std::nextafter(upper[j], lower[j]);
    }
    const auto shifts = [&](const Vector &t) -> Vector {
        Vector result = start;
        for (Eigen::Index j = 0; j < n; ++j) {
            const auto index = problem.free[static_cast<std::size_t>(j)];
            const auto &p = problem.input.phases[index];
            // Only undo a possible rounding error from the unit conversion.
            result[static_cast<Eigen::Index>(index)] = std::clamp(t[j] * problem.width, p.lower_shift, p.upper_shift);
        }
        return result;
    };
    current = problem.evaluate(shifts(x));
    const auto &options = problem.input.options;
    const auto budget = options.maximum_evaluations ? options.maximum_evaluations :
        100 * std::max<std::size_t>(1, problem.input.phases.size());
    std::size_t evaluations = 1;
    double radius = 1, optimality = 0;
    const double difference = std::cbrt(std::numeric_limits<double>::epsilon());
    ProfileFitTermination termination = ProfileFitTermination::budget;
    const auto differentiate = [&](const Vector &x, const Vector &residual) -> Matrix {
        Matrix jacobian(residual.size(), n);
        for (Eigen::Index j = 0; j < n; ++j) {
            const double desired = difference * std::max(1., std::abs(x[j]));
            const double left = x[j] - lower[j], right = upper[j] - x[j];
            Vector one = x, two = x;
            double h;
            if (std::min(left, right) >= desired) {
                h = desired; one[j] -= h; two[j] += h;
                const auto minus = problem.evaluate(shifts(one), true);
                const auto plus = problem.evaluate(shifts(two), true);
                jacobian.col(j) = (plus.residual - minus.residual) / (two[j] - one[j]);
            } else {
                h = (right >= left ? 1 : -1) * std::min(desired, .5 * std::max(left, right));
                one[j] += h; two[j] += 2 * h;
                if (one[j] == x[j] || two[j] == one[j]) {
                    jacobian.col(j).setZero(); continue;
                }
                const auto first = problem.evaluate(shifts(one), true);
                const auto second = problem.evaluate(shifts(two), true);
                jacobian.col(j) = (-3 * residual + 4 * first.residual - second.residual) / (2 * h);
            }
        }
        if (!jacobian.allFinite()) fail("pfsm.trf", "Nonfinite reduced Jacobian", ErrorCode::numerical_failure);
        return jacobian;
    };
    while (evaluations < budget) {
        checkpoint(problem.context);
        const Matrix jacobian = differentiate(x, current.residual);
        const Vector gradient = jacobian.transpose() * current.residual;
        Vector v(n), dv(n);
        for (Eigen::Index j = 0; j < n; ++j) {
            v[j] = gradient[j] < 0 ? upper[j] - x[j] : gradient[j] > 0 ? x[j] - lower[j] : 1;
            dv[j] = gradient[j] < 0 ? -1 : gradient[j] > 0 ? 1 : 0;
        }
        optimality = gradient.cwiseProduct(v).lpNorm<Eigen::Infinity>();
        if (optimality < options.gtol) { termination = ProfileFitTermination::gradient; break; }
        const Vector d = v.cwiseSqrt();
        const Matrix scaled_jacobian = jacobian * d.asDiagonal();
        Matrix b = scaled_jacobian.transpose() * scaled_jacobian;
        b.diagonal() += gradient.cwiseProduct(dv);
        const Vector g = d.cwiseProduct(gradient);
        const double theta = std::max(.995, 1 - optimality);
        bool accepted = false, finished = false;
        while (evaluations < budget) {
            checkpoint(problem.context);
            const Vector p = selectStep(x, lower, upper, d, b, g, radius, theta);
            const Vector physical = d.cwiseProduct(p);
            Vector trial = x + physical;
            for (Eigen::Index j = 0; j < n; ++j) {
                if (trial[j] <= lower[j]) trial[j] = std::nextafter(lower[j], upper[j]);
                if (trial[j] >= upper[j]) trial[j] = std::nextafter(upper[j], lower[j]);
            }
            const double predicted = -(g.dot(p) + .5 * p.dot(b * p));
            if (!(predicted > 0) || (trial - x).norm() == 0) {
                termination = ProfileFitTermination::no_progress; finished = true; break;
            }
            auto candidate = problem.evaluate(shifts(trial)); ++evaluations;
            const double actual = current.cost - candidate.cost;
            const double ratio = actual / predicted;
            const double length = p.norm();
            if (ratio < .25) radius = .25 * length;
            else if (ratio > .75 && length > .95 * radius) radius *= 2;
            const bool cost_stop = actual >= 0 && actual < options.ftol * current.cost && ratio > .25;
            const bool step_stop = physical.norm() < options.xtol * (options.xtol + x.norm());
            if (actual > 0) { x = std::move(trial); current = std::move(candidate); accepted = true; }
            if (cost_stop || step_stop) {
                termination = cost_stop && step_stop ? ProfileFitTermination::cost_and_step :
                    cost_stop ? ProfileFitTermination::cost : ProfileFitTermination::step;
                finished = true; break;
            }
            if (accepted) break;
        }
        if (finished) break;
    }
    Vector final_shifts = shifts(x);
    // Final scales are always jointly resolved at the accepted location.
    current = problem.evaluate(final_shifts);
    // Check a nearby exact bound instead of returning a numerically interior
    // approximation to a constrained optimum. Keep it only if Q does not rise.
    for (Eigen::Index j = 0; j < n; ++j) {
        const double tolerance = std::sqrt(std::numeric_limits<double>::epsilon()) *
            std::max({1., std::abs(lower[j]), std::abs(upper[j])});
        double bound;
        if (x[j] - lower[j] <= tolerance) bound = lower[j];
        else if (upper[j] - x[j] <= tolerance) bound = upper[j];
        else continue;
        Vector candidate_shifts = final_shifts;
        const auto index = problem.free[static_cast<std::size_t>(j)];
        const auto &phase = problem.input.phases[index];
        candidate_shifts[static_cast<Eigen::Index>(index)] = bound == lower[j] ? phase.lower_shift : phase.upper_shift;
        auto candidate = problem.evaluate(candidate_shifts);
        if (candidate.cost <= current.cost) {
            current = std::move(candidate); final_shifts = std::move(candidate_shifts);
        }
    }
    for (Eigen::Index j = 0; j < n; ++j)
        x[j] = final_shifts[static_cast<Eigen::Index>(problem.free[static_cast<std::size_t>(j)])] / problem.width;
    const Vector final_gradient = differentiate(x, current.residual).transpose() * current.residual;
    optimality = 0;
    for (Eigen::Index j = 0; j < n; ++j) {
        const double distance = final_gradient[j] < 0 ? upper[j] - x[j] :
            final_gradient[j] > 0 ? x[j] - lower[j] : 1.;
        optimality = std::max(optimality, std::abs(final_gradient[j] * distance));
    }
    return {std::move(current), final_shifts, termination, optimality};
}
} // namespace qualpf::profile::detail
