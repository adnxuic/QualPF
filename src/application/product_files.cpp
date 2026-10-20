#include "qualpf/application/product_files.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include <algorithm>
#include <cmath>
namespace qualpf::application {
bool hasDerivedProjectData(const ProductState &s) {
    return s.peaks.valid || s.preprocessing.peak_candidates || s.pattern.background ||
        s.pattern.processing.background_subtracted == true || !s.candidates.empty() || !s.phases.empty() ||
        (s.preprocessing.fwhm_usage && s.preprocessing.fwhm_usage->origin != FwhmOrigin::fixed);
}
Result<ProjectSerializationInput> prepareProjectSerialization(ProductHandle source, std::span<const QuantificationInput> inputs) {
    auto valid = validateProjectSaveSource(source);
    if (!valid) return Result<ProjectSerializationInput>{valid.error()};
    if (inputs.size() != source->phases.size()) return Result<ProjectSerializationInput>{Error{ErrorCode::invalid_input,
        "project.quantification", "Quantification inputs disagree with the frozen phases", "invalid_project"}};
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        const auto &phase = source->phases[i]; const auto &input = inputs[i];
        if (input.calculated != phase.fomCalculated || input.scale != phase.scale ||
            !std::ranges::equal(input.intensity, phase.intensity)) return Result<ProjectSerializationInput>{Error{ErrorCode::stale_snapshot,
                "project.quantification", "Quantification inputs disagree with the frozen phases", "invalid_project"}};
    }
    auto percentages = quantifyProduct(*source, inputs);
    // Preserve ordered binary64 normalization: 100*rap/sum can round one ULP
    // above 100 even for a valid single phase. Do not clamp or reject that value.
    if ((!percentages.empty() && percentages.size() != inputs.size()) ||
        std::any_of(percentages.begin(), percentages.end(), [](double value) { return !std::isfinite(value) || value < 0; }))
        return Result<ProjectSerializationInput>{Error{ErrorCode::invalid_input, "algorithm.output.rir", "Invalid quantitative output", "invalid_algorithm_output"}};
    return Result<ProjectSerializationInput>{ProjectSerializationInput{std::move(source), std::move(percentages)}};
}
std::vector<Pattern> prepareComparisonPatterns(std::vector<Pattern> patterns) {
    for(auto &p:patterns) {
        p.observed=*p.original_observed;
        p.processing.intensity_scaled=false;
        p.intensity_scale=IntensityScale::counts;
    }
    return patterns;
}
Status importPeakFile(ProductSearchSession &session,const TextFileStore &files,const std::string &path,PeakCoordinateRepresentation representation) {
    ProductPreprocessingEdit edit(session);
    const auto source=session.snapshot();
    auto bytes=files.read(path);
    if (!bytes) return Status{bytes.error()};
    auto entries=parsePeakCoordinates(bytes.value(),representation);
    if (!entries) return Status{entries.error()};
    if (source->pattern.radiation.lines.empty())
        return Status{Error{ErrorCode::invalid_input,"peaks.coordinate","A wavelength is required","missing_wavelength"}};
    PreprocessingRequest request; request.imported=std::move(entries).value();
    request.wavelength=source->pattern.radiation.lines.front().wavelength;
    auto status=edit.preview(request);
    return status ? edit.commit() : status;
}
Result<std::string> preparePeakExport(const ProductState &source, preprocessing::PeakTextFormat format) {
    auto peaks=productSearchPeaks(source.peaks);
    if (!peaks) return Result<std::string>{peaks.error()};
    return preprocessing::export_peak_text(peaks.value(),format);
}
namespace exporting {
Status writePatternExport(const AtomicExportTargetFactory &factory, const std::string &path,
    const qualpf::exporting::PatternExportSnapshot &snapshot, qualpf::exporting::PatternFormat format) {
    auto target=factory.create(path);
    if (!target) return Status{target.error()};
    return write_pattern_export_to(*target.value(),snapshot,format);
}
}
}
