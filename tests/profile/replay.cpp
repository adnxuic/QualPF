#include "qualpf/profile/powder_profile.h"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace qualpf::profile;
template <class T> T take(qualpf::Result<T> r) {
  if (!r)
    throw std::runtime_error(r.error().field + "/" + r.error().reason);
  return std::move(r).value();
}
void array(const char *tag, std::span<const double> a) {
  std::cout << tag;
  for (double x : a)
    std::cout << ' ' << x;
  std::cout << '\n';
}
int main() {
  std::cout << std::setprecision(17);
  int id, model;
  std::size_t nx, nw, np;
  try {
    while (std::cin >> id >> model >> nx >> nw >> np) {
      ProfileDisplay display;
      std::cin >> display.scale >> display.background;
      ProfileParameters parameters;
      parameters.model = static_cast<PeakModel>(model);
      auto &w = parameters.width;
      auto &s = parameters.shape;
      std::cin >> w.u >> w.v >> w.w >> w.z >> w.x >> w.y >> s.beta0 >>
          s.beta1 >> s.beta2 >> s.eta0 >> s.eta1 >> s.eta2;
      for (auto &v : s.asymmetry)
        std::cin >> v;
      ProfileRadiation radiation;
      for (std::size_t k = 0; k < nw; ++k) {
        RadiationComponent c;
        std::cin >> c.wavelength_angstrom >> c.intensity_ratio;
        radiation.components.push_back(c);
      }
      std::vector<double> x(nx);
      for (double &v : x)
        std::cin >> v;
      auto grid = take(ProfileGrid::coordinates(x));
      std::vector<ProfilePhase> phases(np);
      for (auto &phase : phases) {
        phase.parameters = parameters;
        std::size_t nr;
        std::cin >> nr >> phase.scale;
        for (std::size_t j = 0; j < nr; ++j) {
          ProfileReflection r;
          std::cin >> r.identity >> r.fc_squared >> r.multiplicity >>
              r.preferred_orientation >> r.support_widths >>
              r.support_intensity_ratio;
          for (std::size_t k = 0; k < nw; ++k) {
            ReflectionComponent c;
            std::cin >> c.center_degrees >> c.lp >> c.absorption;
            r.components.push_back(c);
          }
          phase.reflections.push_back(r);
        }
      }
      if (!std::cin)
        throw std::runtime_error("incomplete protocol");
      std::cout << "CASE " << id << '\n';
      array("GRID", x);
      std::vector<double> weighted(nx, 0.);
      for (std::size_t p = 0; p < np; ++p) {
        const auto &phase = phases[p];
        std::vector<double> running(nx, 0.);
        for (std::size_t k = 0; k < nw; ++k)
          for (std::size_t j = 0; j < phase.reflections.size(); ++j) {
            const auto &r = phase.reflections[j];
            auto c = take(
                reflection_contribution(grid, r, k, radiation, parameters));
            auto shape = c.shape;
            double support = shape.fwhm_degrees * r.support_widths *
                             r.support_intensity_ratio;
            std::cout << "PEAK " << p + 1 << ' ' << k + 1 << ' ' << j + 1 << ' '
                      << c.first_index + 1 << ' ' << c.last_index + 1 << ' '
                      << shape.center_degrees << ' ' << shape.fwhm_degrees
                      << ' ' << shape.beta_or_eta << ' '
                      << c.integrated_intensity << ' ' << support << '\n';
            for (std::size_t i = c.first_index; i <= c.last_index; ++i) {
              double kernel =
                  take(symmetric_kernel(shape, x[i] - shape.center_degrees));
              double asym =
                  take(asymmetry_factor(shape, x[i] - shape.center_degrees));
              double value = c.intensity[i - c.first_index];
              running[i] += value;
              std::cout << "POINT " << p + 1 << ' ' << k + 1 << ' ' << j + 1
                        << ' ' << i + 1 << ' ' << kernel << ' ' << asym << ' '
                        << value << ' ' << running[i] << '\n';
            }
          }
        auto raw = take(phase_profile(grid, phase, radiation));
        if (raw != running)
          throw std::runtime_error("layer accumulation mismatch");
        array("RAW", raw);
        for (std::size_t i = 0; i < nx; ++i)
          weighted[i] += phase.scale * raw[i];
        array("WEIGHTED", weighted);
      }
      auto start = std::chrono::steady_clock::now();
      auto final = take(powder_profile(grid, radiation, phases, display));
      auto stop = std::chrono::steady_clock::now();
      array("FINAL", final.calculated_intensity);
      array("BACKGROUND", final.background);
      std::cout
          << "TIME_US "
          << std::chrono::duration<double, std::micro>(stop - start).count()
          << '\n';
    }
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
