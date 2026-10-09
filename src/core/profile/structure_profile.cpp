#include "qualpf/profile/structure_profile.h"
#include "qualpf/crystallography/powder_intensity.h"
#include <cmath>

namespace qualpf::profile {
using namespace crystallography;
Result<ProfileCrystal>
ProfileCrystal::isotropic_view(CrystalStructure source,
                               std::span<const double> biso) {
  if (biso.size() != source.sites().size())
    return Result<ProfileCrystal>(
        Error{ErrorCode::invalid_input, "profile.isotropic_b",
              "One explicit B per site is required", "size_mismatch"});
  std::vector<AtomicSite> sites(source.sites().begin(), source.sites().end());
  for (std::size_t i = 0; i < sites.size(); ++i) {
    auto displacement = Displacement::isotropic_b(biso[i]);
    if (!displacement)
      return Result<ProfileCrystal>(displacement.error());
    sites[i].displacement = displacement.value();
  }
  auto view =
      CrystalStructure::create(source.cell(), source.group(), std::move(sites));
  if (!view)
    return Result<ProfileCrystal>(view.error());
  return Result<ProfileCrystal>(
      ProfileCrystal(std::move(source), std::move(view).value()));
}
Result<ProfileStructurePhase>
structure_profile_phase(const ProfileCrystal &crystal, const ProfileGrid &grid,
                        const ProfileRadiation &radiation,
                        ProfileParameters parameters, bool anomalous) {
  if (radiation.components.empty() || radiation.components.size() > 2)
    return Result<ProfileStructurePhase>(Error{
        ErrorCode::invalid_input, "profile.radiation",
        "One or two components are supported", "unsupported_component_count"});
  for (auto c : radiation.components)
    if (!std::isfinite(c.wavelength_angstrom) || c.wavelength_angstrom <= 0 ||
        !std::isfinite(c.intensity_ratio) || c.intensity_ratio < 0)
      return Result<ProfileStructurePhase>(
          Error{ErrorCode::invalid_input, "profile.radiation",
                "Invalid component", "invalid_radiation"});
  const auto &structure = crystal.calculation();
  ScatteringRadiation scatter{
      RadiationKind::xray,
      WavelengthAngstrom{radiation.components[0].wavelength_angstrom},
      anomalous};
  ReflectionGenerationOptions options;
  options.wavelength = scatter.wavelength;
  options.minimum_two_theta = TwoThetaDegrees{grid.points().front()};
  options.maximum_two_theta = TwoThetaDegrees{grid.points().back()};
  options.friedel = FriedelPolicy::merge;
  auto d = d_spacing(options.maximum_two_theta, options.wavelength);
  if (!d)
    return Result<ProfileStructurePhase>(d.error());
  options.minimum_d = d.value();
  auto refs =
      generate_reflections(structure.cell(), structure.group(), options);
  if (!refs)
    return Result<ProfileStructurePhase>(refs.error());
  if (refs.value().reflections().empty())
    return Result<ProfileStructurePhase>(
        Error{ErrorCode::missing_data, "profile.reflections",
              "No reflections in requested center range", "empty_reflections"});
  auto factors = structure_factors(structure, refs.value(), scatter);
  if (!factors)
    return Result<ProfileStructurePhase>(factors.error());
  ProfileStructurePhase result;
  result.phase.parameters = parameters;
  PowderIntensityOptions lp_options;
  lp_options.radiation = scatter;
  lp_options.synchrotron = false;
  for (std::size_t i = 0; i < refs.value().reflections().size(); ++i) {
    const auto &ref = refs.value().reflections()[i];
    ProfileReflection r{
        i, factors.value()[i].fc_squared, ref.multiplicity, 1, 20, 1, {}};
    for (auto component : radiation.components) {
      auto center =
          two_theta(ref.d, WavelengthAngstrom{component.wavelength_angstrom});
      if (!center)
        return Result<ProfileStructurePhase>(center.error());
      auto lp = powder_correction(center.value(), lp_options);
      if (!lp)
        return Result<ProfileStructurePhase>(lp.error());
      r.components.push_back({center.value().value, lp.value().combined, 1});
    }
    result.reflections.push_back(ref);
    result.phase.reflections.push_back(std::move(r));
  }
  return Result<ProfileStructurePhase>(std::move(result));
}
} // namespace qualpf::profile
