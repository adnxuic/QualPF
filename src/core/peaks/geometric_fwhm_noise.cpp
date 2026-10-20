#include "geometric_fwhm_noise.h"
#include <Eigen/Eigenvalues>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace qualpf::preprocessing::geometric_detail {
namespace {
constexpr double tolerance = 64 * std::numeric_limits<double>::epsilon();
Error invalid(const char *reason) { return {ErrorCode::invalid_input, "fwhm.noise", reason, reason}; }
bool nonnegative(double v) { return std::isfinite(v) && v >= 0; }
}
double Noise::sigma(std::size_t first, std::size_t last) const {
    double maximum = 0;
    for (auto i = first; i <= last; ++i) maximum = std::max(maximum, variance[i]);
    return std::sqrt(maximum);
}
double Noise::covariance(std::size_t i, std::size_t j) const {
    if (i == j) return variance[i];
    const auto it = upper.find(std::minmax(i, j));
    return it == upper.end() ? 0 : it->second;
}
Result<Noise> prepare_noise(const GeometricFwhmInput &in, const std::vector<bool> &valid,
    const ExecutionContext &context) {
    using Return = Result<Noise>;
    Noise noise;
    const auto n = in.intensity.size();
    if (std::holds_alternative<std::monostate>(in.noise)) return Return{std::move(noise)};
    noise.variance.resize(n);
    const auto sd = std::get_if<FwhmStandardDeviations>(&in.noise);
    const auto counts = std::get_if<FwhmPoissonCounts>(&in.noise);
    const auto rate = std::get_if<FwhmPoissonRates>(&in.noise);
    const auto cov = std::get_if<FwhmCovariance>(&in.noise);
    if ((sd && sd->values.size() != n) || (rate && (rate->counts.size() != n || rate->seconds.size() != n)) ||
        (cov && cov->diagonal.size() != n)) return Return{invalid("invalid_noise_shape")};
    noise.model = sd ? FwhmNoiseModel::independent_standard_deviations : counts ? FwhmNoiseModel::poisson_counts :
        rate ? FwhmNoiseModel::poisson_rates : FwhmNoiseModel::covariance;
    for (std::size_t i = 0; i < n; ++i) {
        if ((i & 1023) == 0 && context.cancellation.stop_requested())
            return Return{Error{ErrorCode::cancelled, "fwhm.noise", "FWHM estimation canceled"}};
        double variance = 0;
        if (sd) {
            if (!nonnegative(sd->values[i])) return Return{invalid("invalid_standard_deviation")};
            variance = sd->values[i] * sd->values[i];
            if (sd->values[i] > 0 && variance == 0) return Return{invalid("variance_underflow")};
        } else if (counts) {
            if (!valid[i]) continue;
            if (!nonnegative(in.intensity[i])) return Return{invalid("invalid_raw_counts")};
            variance = std::max(in.intensity[i], 1.0);
        } else if (rate) {
            if (!nonnegative(rate->counts[i]) || !std::isfinite(rate->seconds[i]) || !(rate->seconds[i] > 0))
                return Return{invalid("invalid_count_rate")};
            const double expected = rate->counts[i] / rate->seconds[i];
            if (!std::isfinite(expected) || (valid[i] && std::abs(in.intensity[i] - expected) >
                tolerance * std::max(std::abs(in.intensity[i]), std::abs(expected))))
                return Return{invalid("inconsistent_count_rate_units")};
            const double sigma = std::sqrt(std::max(rate->counts[i], 1.0)) / rate->seconds[i];
            variance = sigma * sigma;
            if (variance == 0) return Return{invalid("variance_underflow")};
        } else if (cov) variance = cov->diagonal[i];
        if (!nonnegative(variance)) return Return{invalid("invalid_variance")};
        noise.variance[i] = variance;
    }
    if (cov) for (std::size_t k = 0; k < cov->upper.size(); ++k) {
        if ((k & 1023) == 0 && context.cancellation.stop_requested())
            return Return{Error{ErrorCode::cancelled, "fwhm.noise", "FWHM estimation canceled"}};
        const auto &c = cov->upper[k];
        if (c.first >= c.second || c.second >= n || !std::isfinite(c.value))
            return Return{invalid("invalid_covariance_entry")};
        if (!noise.upper.emplace(std::make_pair(c.first, c.second), c.value).second)
            return Return{invalid("duplicate_covariance_entry")};
        const double s1 = std::sqrt(noise.variance[c.first]), s2 = std::sqrt(noise.variance[c.second]);
        if ((s1 == 0 || s2 == 0) ? c.value != 0 : std::abs(c.value) / s1 / s2 > 1 + tolerance)
            return Return{invalid("invalid_covariance_bound")};
    }
    return Return{std::move(noise)};
}

