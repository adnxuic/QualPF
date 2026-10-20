#pragma once
#include "qualpf/domain/pattern.h"
#include "qualpf/domain/result.h"
#include <optional>
#include <span>
#include <vector>

namespace qualpf::search {
struct RirInput {
    bool calculated;
    std::span<const double> intensity;
    double scale;
    std::optional<double> rir;
};
std::vector<double> quantify_rir(std::span<const RirInput>);

// Fixed common-width, peak-top IL convention. These coefficients belong to the
// selected joint baseline, not a candidate trial. Reference maxima are taken
// from the complete discrete card before radiation expansion or windowing.
struct ProfileRirInput {
    double scale = 0, reference_maximum = 0;
    std::optional<double> rir;
};
Result<double> rir_reference_maximum(std::span<const double>);
Result<std::vector<double>> quantify_profile_rir(std::span<const ProfileRirInput>);
// Accept recorded Cu Kalpha centroids rounded to 4 decimal places. This is a
// radiation compatibility classification, never a change to fitted wavelengths.
bool profile_rir_supports_radiation(const Radiation &);
} // namespace qualpf::search
