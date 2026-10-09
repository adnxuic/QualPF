#include "qualpf/crystallography/composition.h"
#include "qualpf/profile/structure_profile.h"
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::profile;
using namespace qualpf::crystallography;
template <class T> T take(Result<T> r) {
  if (!r)
    throw std::runtime_error(r.error().field + "/" + r.error().reason);
  return std::move(r).value();
}
void array(const char *tag, std::span<const double> a) {
  std::cout << tag;
  for (double v : a)
    std::cout << ' ' << v;
  std::cout << '\n';
}
int main() {
  std::cout << std::setprecision(17);
  int id, model, np, nw;
  try {
    while (std::cin >> id >> model >> np >> nw) {
      ProfileRange range;
      ProfileDisplay display;
      double width;
      std::cin >> range.start >> range.end >> range.step >> width >>
          display.scale >> display.background;
      auto grid = take(ProfileGrid::regular(range));
      ProfileRadiation rad;
      for (int k = 0; k < nw; ++k) {
        RadiationComponent c;
        std::cin >> c.wavelength_angstrom >> c.intensity_ratio;
        rad.components.push_back(c);
      }
      std::cout << "CASE " << id << '\n';
      array("GRID", grid.points());
      std::vector<ProfilePhase> phases;
      for (int p = 0; p < np; ++p) {
        int number, setting, na;
        double weight;
        std::cin >> number >> setting >> na >> weight;
        UnitCellParameters cellp;
        for (double &v : cellp.lengths_angstrom)
          std::cin >> v;
        for (double &v : cellp.angles_degrees)
          std::cin >> v;
        auto cell = take(UnitCell::create(cellp));
        auto group = take(SpaceGroupRegistry{}.by_number(number, setting));
        std::vector<AtomicSite> sites;
        std::vector<double> biso;
        for (int i = 0; i < na; ++i) {
          std::string label;
          std::array<double, 3> xyz;
          double occ;
          int adp;
          DisplacementTensor tensor;
          std::cin >> label;
          for (double &v : xyz)
            std::cin >> v;
          std::cin >> occ >> adp;
          for (double &v : tensor)
            std::cin >> v;
          auto species = take(ScatteringRegistry{}.find(label));
          auto pos = take(FractionalPosition::create(xyz));
          auto occupancy = take(Occupancy::create(occ));
          auto displacement =
              take(adp == 0   ? Displacement::isotropic_b(tensor[0])
                   : adp == 1 ? Displacement::isotropic_u(tensor[0])
                   : adp == 2 ? Displacement::anisotropic_u(tensor)
                              : Displacement::anisotropic_beta(tensor));
          sites.push_back({species, pos, occupancy, displacement});
          // Explicit observed native atom.biso field; source tensor is
          // retained.
          biso.push_back(adp == 0   ? tensor[0]
                         : adp == 1 ? 8 * std::numbers::pi * std::numbers::pi *
                                          tensor[0]
                                    : 0);
        }
        auto source =
            take(CrystalStructure::create(cell, group, std::move(sites)));
        auto wrong_b = biso;
        wrong_b.push_back(0);
        if (ProfileCrystal::isotropic_view(source, wrong_b))
          throw std::runtime_error("accepted wrong B count");
        wrong_b = biso;
        wrong_b[0] = -1;
        if (ProfileCrystal::isotropic_view(source, wrong_b))
          throw std::runtime_error("accepted negative B");
        auto crystal = take(ProfileCrystal::isotropic_view(source, biso));
        for (std::size_t i = 0; i < source.sites().size(); ++i)
          if (take(source.sites()[i].displacement.beta(cell)) !=
              take(crystal.source().sites()[i].displacement.beta(cell)))
            throw std::runtime_error("source displacement changed");
        ProfileParameters parameters;
        parameters.model = static_cast<PeakModel>(model);
        parameters.width.w = width * width;
        auto generated =
            take(structure_profile_phase(crystal, grid, rad, parameters));
        for (std::size_t i = 0; i < generated.reflections.size(); ++i) {
          auto r = generated.reflections[i];
          const auto &pr = generated.phase.reflections[i];
          std::cout << "REF " << p + 1 << ' ' << i + 1 << ' ' << r.hkl.h << ' '
                    << r.hkl.k << ' ' << r.hkl.l << ' ' << r.multiplicity << ' '
                    << r.d.value << ' ' << pr.fc_squared;
          for (auto component : pr.components)
            std::cout << ' ' << component.center_degrees << ' ' << component.lp;
          std::cout << '\n';
        }
        if (weight >= 0) {
          auto composition = take(CrystalComposition::calculate(source));
          auto density = take(crystal_density(composition.cell_mass_daltons(),
                                              cell.volume_angstrom_cubed()));
          generated.phase.scale = take(density_phase_scale(
              weight, density.grams_per_cm3, cell.volume_angstrom_cubed()));
          std::cout << "DENSITY " << p + 1 << ' ' << density.grams_per_cm3
                    << ' ' << cell.volume_angstrom_cubed() << '\n';
        }
        std::cout << "SCALE " << p + 1 << ' ' << generated.phase.scale << '\n';
        array("RAW", take(phase_profile(grid, generated.phase, rad)));
        phases.push_back(std::move(generated.phase));
      }
      auto profile = take(powder_profile(grid, rad, phases, display));
      array("WEIGHTED", profile.weighted_intensity);
      array("FINAL", profile.calculated_intensity);
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
