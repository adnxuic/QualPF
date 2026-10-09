#pragma once

#include "gui/model/experimentalpeaks.h"
#include "qualpf/application/domain_inputs.h"

#include <span>

namespace qualpf::gui {
using namespace application;

// Copy only. Does not read AppState, settings, Fortran or mutate the Qt source.
[[nodiscard]] Result<PeakSet> copy_experimental_peaks(
    const ExperimentalPeaks&, DatasetId, std::span<const PeakId> stable_ids);

} // namespace qualpf::gui
