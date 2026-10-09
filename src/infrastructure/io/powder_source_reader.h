#pragma once
#include "qualpf/application/powder_cif_source.h"
namespace qualpf::infrastructure {
class PowderSourceReader final : public application::PowderCifSourceReader {
  public:
    application::MetadataObject encodeProvenance(const application::PowderCifProvenance &) const override;
    io::powder_cif::ImportResult<application::SourceBytes> read(const std::string &identity) const override;
};
} // namespace qualpf::infrastructure
