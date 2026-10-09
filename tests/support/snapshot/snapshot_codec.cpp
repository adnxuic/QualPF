#include "snapshot_codec.h"

#include <limits>
#include <utility>

namespace qualpf::legacy {
static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24,
              "Legacy float32 projection requires IEEE binary32 float");
namespace {
Error mismatch(const char* field)
{
    return {ErrorCode::invalid_input, field, "Export array lengths or flags disagree"};
}
} // namespace

Result<int> checked_abi_count(std::size_t count)
{
    if (count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        return Result<int>{Error{ErrorCode::invalid_input, "abi.count", "Array length exceeds the C int ABI capacity"}};
    return Result<int>{static_cast<int>(count)};
}

Result<PatternProjection> copy_pattern_export(const PatternExportView& source,
                                             DatasetId id, std::string decoded_source_id)
{
    if (source.two_theta.size() != source.observed.size())
        return Result<PatternProjection>{mismatch("pattern.observed")};
    if (source.wavelength.size() != source.ratio.size())
        return Result<PatternProjection>{mismatch("pattern.radiation")};
    if (!source.has_background && !source.background.empty())
        return Result<PatternProjection>{mismatch("pattern.background")};
    for (auto count : {source.two_theta.size(), source.background.size(), source.wavelength.size()}) {
        auto bounded = checked_abi_count(count);
        if (!bounded) return Result<PatternProjection>{bounded.error()};
    }
    PatternProjection output;
    auto& pattern = output.pattern;
    pattern.id = id;
    pattern.source_id = std::move(decoded_source_id);
    pattern.source_precision = SourcePrecision::float32_abi;
    pattern.calculation_range = source.reported_range;
    pattern.processing.background_subtracted = source.background_subtracted;
    output.legacy_radiation_code = source.radiation_code;
    for (float x : source.two_theta) pattern.two_theta.emplace_back(static_cast<double>(x));
    pattern.observed.assign(source.observed.begin(), source.observed.end());
    if (source.has_background) {
        pattern.background = SampledSignal{};
        pattern.background->values.assign(source.background.begin(), source.background.end());
    }
    for (std::size_t i = 0; i < source.wavelength.size(); ++i)
        pattern.radiation.lines.push_back({WavelengthAngstrom{source.wavelength[i]}, source.ratio[i]});
    output.unavailable_fields = {
        "original_observed", "corrected_two_theta", "calculated", "background_subtracted_signal",
        "weights", "background.window", "radiation.kind", "intensity_scale",
        "processing.kalpha2_subtracted", "processing.intensity_scaled",
        "preprocessing_parameters", "additional_datasets"
    };
    if (!source.reported_range) output.unavailable_fields.push_back("calculation_range");
    const auto issues = validate(pattern);
    if (!issues.empty()) return Result<PatternProjection>{issues.front()};
    return Result<PatternProjection>{std::move(output)};
}

Result<PeakSet> copy_peak_export(const PeakExportView& source, DatasetId id)
{
    const auto count = source.two_theta.size();
    for (auto length : {source.ids.size(), source.d_spacing.size(), source.delta_d.size(),
                        source.intensity.size(), source.fwhm.size()})
        if (length != count) return Result<PeakSet>{mismatch("peaks")};
    const auto bounded = checked_abi_count(count);
    if (!bounded) return Result<PeakSet>{bounded.error()};
    PeakSet output;
    output.dataset = id;
    output.effective_wavelength = source.effective_wavelength;
    output.source_precision = SourcePrecision::float32_abi;
    for (std::size_t i = 0; i < count; ++i) {
        Peak peak;
        peak.id = source.ids[i];
        peak.two_theta = TwoThetaDegrees{source.two_theta[i]};
        peak.d_spacing = DSpacingAngstrom{source.d_spacing[i]};
        peak.delta_d = DSpacingAngstrom{source.delta_d[i]};
        peak.intensity = source.intensity[i];
        peak.fwhm = AngularWidthDegrees{source.fwhm[i]};
        // Original/residual distinction and integrated area are not in this export.
        output.peaks.push_back(std::move(peak));
    }
    const auto issues = validate(output);
    if (!issues.empty()) return Result<PeakSet>{issues.front()};
    return Result<PeakSet>{std::move(output)};
}

} // namespace qualpf::legacy
