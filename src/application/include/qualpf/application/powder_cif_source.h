#pragma once
#include "powder_cif_importer.h"
#include "qualpf/application/product_values.h"
#include "qualpf/application/import/powder_cif_provenance.h"
namespace qualpf::application {
struct SourceBytes {
    std::string bytes, identity;
};
class PowderCifSourceReader {
  public:
    virtual ~PowderCifSourceReader() = default;
    virtual MetadataObject encodeProvenance(const PowderCifProvenance &) const = 0;
    virtual io::powder_cif::ImportResult<SourceBytes> read(const std::string &identity) const = 0;
};
} // namespace qualpf::application
