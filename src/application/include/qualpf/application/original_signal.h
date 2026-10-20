#pragma once
#include <optional>
#include <string>
#include <vector>
namespace qualpf::application {
// Runtime importer evidence owned by Application. Legacy project arrays alone
// cannot attest the origin, units or statistical model of retained originals.
struct OriginalSignalProvenance {
    std::string source, intensity_units;
    bool unclipped = false, intensity_scaled = false;
    std::optional<std::vector<double>> standard_deviations;
    bool raw_poisson_counts = false;
    std::string noise_source;
    friend bool operator==(const OriginalSignalProvenance &, const OriginalSignalProvenance &) = default;
};
} // namespace qualpf::application
