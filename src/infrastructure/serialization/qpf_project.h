#pragma once
#include "qualpf/application/product_files.h"
namespace qualpf::infrastructure {
class QpfProjectReader final : public application::ProjectSourceReader {
  public:
    Result<application::ProjectDocument> read(const std::string &, application::SearchBackend,
                                              const application::ProjectReadOptions & = {}) const override;
};
class QpfProjectWriter final : public application::ProjectTargetWriter {
  public:
    Result<application::ProjectIdentity> writeAtomic(const std::string &,
                                                     const application::ProjectSaveSnapshot &) const override;
};
} // namespace qualpf::infrastructure
