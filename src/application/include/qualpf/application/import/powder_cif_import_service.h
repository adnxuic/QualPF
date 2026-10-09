#pragma once
#include "powder_cif_importer.h"
#include "qualpf/application/product_session.h"
#include "qualpf/application/powder_cif_source.h"
namespace qualpf::application {
struct PowderCifImportDraft {
    ProductHandle source;
    io::powder_cif::PowderCifPreparation preparation;
};
struct PreparedPowderCifImport {
    ProductHandle source;
    io::powder_cif::ImportedPowderPattern imported;
    ProductState next;
};
io::powder_cif::ImportResult<PowderCifImportDraft> preparePowderImport(ProductHandle, std::string bytes,
                                                                       std::string sourceId);
io::powder_cif::ImportResult<PreparedPowderCifImport>
completePowderImport(const PowderCifImportDraft &,
                     std::optional<io::powder_cif::UserSuppliedRadiation> supplied = {},
                     bool synchrotron = false, const PowderCifSourceReader *codec = nullptr);
Status commitPowderImport(ProductSearchSession &, PreparedPowderCifImport,
                          const std::function<Status(const ProductState &)> &preflight = {},
                          const std::function<void(const ProductHandle &)> &effects = {});
} // namespace qualpf::application
