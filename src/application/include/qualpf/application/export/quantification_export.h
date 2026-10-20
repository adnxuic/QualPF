#pragma once
#include "qualpf/application/presentation/quantification.h"
#include "qualpf/application/export/atomic_export_writer.h"
namespace qualpf::application::exporting {
class AtomicExportTargetFactory;
Result<std::string> prepareQuantificationCsv(const QuantificationResult &);
Status write_quantification_csv_to(AtomicExportTarget &, const QuantificationResult &);
Status writeQuantificationCsv(const AtomicExportTargetFactory &, const std::string &path,
                              const QuantificationResult &);
} // namespace qualpf::application::exporting
