#pragma once
#include "qualpf/crystallography/structure_factor.h"
#include "qualpf/profile/powder_profile.h"

namespace qualpf::profile {

// Legacy's generator evaluates the isotropic B field even when a source tensor
// is present. Its value must be supplied explicitly; no tensor-to-Ueq fallback
// or default-ADP policy is inferred here. Source and calculation views coexist.
class ProfileCrystal {
public:
  static Result<ProfileCrystal>
  isotropic_view(crystallography::CrystalStructure source,
                 std::span<const double> biso);
  const crystallography::CrystalStructure &source() const noexcept {
    return source_;
  }
  const crystallography::CrystalStructure &calculation() const noexcept {
    return calculation_;
  }

private:
  ProfileCrystal(crystallography::CrystalStructure source,
                 crystallography::CrystalStructure calculation)
      : source_(std::move(source)), calculation_(std::move(calculation)) {}
  crystallography::CrystalStructure source_, calculation_;
};
struct ProfileStructurePhase {
  ProfilePhase phase;
  std::vector<crystallography::Reflection>
      reflections; // ungrouped geometry/identity
};

// First wavelength enumerates inclusive center range; all components share its
// Fc² and multiplicity. LP is the generator's hardcoded X-ray convention.
// No preferred-orientation/absorption model is implicitly activated.
Result<ProfileStructurePhase>
structure_profile_phase(const ProfileCrystal &crystal, const ProfileGrid &grid,
                        const ProfileRadiation &radiation,
                        ProfileParameters parameters = {},
                        bool anomalous = false);

} // namespace qualpf::profile
