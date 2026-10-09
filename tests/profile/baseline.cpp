#include "qualpf/profile/powder_profile.h"
#include <chrono>
#include <iostream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
// clang-format off: psapi.h requires the Windows type declarations first.
#include <windows.h>
#include <psapi.h>
// clang-format on
#endif
using namespace qualpf::profile;
int main() {
  for (std::size_t count : {10u, 100u, 1000u})
    for (std::size_t components : {1u, 2u}) {
      auto grid = ProfileGrid::regular({5, 85, .01});
      if (!grid)
        return 1;
      ProfilePhase phase;
      ProfileRadiation rad;
      for (std::size_t k = 0; k < components; ++k)
        rad.components.push_back({1.54 + k * .004, k ? .5 : 1});
      for (std::size_t i = 0; i < count; ++i) {
        ProfileReflection r{i, 100, 2, 1, 20, 1, {}};
        for (std::size_t k = 0; k < components; ++k)
          r.components.push_back({10 + 65 * double(i) / count + k * .05, 1, 1});
        phase.reflections.push_back(r);
      }
      auto start = std::chrono::steady_clock::now();
      auto output = powder_profile(grid.value(), rad, std::span(&phase, 1));
      auto stop = std::chrono::steady_clock::now();
      if (!output)
        return 1;
      std::size_t memory = 0;
#ifdef _WIN32
      PROCESS_MEMORY_COUNTERS counters{};
      counters.cb = sizeof(counters);
      if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters,
                                sizeof(counters)))
        return 2;
      memory = counters.PeakWorkingSetSize;
#endif
      std::cout
          << count << ' ' << grid.value().points().size() << ' ' << components
          << ' '
          << std::chrono::duration<double, std::milli>(stop - start).count()
          << ' ' << memory << '\n';
    }
}
