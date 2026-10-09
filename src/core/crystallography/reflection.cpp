#include "qualpf/crystallography/reflection.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>

namespace qualpf::crystallography {
namespace {
Error invalid(const char *field, const char *reason) {
    return {ErrorCode::invalid_input, field, "Invalid reflection input", reason};
}
bool zero(Hkl h) { return h.h == 0 && h.k == 0 && h.l == 0; }
bool valid(FriedelPolicy p) { return p == FriedelPolicy::separate || p == FriedelPolicy::merge; }
bool positive(double v) { return std::isfinite(v) && v > 0; }
bool representable(std::int64_t v) {
    return v >= std::numeric_limits<std::int32_t>::min() &&
           v <= std::numeric_limits<std::int32_t>::max();
}

bool compatible(const UnitCell &cell, const SpaceGroup &group) {
    const auto &g = cell.metric().angstrom_squared;
    const auto &lengths = cell.parameters().lengths_angstrom;
    for (const auto &op : group.operations()) {
        const auto &r = op.rotation();
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                // Normalize before summing so huge valid metrics cannot overflow.
                const double scale = lengths[i] * lengths[j];
                double transformed = 0;
                for (int a = 0; a < 3; ++a)
                    for (int b = 0; b < 3; ++b)
                        transformed += r[3 * a + i] * (g[3 * a + b] / scale) * r[3 * b + j];
                if (!std::isfinite(transformed) ||
                    std::abs(transformed - g[3 * i + j] / scale) > 1e-10)
                    return false;
            }
    }
    return true;
}
} // namespace

bool equal_hkl(Hkl a, Hkl b) noexcept { return a.h == b.h && a.k == b.k && a.l == b.l; }
bool HklLess::operator()(Hkl a, Hkl b) const noexcept {
    return std::tie(a.h, a.k, a.l) < std::tie(b.h, b.k, b.l);
}

Result<Hkl> reciprocal_image(Hkl h, const SymmetryOperation &operation) {
    const auto &r = operation.rotation();
    const std::int64_t v[3]{h.h, h.k, h.l};
    std::int32_t image[3]{};
    for (int j = 0; j < 3; ++j) {
        std::int64_t sum = 0; // 3 * int32 * bounded rotation fits int64.
        for (int i = 0; i < 3; ++i)
            sum += v[i] * r[3 * i + j];
        if (!representable(sum))
            return Result<Hkl>(invalid("reflection.hkl", "index_overflow"));
        image[j] = static_cast<std::int32_t>(sum);
    }
    return Result<Hkl>(Hkl{image[0], image[1], image[2]});
}

Result<bool> systematically_absent(Hkl h, const SpaceGroup &group) {
    if (zero(h))
        return Result<bool>(invalid("reflection.hkl", "zero_hkl"));
    bool absent = false;
    for (const auto &op : group.operations()) {
        const auto image = reciprocal_image(h, op);
        if (!image)
            return Result<bool>(image.error());
        if (equal_hkl(image.value(), h)) {
            const auto &t = op.translation_twelfths();
            const std::int64_t phase =
                std::int64_t(h.h) * t[0] + std::int64_t(h.k) * t[1] + std::int64_t(h.l) * t[2];
            absent = absent || phase % SymmetryOperation::translation_denominator != 0;
        }
    }
    return Result<bool>(absent);
}

Result<ReflectionClass> reflection_class(Hkl h, const SpaceGroup &group, FriedelPolicy policy) {
    if (zero(h))
        return Result<ReflectionClass>(invalid("reflection.hkl", "zero_hkl"));
    if (!valid(policy))
        return Result<ReflectionClass>(invalid("reflection.friedel", "invalid_policy"));
    std::vector<Hkl> members;
    members.reserve(group.operations().size() * (policy == FriedelPolicy::merge ? 2 : 1));
    for (const auto &op : group.operations()) {
        auto image = reciprocal_image(h, op);
        if (!image)
            return Result<ReflectionClass>(image.error());
        const auto v = image.value();
        members.push_back(v);
        if (policy == FriedelPolicy::merge) {
            if (!representable(-std::int64_t(v.h)) || !representable(-std::int64_t(v.k)) ||
                !representable(-std::int64_t(v.l)))
                return Result<ReflectionClass>(invalid("reflection.hkl", "index_overflow"));
            members.push_back({-v.h, -v.k, -v.l});
        }
    }
    std::sort(members.begin(), members.end(), HklLess{});
    members.erase(std::unique(members.begin(), members.end(), equal_hkl), members.end());
    return Result<ReflectionClass>(ReflectionClass(std::move(members), policy));
}

