#pragma once
#include "qualpf/application/workspace_ports.h"
#include "qualpf/application/powder_cif_source.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include "qualpf/preprocessing/legacy.h"
#include "qualpf/application/peak_coordinates.h"
namespace qualpf::application {
class ProjectTargetWriter {
public:
    virtual ~ProjectTargetWriter() = default;
    virtual Result<std::string> prepare(ProductHandle, const MetadataObject &, const std::string &created) const = 0;
    virtual Status commit(const std::string &path, const std::string &bytes) const = 0;
};
class TextFileStore {
public:
    virtual ~TextFileStore() = default;
    virtual Result<std::string> read(const std::string &) const = 0;
    virtual Status writeAtomic(const std::string &, const std::string &) const = 0;
};
namespace exporting {
class AtomicExportTargetFactory {
public:
    virtual ~AtomicExportTargetFactory() = default;
    virtual Result<std::unique_ptr<AtomicExportTarget>> create(const std::string &) const = 0;
};
Status writePatternExport(const AtomicExportTargetFactory &, const std::string &,
    const qualpf::exporting::PatternExportSnapshot &, qualpf::exporting::PatternFormat);
}
struct ProductFiles {
    PatternSourceReader &patterns;
    ProjectSourceReader &projects;
    ProjectTargetWriter &projectWriter;
    PowderCifSourceReader &powder;
    TextFileStore &text;
    exporting::AtomicExportTargetFactory &exports;
};
std::vector<Pattern> prepareComparisonPatterns(std::vector<Pattern>);
Status importPeakFile(ProductSearchSession &, const TextFileStore &, const std::string &, PeakCoordinateRepresentation);
Status saveProjectInput(ProductHandle, const ProjectTargetWriter &, const std::string &,
    const MetadataObject &, const std::string &created);
Result<std::string> preparePeakExport(const ProductState &, preprocessing::PeakTextFormat);
} // namespace qualpf::application
