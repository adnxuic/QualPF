#include "qualpf/application/product_files.h"
#include "qualpf/application/preprocessing_evaluation.h"
namespace qualpf::application {
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
Status saveProjectInput(ProductHandle source, const ProjectTargetWriter &writer, const std::string &path,
                        const MetadataObject &defaults, const std::string &created) {
    if (!source) return Status{Error{ErrorCode::missing_data,"project.source","Missing project source"}};
    if (source->editing) return Status{Error{ErrorCode::invalid_input,"session.edit","Finish the preprocessing edit before saving a project."}};
    auto bytes=writer.prepare(std::move(source),defaults,created);
    if (!bytes) return Status{bytes.error()};
    return writer.commit(path,bytes.value());
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
