#pragma once

#include "qualpf/crystallography/symmetry.h"
#include "qualpf/crystallography/unit_cell.h"
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace qualpf::crystallography {

[[nodiscard]] bool equal_hkl(Hkl a, Hkl b) noexcept;
struct HklLess {
    [[nodiscard]] bool operator()(Hkl a, Hkl b) const noexcept;
};

// Symmetry-only includes inversion exactly when it belongs to the space group.
// Powder Friedel equivalence additionally identifies opposite reciprocal vectors.
enum class FriedelPolicy { separate, merge };
enum class ReflectionOrder { two_theta, hkl };

// Direct fractional operation is R*x+t. Reciprocal ROW action here is h*R,
// equivalently R^T*h for a column h. Translation is irrelevant to the image.
[[nodiscard]] Result<Hkl> reciprocal_image(Hkl h, const SymmetryOperation &operation);
[[nodiscard]] Result<bool> systematically_absent(Hkl h, const SpaceGroup &group);

class ReflectionClass {
  public:
    [[nodiscard]] Hkl canonical() const noexcept { return members_.back(); }
    [[nodiscard]] std::span<const Hkl> members() const noexcept { return members_; }
    [[nodiscard]] std::size_t multiplicity() const noexcept { return members_.size(); }
    [[nodiscard]] FriedelPolicy friedel_policy() const noexcept { return policy_; }

  private:
    friend Result<ReflectionClass> reflection_class(Hkl, const SpaceGroup &, FriedelPolicy);
    ReflectionClass(std::vector<Hkl> members, FriedelPolicy policy)
        : members_(std::move(members)), policy_(policy) {}
    std::vector<Hkl> members_; // sorted, unique, nonempty; no zero member
    FriedelPolicy policy_;
};

[[nodiscard]] Result<ReflectionClass> reflection_class(Hkl h, const SpaceGroup &group,
                                                       FriedelPolicy policy);

struct Reflection {
    Hkl hkl;
    DSpacingAngstrom d;
    TwoThetaDegrees two_theta;
    std::size_t multiplicity;
};

struct ReflectionGenerationOptions {
    WavelengthAngstrom wavelength{1.5406};
    DSpacingAngstrom minimum_d{1.0};
    std::optional<DSpacingAngstrom> maximum_d;
    TwoThetaDegrees minimum_two_theta{0.0}; // zero means no lower angular cutoff
    TwoThetaDegrees maximum_two_theta{180.0};
    FriedelPolicy friedel = FriedelPolicy::merge; // explicit powder contract
    ReflectionOrder order = ReflectionOrder::two_theta;
    std::size_t maximum_candidates = 5'000'000; // whole box, including zero
};

class ReflectionSet {
  public:
    [[nodiscard]] std::span<const Reflection> reflections() const noexcept { return values_; }
    [[nodiscard]] const ReflectionGenerationOptions &options() const noexcept { return options_; }

  private:
    friend Result<ReflectionSet> generate_reflections(const UnitCell &, const SpaceGroup &,
                                                      ReflectionGenerationOptions);
    ReflectionSet(std::vector<Reflection> values, ReflectionGenerationOptions options)
        : values_(std::move(values)), options_(options) {}
    std::vector<Reflection> values_;
    ReflectionGenerationOptions options_;
};

// Closed binary64 ranges. No intensity, mutable global cache or partial result.
// Incompatible cell symmetry and impractical index boxes return stable errors.
[[nodiscard]] Result<ReflectionSet> generate_reflections(const UnitCell &cell,
                                                         const SpaceGroup &group,
                                                         ReflectionGenerationOptions options);

} // namespace qualpf::crystallography