Status propagate(const GeometricFwhmInput &in, const std::vector<double> &z,
    const Noise &noise, PeakFwhmDiagnostic &d) {
    const auto p = d.sample_index;
    std::array<std::size_t, 7> indexes{p - 1, p, p + 1, d.left->first, d.left->second, d.right->first, d.right->second};
    std::sort(indexes.begin(), indexes.end());
    const auto end = std::unique(indexes.begin(), indexes.end());
    const auto size = static_cast<Eigen::Index>(end - indexes.begin());
    const auto at = [&](std::size_t i) { return static_cast<Eigen::Index>(std::lower_bound(indexes.begin(), end, i) - indexes.begin()); };
    // Envelope theorem: derivative of the interpolating polynomial's maximum
    // is its Lagrange basis at the vertex; no derivative of the vertex is needed.
    const double hl = in.two_theta[p].value - in.two_theta[p - 1].value;
    const double hr = in.two_theta[p + 1].value - in.two_theta[p].value;
    const double scale = std::max(hl, hr);
    const double l = hl / scale, r = hr / scale;
    const double u = *d.apex_offset_degrees / scale;
    Eigen::VectorXd ga = Eigen::VectorXd::Zero(size);
    ga[at(p - 1)] = u * (u - r) / (l * (l + r));
    ga[at(p)] = -(u + l) * (u - r) / (l * r);
    ga[at(p + 1)] = u * (u + l) / (r * (l + r));
    auto gradient = [&](const FwhmCrossing &c) -> Eigen::VectorXd {
        const double ys = std::max(std::abs(z[c.first]), std::abs(z[c.second]));
        const double slope_factor = ((in.two_theta[c.second].value - in.two_theta[c.first].value) / ys) /
            (z[c.second] / ys - z[c.first] / ys);
        Eigen::VectorXd g = .5 * ga;
        g[at(c.first)] -= 1 - c.fraction;
        g[at(c.second)] -= c.fraction;
        return (g * slope_factor).eval();
    };
    Eigen::VectorXd g = gradient(*d.right) - gradient(*d.left);
    if (!g.allFinite()) return Status{invalid("uncertainty_gradient_overflow")};
    double covariance_scale = 0;
    for (auto i = indexes.begin(); i != end; ++i) covariance_scale = std::max(covariance_scale, noise.variance[*i]);
    double uncertainty = 0;
    if (covariance_scale > 0) {
        Eigen::MatrixXd covariance(size, size);
        for (Eigen::Index i = 0; i < size; ++i) for (Eigen::Index j = 0; j < size; ++j)
            covariance(i, j) = noise.covariance(indexes[i], indexes[j]) / covariance_scale;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> solver(covariance, Eigen::EigenvaluesOnly);
        const double bound = tolerance * covariance.cwiseAbs().rowwise().sum().maxCoeff();
        if (solver.info() != Eigen::Success || !solver.eigenvalues().allFinite() || solver.eigenvalues().minCoeff() < -bound)
            return Status{invalid("indefinite_local_covariance")};
        const double gs = g.cwiseAbs().maxCoeff();
        if (gs > 0) {
            g /= gs;
            const double variance = g.dot(covariance * g);
            if (!std::isfinite(variance) || variance < -bound * g.squaredNorm())
                return Status{invalid("invalid_propagated_variance")};
            // Only negative roundoff within the PSD tolerance is rounded to zero.
            uncertainty = (std::sqrt(std::max(0.0, variance)) * gs) * std::sqrt(covariance_scale);
        }
    }
    const double relative = uncertainty / d.width->value;
    if (!std::isfinite(uncertainty) || !std::isfinite(relative)) return Status{invalid("uncertainty_overflow")};
    d.uncertainty = AngularWidthDegrees{uncertainty};
    d.relative_uncertainty = relative;
    d.conditional_uncertainty = true;
    return Status{std::monostate{}};
}
} // namespace qualpf::preprocessing::geometric_detail
