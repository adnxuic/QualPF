#include "qualpf/crystallography/powder_intensity.h"
#include "phase_sincos.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>

namespace qualpf::crystallography {
namespace {
Error invalid(const char *field, const char *reason) {
  return {ErrorCode::invalid_input, field, reason, reason};
}
Error numerical(const char *field, const char *reason) {
  return {ErrorCode::numerical_failure, field, reason, reason};
}
Result<bool> valid_reflection(const Reflection &r) {
  if (r.hkl.h == 0 && r.hkl.k == 0 && r.hkl.l == 0)
    return Result<bool>(invalid("powder.reflection.hkl", "zero_hkl"));
  if (!std::isfinite(r.d.value) || r.d.value <= 0)
    return Result<bool>(invalid("powder.reflection.d", "invalid_d_spacing"));
  if (r.multiplicity == 0)
    return Result<bool>(
        invalid("powder.reflection.multiplicity", "invalid_multiplicity"));
  if (!std::isfinite(r.two_theta.value) || r.two_theta.value < 0 ||
      r.two_theta.value > 180)
    return Result<bool>(
        invalid("powder.reflection.two_theta", "invalid_two_theta"));
  if (r.two_theta.value == 0 || r.two_theta.value == 180)
    return Result<bool>(
        invalid("powder.reflection.two_theta", "lp_singularity"));
  return Result<bool>(true);
}
bool same_reflection(const Reflection &a, const Reflection &b) {
  return equal_hkl(a.hkl, b.hkl) && a.d.value == b.d.value &&
         a.two_theta.value == b.two_theta.value &&
         a.multiplicity == b.multiplicity;
}
bool same_radiation(ScatteringRadiation a, ScatteringRadiation b) {
  return a.kind == b.kind && a.wavelength.value == b.wavelength.value &&
         a.anomalous == b.anomalous;
}
bool reflection_less(const PowderReflection &a, const PowderReflection &b) {
  if (a.reflection.two_theta.value != b.reflection.two_theta.value)
    return a.reflection.two_theta.value < b.reflection.two_theta.value;
  return a.identity < b.identity;
}
} // namespace

Result<PowderStructureFactors>
PowderStructureFactors::calculate(const CrystalStructure &structure,
                                  const ReflectionSet &reflections,
                                  ScatteringRadiation radiation) {
  // Existing validation and mathematics remain the single source of Fc2.
  auto computed = structure_factors(structure, reflections, radiation);
  if (!computed)
    return Result<PowderStructureFactors>(computed.error());
  if (reflections.reflections().empty())
    return Result<PowderStructureFactors>(
        invalid("powder.reflections", "empty_reflection_set"));
  std::vector<PowderStructureFactor> bound;
  bound.reserve(computed.value().size());
  for (std::size_t i = 0; i < computed.value().size(); ++i)
    bound.push_back({0, reflections.reflections()[i], computed.value()[i]});
  std::sort(bound.begin(), bound.end(), [](const auto &a, const auto &b) {
    return HklLess{}(a.reflection.hkl, b.reflection.hkl);
  });
  for (std::size_t i = 0; i < bound.size(); ++i)
    bound[i].identity = i + 1;
  return Result<PowderStructureFactors>(PowderStructureFactors(
      structure.cell().parameters(), structure.group().setting().id, radiation,
      reflections.options().friedel, std::move(bound)));
}

Result<PowderCorrection>
powder_correction(TwoThetaDegrees angle,
                  const PowderIntensityOptions &options) {
  auto radiation = validate_radiation(options.radiation);
  if (!radiation)
    return Result<PowderCorrection>(radiation.error());
  if (!std::isfinite(angle.value) || angle.value < 0 || angle.value > 180)
    return Result<PowderCorrection>(
        invalid("powder.reflection.two_theta", "invalid_two_theta"));
  if (angle.value == 0 || angle.value == 180)
    return Result<PowderCorrection>(
        invalid("powder.reflection.two_theta", "lp_singularity"));
  const double tth = angle.value * (std::numbers::pi_v<double> / 180.0);
  const double th = 0.5 * tth;
  const auto theta = detail::phase_phasor(th);
  const double sine = theta.imag(), cosine = theta.real();
  const double lorentz = 1.0 / (2.0 * sine * sine * cosine);
  const bool unpolarized =
      options.radiation.kind == RadiationKind::neutron || options.synchrotron;
  const double cos_tth = detail::phase_phasor(tth).real();
  const double polarization = unpolarized ? 1.0 : 1.0 + 0.8 * cos_tth * cos_tth;
  const double cosecant = 1.0 / sine, secant = 1.0 / cosine;
  // Retain the observed X-ray multiplication order; no float emulation.
  const double lp = unpolarized
                        ? lorentz
                        : (0.5 * polarization) * (cosecant * cosecant) * secant;
  if (!std::isfinite(lorentz) || !std::isfinite(polarization) ||
      !std::isfinite(lp) || lp <= 0)
    return Result<PowderCorrection>(
        numerical("powder.correction", "nonfinite_correction"));
  return Result<PowderCorrection>(PowderCorrection{lorentz, polarization, lp});
}

Result<PowderReflection>
powder_reflection(ReflectionIdentity identity, Reflection reflection,
                  double fc_squared, const PowderIntensityOptions &options) {
  auto valid = valid_reflection(reflection);
  if (!valid)
    return Result<PowderReflection>(valid.error());
  if (!std::isfinite(fc_squared) || fc_squared < 0)
    return Result<PowderReflection>(
        invalid("powder.reflection.fc_squared", "invalid_fc_squared"));
  auto correction = powder_correction(reflection.two_theta, options);
  if (!correction)
    return Result<PowderReflection>(correction.error());
  const double multiplier = static_cast<double>(reflection.multiplicity) *
                            correction.value().combined;
  if (!std::isfinite(multiplier))
    return Result<PowderReflection>(numerical("powder.reflection.raw_intensity",
                                              "nonfinite_raw_intensity"));
  const double raw = multiplier * fc_squared;
  if (!std::isfinite(raw))
    return Result<PowderReflection>(numerical("powder.reflection.raw_intensity",
                                              "nonfinite_raw_intensity"));
  return Result<PowderReflection>(PowderReflection{
      identity, reflection, fc_squared, correction.value(), raw});
}

Result<std::vector<PowderLine>>
merge_powder_reflections(std::span<const PowderReflection> reflections,
                         PowderOverlapOptions options) {
  if (!std::isfinite(options.tolerance_degrees) ||
      options.tolerance_degrees < 0)
    return Result<std::vector<PowderLine>>(
        invalid("powder.overlap.tolerance", "invalid_overlap_configuration"));
  if (reflections.empty())
    return Result<std::vector<PowderLine>>(
        invalid("powder.reflections", "empty_reflection_set"));
  std::set<ReflectionIdentity> identities;
  for (const auto &r : reflections) {
    auto valid = valid_reflection(r.reflection);
    if (!valid)
      return Result<std::vector<PowderLine>>(valid.error());
    if (!identities.insert(r.identity).second)
      return Result<std::vector<PowderLine>>(invalid(
          "powder.reflection.identity", "duplicate_reflection_identity"));
    if (!std::isfinite(r.raw_intensity) || r.raw_intensity < 0)
      return Result<std::vector<PowderLine>>(
          invalid("powder.reflection.raw_intensity", "invalid_raw_intensity"));
    if (!std::isfinite(r.fc_squared) || r.fc_squared < 0)
      return Result<std::vector<PowderLine>>(
          invalid("powder.reflection.fc_squared", "invalid_fc_squared"));
    if (!std::isfinite(r.correction.lorentz) ||
        !std::isfinite(r.correction.polarization) ||
        !std::isfinite(r.correction.combined) || r.correction.lorentz <= 0 ||
        r.correction.polarization <= 0 || r.correction.combined <= 0)
      return Result<std::vector<PowderLine>>(
          invalid("powder.correction", "invalid_correction"));
  }
  std::vector<PowderReflection> ordered(reflections.begin(), reflections.end());
  std::sort(ordered.begin(), ordered.end(), reflection_less);
  std::vector<PowderLine> lines;
  double previous_angle = 0;
  for (const auto &r : ordered) {
    if (lines.empty() || r.reflection.two_theta.value - previous_angle >
                             options.tolerance_degrees) {
      lines.push_back({r.identity, r.reflection, r.raw_intensity, 0, {r}});
    } else {
      auto &line = lines.back();
      line.raw_intensity += r.raw_intensity;
      if (!std::isfinite(line.raw_intensity))
        return Result<std::vector<PowderLine>>(
            numerical("powder.line.raw_intensity", "nonfinite_merged_result"));
      line.members.push_back(r);
    }
    previous_angle = r.reflection.two_theta.value;
  }
  return Result<std::vector<PowderLine>>(std::move(lines));
}

Result<PowderLineSet> normalize_powder_lines(std::vector<PowderLine> lines) {
  if (lines.empty())
    return Result<PowderLineSet>(invalid("powder.lines", "empty_line_set"));
  double maximum = 0;
  std::set<ReflectionIdentity> identities;
  for (const auto &line : lines) {
    if (!std::isfinite(line.raw_intensity) || line.raw_intensity < 0)
      return Result<PowderLineSet>(
          invalid("powder.line.raw_intensity", "invalid_raw_intensity"));
    auto valid = valid_reflection(line.representative);
    if (!valid)
      return Result<PowderLineSet>(valid.error());
    if (line.members.empty() ||
        line.representative_identity != line.members.front().identity ||
        !same_reflection(line.representative, line.members.front().reflection))
      return Result<PowderLineSet>(
          invalid("powder.line.members", "invalid_line_membership"));
    double sum = 0;
    for (std::size_t i = 0; i < line.members.size(); ++i) {
      const auto &member = line.members[i];
      auto member_valid = valid_reflection(member.reflection);
      if (!member_valid)
        return Result<PowderLineSet>(member_valid.error());
      if (!identities.insert(member.identity).second ||
          (i > 0 && reflection_less(member, line.members[i - 1])))
        return Result<PowderLineSet>(
            invalid("powder.line.members", "invalid_line_membership"));
      if (!std::isfinite(member.raw_intensity) || member.raw_intensity < 0)
        return Result<PowderLineSet>(invalid("powder.reflection.raw_intensity",
                                             "invalid_raw_intensity"));
      sum += member.raw_intensity;
    }
    if (!std::isfinite(sum))
      return Result<PowderLineSet>(
          numerical("powder.line.raw_intensity", "nonfinite_merged_result"));
    if (sum != line.raw_intensity)
      return Result<PowderLineSet>(
          invalid("powder.line.members", "invalid_line_membership"));
    maximum = std::max(maximum, line.raw_intensity);
  }
  if (maximum == 0)
    return Result<PowderLineSet>(
        invalid("powder.lines", "all_zero_normalization"));
  for (auto &line : lines)
    line.relative_intensity = 1000.0 * (line.raw_intensity / maximum);
  std::sort(lines.begin(), lines.end(), [](const auto &a, const auto &b) {
    if (a.representative.two_theta.value != b.representative.two_theta.value)
      return a.representative.two_theta.value <
             b.representative.two_theta.value;
    return a.representative_identity < b.representative_identity;
  });
  return Result<PowderLineSet>(PowderLineSet(std::move(lines)));
}

Result<PowderLineSet> powder_lines(const ReflectionSet &reflections,
                                   const PowderStructureFactors &factors,
                                   const PowderIntensityOptions &options) {
  auto radiation = validate_radiation(options.radiation);
  if (!radiation)
    return Result<PowderLineSet>(radiation.error());
  if (!same_radiation(options.radiation, factors.radiation()) ||
      reflections.options().wavelength.value !=
          factors.radiation().wavelength.value)
    return Result<PowderLineSet>(
        invalid("powder.radiation", "incompatible_powder_radiation"));
  if (reflections.reflections().size() != factors.values().size() ||
      reflections.options().friedel != factors.friedel_policy())
    return Result<PowderLineSet>(
        invalid("powder.reflections", "incompatible_reflection_set"));
  std::vector<Reflection> ordered(reflections.reflections().begin(),
                                  reflections.reflections().end());
  std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
    return HklLess{}(a.hkl, b.hkl);
  });
  std::vector<PowderReflection> intensities;
  intensities.reserve(ordered.size());
  for (std::size_t i = 0; i < ordered.size(); ++i) {
    const auto &f = factors.values()[i];
    if (!same_reflection(ordered[i], f.reflection))
      return Result<PowderLineSet>(
          invalid("powder.reflections", "incompatible_reflection_set"));
    auto r = powder_reflection(f.identity, f.reflection, f.factor.fc_squared,
                               options);
    if (!r)
      return Result<PowderLineSet>(r.error());
    intensities.push_back(r.value());
  }
  auto merged = merge_powder_reflections(intensities, options.overlap);
  if (!merged)
    return Result<PowderLineSet>(merged.error());
  return normalize_powder_lines(std::move(merged.value()));
}

Result<PowderLineSet> powder_lines(const CrystalStructure &structure,
                                   const ReflectionSet &reflections,
                                   const PowderIntensityOptions &options) {
  auto factors = PowderStructureFactors::calculate(structure, reflections,
                                                   options.radiation);
  if (!factors)
    return Result<PowderLineSet>(factors.error());
  return powder_lines(reflections, factors.value(), options);
}
} // namespace qualpf::crystallography
