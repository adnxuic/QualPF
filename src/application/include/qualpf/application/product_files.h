#pragma once
#include "qualpf/application/workspace_ports.h"
#include "qualpf/application/powder_cif_source.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include "qualpf/preprocessing/legacy.h"
#include "qualpf/application/peak_coordinates.h"
#include "qualpf/application/presentation/quantification.h"
namespace qualpf::application {
class ProjectSerializationInput {
  public:
    const ProductHandle &source() const { return source_; }
    const std::vector<double> &percentages() const { return percentages_; }
    const AlgorithmHandle &algorithms() const { return source_->algorithms; }
    bool quantitativeValid() const { return !percentages_.empty() && percentages_.size() == source_->phases.size(); }
    bool legacyUnverified() const { return source_->provenance.legacyUnverified; }
  private:
    ProductHandle source_;
    std::vector<double> percentages_;
    ProjectSerializationInput(ProductHandle source, std::vector<double> percentages)
        : source_(std::move(source)), percentages_(std::move(percentages)) {}
    friend Result<ProjectSerializationInput> prepareProjectSerialization(ProductHandle, std::span<const QuantificationInput>);
};
bool hasDerivedProjectData(const ProductState &);
Status validateProjectSaveSource(ProductHandle);
Result<ProjectSerializationInput> prepareProjectSerialization(ProductHandle, std::span<const QuantificationInput>);
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
Status saveProjectInput(Workspace &, const ProjectTargetWriter &, const std::string &);
Result<std::string> preparePeakExport(const ProductState &, preprocessing::PeakTextFormat);
} // namespace qualpf::application
