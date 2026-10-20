#pragma once
#include "qualpf/application/product_files.h"
namespace qualpf::infrastructure::legacy_fixture {
using namespace application;
class ProjectTargetWriter {
public:
    virtual ~ProjectTargetWriter() = default;
    virtual Result<std::string> prepare(ProductHandle, const MetadataObject &, const std::string &created) const = 0;
    virtual Status commit(const std::string &path, const std::string &bytes) const = 0;
};

class ProjectSourceReader {
public:
 virtual ~ProjectSourceReader() = default;
 virtual Result<ProductState> read(const std::string &,SearchBackend,const MetadataObject &) const = 0;
};
class ProjectFileWriter : public ProjectTargetWriter {
public:
 Result<std::string> prepare(ProductHandle,const MetadataObject &,const std::string &) const override;
 Status commit(const std::string &,const std::string &) const override;
};
Status validateLegacyProjectSaveSource(ProductHandle);
Status saveProjectInput(ProductHandle,const ProjectTargetWriter &,const std::string &,const MetadataObject &,const std::string &);
Status openProjectInput(ProductSearchSession &,const ProjectSourceReader &,const std::string &,const MetadataObject &);
}
