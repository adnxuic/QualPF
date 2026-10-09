#include "gui/adapters/experimental_peaks.h"

#include <utility>

namespace qualpf::gui {
using namespace application;

Result<PeakSet> copy_experimental_peaks(const ExperimentalPeaks& source, DatasetId id,
                                       std::span<const PeakId> stable_ids)
{
    if (!source.valid)
        return Result<PeakSet>{Error{ErrorCode::missing_data, "peaks", "Qt peak snapshot is not valid"}};
    const auto count = source.tth.size();
    if (source.integratedIntensity && source.integratedIntensity->size() != count)
        return Result<PeakSet>{
            Error{ErrorCode::invalid_input, "peaks.area", "Peak area column length differs"}};
    if (stable_ids.size() != static_cast<std::size_t>(count))
        return Result<PeakSet>{Error{ErrorCode::invalid_input, "peaks.ids", "One stable ID is required per peak"}};
    for (auto size : {source.d.size(), source.deltaD.size(), source.intensity.size(),
                      source.intensityOrig.size(), source.fwhm.size()})
        if (size != count)
            return Result<PeakSet>{Error{ErrorCode::invalid_input, "peaks", "Qt peak columns have different lengths"}};
    PeakSet output;
    output.dataset = id;
    output.effective_wavelength = WavelengthAngstrom{source.wave};
    output.source_precision = SourcePrecision::float64;
    for (qsizetype i = 0; i < count; ++i) {
        Peak peak;
        peak.id = stable_ids[static_cast<std::size_t>(i)];
        peak.two_theta = TwoThetaDegrees{source.tth[i]};
        peak.d_spacing = DSpacingAngstrom{source.d[i]};
        peak.delta_d = DSpacingAngstrom{source.deltaD[i]};
        peak.intensity = source.intensity[i];
        peak.original_intensity = source.intensityOrig[i];
        peak.fwhm = AngularWidthDegrees{source.fwhm[i]};
        if (source.integratedIntensity)
            peak.integrated_intensity = (*source.integratedIntensity)[i];
        output.peaks.push_back(std::move(peak));
    }
    return application::checkedPeakSet(std::move(output));
}

} // namespace qualpf::gui
