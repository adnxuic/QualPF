#include "qualpf/crystallography/powder_intensity.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
using namespace qualpf;
using namespace qualpf::crystallography;
namespace {
double number() {
  std::string s;
  std::cin >> s;
  return std::stod(s);
}
void error(int id, const Error &e) {
  std::cout << "ERROR " << id << ' ' << e.field << ' ' << e.reason << '\n';
}
} // namespace
int main() {
  std::cout << std::setprecision(17);
  int cid, rad, sync;
  std::size_t n;
  while (std::cin >> cid >> rad >> sync) {
    PowderIntensityOptions options;
    options.radiation.kind = static_cast<RadiationKind>(rad);
    options.synchrotron = sync != 0;
    options.radiation.wavelength = WavelengthAngstrom{number()};
    options.overlap.tolerance_degrees = number();
    std::cin >> n;
    std::vector<PowderReflection> refs;
    std::optional<Error> failure;
    for (std::size_t i = 0; i < n; ++i) {
      ReflectionIdentity identity;
      Reflection ref{{0, 0, 0}, DSpacingAngstrom{0}, TwoThetaDegrees{0}, 0};
      std::cin >> identity >> ref.hkl.h >> ref.hkl.k >> ref.hkl.l >>
          ref.multiplicity;
      ref.d = DSpacingAngstrom{number()};
      ref.two_theta = TwoThetaDegrees{number()};
      auto r = powder_reflection(identity, ref, number(), options);
      if (r)
        refs.push_back(r.value());
      else if (!failure)
        failure = r.error();
    }
    if (failure) {
      error(cid, *failure);
      continue;
    }
    auto merged = merge_powder_reflections(refs, options.overlap);
    if (!merged) {
      error(cid, merged.error());
      continue;
    }
    auto normalized = normalize_powder_lines(merged.value());
    if (!normalized) {
      error(cid, normalized.error());
      continue;
    }
    std::cout << "CASE " << cid << ' ' << rad << ' ' << sync << ' ' << n << ' '
              << options.radiation.wavelength.value << ' '
              << options.overlap.tolerance_degrees << '\n';
    for (const auto &r : refs) {
      const auto &h = r.reflection.hkl;
      std::cout << "REF " << r.identity << ' ' << h.h << ' ' << h.k << ' '
                << h.l << ' ' << r.reflection.multiplicity << ' '
                << r.reflection.d.value << ' ' << r.reflection.two_theta.value
                << ' ' << r.fc_squared << ' ' << r.correction.lorentz << ' '
                << r.correction.polarization << ' ' << r.correction.combined
                << ' ' << r.raw_intensity << ' ' << std::sqrt(r.fc_squared)
                << '\n';
    }
    std::size_t index = 0;
    for (const auto &line : normalized.value().lines()) {
      ++index;
      std::cout << "MEMBERS " << index << ' ' << line.representative_identity
                << ' ' << line.members.size();
      for (const auto &r : line.members)
        std::cout << ' ' << r.identity;
      std::cout << '\n';
      std::cout << "LINE " << index << ' ' << line.representative_identity
                << ' ' << line.representative.d.value << ' '
                << line.representative.two_theta.value << ' '
                << line.raw_intensity << ' ' << line.relative_intensity << '\n';
    }
  }
  return std::cin.eof() ? 0 : 1;
}
