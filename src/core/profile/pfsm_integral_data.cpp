#include "qualpf/profile/pfsm_integral_screen.h"
#include "pfsm_internal.h"
#include <numeric>

namespace qualpf::profile {
namespace {
using namespace detail;
double median(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    return v.size() % 2 ? v[v.size() / 2] : std::midpoint(v[v.size() / 2 - 1], v[v.size() / 2]);
}
double mass(double lo, double hi, double a) {
    if (lo == hi) return 0;
    if (hi <= 0) return mass(-hi, -lo, a);
    if (lo <= 0) return .5 * (hi / std::hypot(hi, a) - lo / std::hypot(lo, a));
    // Rationalize the difference on the same side of the peak. Scaling and
    // hypot avoid squared physical coordinates and saturated CDF subtraction.
    const double scale = std::max(hi, a);
    const double l = lo / scale, u = hi / scale, b = a / scale;
    const double hl = std::hypot(l, b), hu = std::hypot(u, b);
    return .5 * (b / hl) * (b / hu) * ((u - l) * (u + l) / (u * hl + l * hu));
}
void finish(PfsmIntegralLayer &layer) {
    for (std::size_t j = 0; j < layer.weights.size(); ++j) {
        if (layer.weights[j] > 0) layer.net[j] /= layer.weights[j];
        const double z = std::sqrt(layer.weights[j]) * layer.net[j];
        layer.denominator += z * z;
        if (!std::isfinite(layer.weights[j]) || !std::isfinite(layer.net[j]))
            fail("pfsm.integral.data", "Integral aggregation overflow", ErrorCode::numerical_failure);
    }
    if (!(layer.denominator > 0) || !std::isfinite(layer.denominator))
        fail("pfsm.integral.data", "No usable integral signal", ErrorCode::numerical_failure);
}
}
Status validatePfsmIntegralOptions(const PfsmIntegralOptions &o) {
    if (o != PfsmIntegralOptions{})
        return Status{Error{ErrorCode::invalid_input, "pfsm.integral.options", "Expected frozen integral v1 configuration"}};
    return Status{std::monostate{}};
}
Result<double> pfsmIntegralMass(double lo, double hi, double width) {
    if (!std::isfinite(lo) || !std::isfinite(hi) || lo > hi || !std::isfinite(width) || width <= 0)
        return Result<double>{Error{ErrorCode::invalid_input, "pfsm.integral.mass", "Invalid IL integral bounds/width"}};
    const double a = width / std::sqrt(4 * (std::cbrt(4.) - 1));
    if (!(a > 0)) return Result<double>{Error{ErrorCode::numerical_failure, "pfsm.integral.mass", "Unrepresentable IL width"}};
    const double value = mass(lo, hi, a);
    if (!std::isfinite(value) || value < 0 || value > 1)
        return Result<double>{Error{ErrorCode::numerical_failure, "pfsm.integral.mass", "Invalid IL integral"}};
    return Result<double>{value};
}
Result<std::shared_ptr<const PfsmIntegralData>> preparePfsmIntegralData(
    std::shared_ptr<const ProfileObservations> observations, std::span<const std::uint8_t> breaks,
    double width, const PfsmIntegralOptions &options, const ExecutionContext &context) try {
    checkpoint(context);
    const auto validOptions = validatePfsmIntegralOptions(options);
    if (!validOptions) throw Failure{validOptions.error()};
    if (!observations || context.numerical_policy != NumericalPolicy::explicit_float64 ||
        !std::isfinite(width) || width <= 0 || width > 180)
        fail("pfsm.integral.input", "Owned binary64 observations and physical width required");
    const auto &o = *observations;
    const auto n = o.coordinates.size();
    if (n < 2 || o.observed.size() != n || o.background.size() != n || o.weights.size() != n ||
        (!breaks.empty() && breaks.size() != n)) fail("pfsm.integral.input", "Misaligned observations/mask");
    std::vector<double> spacing;
    std::size_t effective = 0;
    for (std::size_t i = 0; i < n; ++i) {
        checkpoint(context);
        if (!std::isfinite(o.coordinates[i]) || (i && o.coordinates[i] <= o.coordinates[i - 1]) ||
            !std::isfinite(o.observed[i]) || !std::isfinite(o.background[i]) ||
            !std::isfinite(o.weights[i]) || o.weights[i] < 0)
            fail("pfsm.integral.input", "Invalid observations");
        if (i) spacing.push_back(o.coordinates[i] - o.coordinates[i - 1]);
        effective += o.weights[i] > 0;
    }
    if (!effective) fail("pfsm.integral.data", "No positive-weight observations", ErrorCode::numerical_failure);
    const double gap = 3 * median(std::move(spacing));
    const auto disconnected = [&](std::size_t i) {
        return (!breaks.empty() && breaks[i]) || o.coordinates[i] - o.coordinates[i - 1] > gap;
    };
    auto data = std::make_shared<PfsmIntegralData>();
    data->original = std::move(observations); data->options = options; data->width = width;
    const double eps = options.tail_mass_epsilon;
    data->radius = (width / std::sqrt(4 * (std::cbrt(4.) - 1))) *
                   ((1 - eps) / std::sqrt(eps * (2 - eps)));
    if (!(data->radius > 0) || !std::isfinite(data->radius))
        fail("pfsm.integral.data", "Unrepresentable integral radius", ErrorCode::numerical_failure);
    const auto count = std::min(options.fine_bins, effective);
    data->edges.reserve(count + 1);
    for (std::size_t j = 0; j <= count; ++j) {
        const double edge = std::lerp(o.coordinates.front(), o.coordinates.back(), double(j) / double(count));
        if (!data->edges.empty() && edge <= data->edges.back())
            fail("pfsm.integral.data", "Unrepresentable bin edges", ErrorCode::numerical_failure);
        data->edges.push_back(edge);
    }
    data->fine.weights.assign(count, 0); data->fine.net.assign(count, 0);
    data->coarse.weights.assign((count + 1) / 2, 0); data->coarse.net.assign((count + 1) / 2, 0);
    data->overlaps.reserve(effective + count);
    for (std::size_t i = 0; i < n; ++i) {
        checkpoint(context);
        if (o.weights[i] == 0) continue;
        const double lo = i && !disconnected(i) ? std::midpoint(o.coordinates[i - 1], o.coordinates[i]) : o.coordinates[i];
        const double hi = i + 1 < n && !disconnected(i + 1) ? std::midpoint(o.coordinates[i], o.coordinates[i + 1]) : o.coordinates[i];
        if (!(hi > lo)) fail("pfsm.integral.cell", "Degenerate sampling cell", ErrorCode::numerical_failure);
        const double density = o.weights[i] / (hi - lo);
        if (!std::isfinite(density)) fail("pfsm.integral.cell", "Weight density overflow", ErrorCode::numerical_failure);
        if (data->regions.empty() || lo > data->regions.back().second || (i && disconnected(i)))
            data->regions.emplace_back(lo, hi);
        else data->regions.back().second = hi;
        auto bin = std::size_t(std::upper_bound(data->edges.begin(), data->edges.end(), lo) - data->edges.begin() - 1);
        bin = std::min(bin, count - 1);
        for (; bin < count && data->edges[bin] < hi; ++bin) {
            const double a = std::max(lo, data->edges[bin]), b = std::min(hi, data->edges[bin + 1]);
            if (!(b > a)) continue;
            data->overlaps.push_back({a, b, density, i, bin});
            const double w = density * (b - a), net = o.observed[i] - o.background[i];
            data->fine.weights[bin] += w; data->fine.net[bin] += w * net;
        }
    }
    for (std::size_t j = 0; j < count; ++j) {
        data->coarse.weights[j / 2] += data->fine.weights[j];
        data->coarse.net[j / 2] += data->fine.net[j];
    }
    finish(data->fine); finish(data->coarse);
    data->buffer_bytes = data->edges.capacity() * sizeof(double) +
        data->overlaps.capacity() * sizeof(PfsmIntegralOverlap) + data->regions.capacity() * sizeof(std::pair<double, double>) +
        (data->fine.weights.capacity() + data->fine.net.capacity() +
         data->coarse.weights.capacity() + data->coarse.net.capacity()) * sizeof(double);
    return Result<std::shared_ptr<const PfsmIntegralData>>{std::move(data)};
} catch (const Failure &e) { return Result<std::shared_ptr<const PfsmIntegralData>>{e.error}; }

Result<PfsmIntegralColumns> integratePfsmScreenTemplate(const PfsmIntegralData &data,
    std::span<const ProfileLine> lines, double shift, const ExecutionContext &context) try {
    checkpoint(context);
    if (!std::isfinite(shift) || data.edges.size() < 2 || !(data.width > 0) || !std::isfinite(data.width) ||
        data.fine.weights.size() + 1 != data.edges.size() ||
        data.coarse.weights.size() != (data.fine.weights.size() + 1) / 2)
        fail("pfsm.integral.template", "Invalid integral template input");
    PfsmIntegralColumns out;
    out.fine.assign(data.fine.weights.size(), 0); out.coarse.assign(data.coarse.weights.size(), 0);
    const double a = data.width / std::sqrt(4 * (std::cbrt(4.) - 1));
    for (const auto &line : lines) {
        checkpoint(context);
        if (!std::isfinite(line.position) || !std::isfinite(line.intensity) || line.intensity < 0)
            fail("pfsm.integral.template", "Invalid expanded reference line");
        const double center = line.position + shift, lower = center - data.radius, upper = center + data.radius;
        if (!std::isfinite(center) || !std::isfinite(lower) || !std::isfinite(upper))
            fail("pfsm.integral.template", "Integral window overflow", ErrorCode::numerical_failure);
        auto it = std::lower_bound(data.overlaps.begin(), data.overlaps.end(), lower,
            [](const auto &cell, double x) { return cell.upper <= x; });
        for (; it != data.overlaps.end() && it->lower < upper; ++it) {
            checkpoint(context);
            const double lo = std::max(it->lower - center, -data.radius);
            const double hi = std::min(it->upper - center, data.radius);
            if (hi > lo) out.fine[it->bin] += line.intensity * it->density * mass(lo, hi, a);
        }
    }
    for (std::size_t j = 0; j < out.fine.size(); ++j) {
        out.coarse[j / 2] += out.fine[j];
        if (data.fine.weights[j] > 0) out.fine[j] /= data.fine.weights[j];
        if (!std::isfinite(out.fine[j]) || out.fine[j] < 0)
            fail("pfsm.integral.template", "Integral template overflow", ErrorCode::numerical_failure);
    }
    for (std::size_t j = 0; j < out.coarse.size(); ++j) {
        if (data.coarse.weights[j] > 0) out.coarse[j] /= data.coarse.weights[j];
        if (!std::isfinite(out.coarse[j]) || out.coarse[j] < 0)
            fail("pfsm.integral.template", "Integral template overflow", ErrorCode::numerical_failure);
    }
    return Result<PfsmIntegralColumns>{std::move(out)};
} catch (const Failure &e) { return Result<PfsmIntegralColumns>{e.error}; }
} // namespace qualpf::profile
