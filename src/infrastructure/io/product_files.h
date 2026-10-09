#pragma once
#include "qualpf/application/product_files.h"
#include "infrastructure/io/pattern_source_reader.h"
#include "infrastructure/io/powder_source_reader.h"
#include "infrastructure/io/atomic_export_target.h"
#include "infrastructure/serialization/project_codec.h"
namespace qualpf::infrastructure {
class ProjectFileWriter final : public application::ProjectTargetWriter {
public:
    Result<std::string> prepare(application::ProductHandle,const application::MetadataObject &,const std::string &) const override;
    Status commit(const std::string &,const std::string &) const override;
};
class QtTextFiles final : public application::TextFileStore {
public:
    Result<std::string> read(const std::string &) const override;
    Status writeAtomic(const std::string &,const std::string &) const override;
};
// Entry-point convenience owner; only its pure port aggregate is borrowed.
struct ProductFileProviders {
    PatternSourceReader patterns;
    ProjectFileReader projects;
    ProjectFileWriter projectWriter;
    PowderSourceReader powder;
    QtTextFiles text;
    exporting::QtAtomicExportFactory exports;
    application::ProductFiles ports{patterns,projects,projectWriter,powder,text,exports};
};
}