Result<ReflectionSet> generate_reflections(const UnitCell &cell, const SpaceGroup &group,
                                           ReflectionGenerationOptions o) {
    if (!positive(o.wavelength.value))
        return Result<ReflectionSet>(invalid("reflection.wavelength", "nonpositive_or_nonfinite"));
    if (!positive(o.minimum_d.value) || (o.maximum_d && !positive(o.maximum_d->value)))
        return Result<ReflectionSet>(invalid("reflection.d_range", "nonpositive_or_nonfinite"));
    if (o.maximum_d && o.maximum_d->value < o.minimum_d.value)
        return Result<ReflectionSet>(invalid("reflection.d_range", "reversed_range"));
    const double amin = o.minimum_two_theta.value, amax = o.maximum_two_theta.value;
    if (!std::isfinite(amin) || !std::isfinite(amax) || amin < 0 || amax <= 0 || amax > 180)
        return Result<ReflectionSet>(invalid("reflection.angle_range", "invalid_angle"));
    if (amin > amax)
        return Result<ReflectionSet>(invalid("reflection.angle_range", "reversed_range"));
    if (!valid(o.friedel))
        return Result<ReflectionSet>(invalid("reflection.friedel", "invalid_policy"));
    if (o.order != ReflectionOrder::hkl && o.order != ReflectionOrder::two_theta)
        return Result<ReflectionSet>(invalid("reflection.order", "invalid_policy"));
    if (o.maximum_candidates == 0)
        return Result<ReflectionSet>(invalid("reflection.maximum_candidates", "zero_budget"));
    if (!compatible(cell, group))
        return Result<ReflectionSet>(invalid("reflection.cell", "incompatible_symmetry"));
    auto angular_d = d_spacing(o.maximum_two_theta, o.wavelength);
    if (!angular_d)
        return Result<ReflectionSet>(angular_d.error());
    const double minimum = std::max(o.minimum_d.value, angular_d.value().value);
    if (o.maximum_d && o.maximum_d->value < minimum)
        return Result<ReflectionSet>(ReflectionSet({}, o));
    std::int64_t bounds[3]{};
    std::size_t candidates = 1;
    for (int i = 0; i < 3; ++i) {
        const double bound =
            std::ceil(std::nextafter(cell.parameters().lengths_angstrom[i] / minimum,
                                     std::numeric_limits<double>::infinity()));
        if (!std::isfinite(bound) || bound > std::numeric_limits<std::int32_t>::max())
            return Result<ReflectionSet>(invalid("reflection.bounds", "index_overflow"));
        bounds[i] = static_cast<std::int64_t>(bound);
        const auto width = static_cast<std::uint64_t>(2 * bounds[i] + 1);
        if (width > o.maximum_candidates / candidates)
            return Result<ReflectionSet>(
                invalid("reflection.maximum_candidates", "candidate_budget_exceeded"));
        candidates *= static_cast<std::size_t>(width);
    }
    std::vector<Reflection> output;
    for (std::int64_t h = -bounds[0]; h <= bounds[0]; ++h)
        for (std::int64_t k = -bounds[1]; k <= bounds[1]; ++k)
            for (std::int64_t l = -bounds[2]; l <= bounds[2]; ++l) {
                const Hkl index{static_cast<std::int32_t>(h), static_cast<std::int32_t>(k),
                                static_cast<std::int32_t>(l)};
                if (zero(index))
                    continue;
                const auto d = cell.d_spacing(index);
                if (!d)
                    return Result<ReflectionSet>(d.error());
                if (d.value().value < o.minimum_d.value ||
                    (o.maximum_d && d.value().value > o.maximum_d->value) ||
                    d.value().value < o.wavelength.value / 2)
                    continue;
                const auto angle = two_theta(d.value(), o.wavelength);
                if (!angle)
                    return Result<ReflectionSet>(angle.error());
                if (angle.value().value < amin || angle.value().value > amax)
                    continue;
                const auto orbit = reflection_class(index, group, o.friedel);
                if (!orbit)
                    return Result<ReflectionSet>(orbit.error());
                if (!equal_hkl(index, orbit.value().canonical()))
                    continue;
                const auto absent = systematically_absent(index, group);
                if (!absent)
                    return Result<ReflectionSet>(absent.error());
                if (!absent.value())
                    output.push_back(
                        {index, d.value(), angle.value(), orbit.value().multiplicity()});
            }
    std::sort(output.begin(), output.end(),
              [order = o.order](const Reflection &a, const Reflection &b) {
                  if (order == ReflectionOrder::two_theta && a.two_theta != b.two_theta)
                      return a.two_theta.value < b.two_theta.value;
                  return HklLess{}(a.hkl, b.hkl);
              });
    return Result<ReflectionSet>(ReflectionSet(std::move(output), o));
}
} // namespace qualpf::crystallography
