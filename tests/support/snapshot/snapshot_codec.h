#pragma once

#include "qualpf/domain/validation.h"

#include <span>
#include <string>
#include <vector>

namespace qualpf::legacy {

// Transport projection of the existing C-float exports. Borrows exist only
// during copy_*(); no pointer/Fortran storage is retained by the returned model.
struct PatternExportView {
    std::span<const float> two_theta;
    std::span<const float> observed;
    std::span<const float> background;
    std::span<const float> wavelength;
    std::span<const float> ratio;
    bool has_background = false;
    bool background_subtracted = false;
    int radiation_code = 0;
    std::optional<AngularRange> reported_range;
    // No nc1/nc2 in current ABI. The converter does not infer background alignment.
};
struct PatternProjection {
    Pattern pattern;
    int legacy_radiation_code = 0; // Preserved outside Core; mapping is not yet verified.
    std::vector<std::string> unavailable_fields;
};
struct PeakExportView {
    std::span<const PeakId> ids; // Assigned by the owner, not synthesized from GUI indices.
    std::span<const float> two_theta;
    std::span<const float> d_spacing;
    std::span<const float> delta_d;
    std::span<const float> intensity;
    std::span<const float> fwhm;
    WavelengthAngstrom effective_wavelength;
};

[[nodiscard]] Result<int> checked_abi_count(std::size_t count);
[[nodiscard]] Result<PatternProjection> copy_pattern_export(
    const PatternExportView&, DatasetId, std::string decoded_source_id);
[[nodiscard]] Result<PeakSet> copy_peak_export(const PeakExportView&, DatasetId);

// Deliberately no live reader/writer and no IScoringBackend implementation:
// the current ABI cannot load/restore a complete isolated numerical context.
// Reading existing global state and calling computeFOM is NOT a pure backend.

} // namespace qualpf::legacy
