#pragma once
#include "qualpf/application/product_files.h"
#include <QString>
namespace qualpf::infrastructure::exporting {
class QtAtomicExportFactory final : public application::exporting::AtomicExportTargetFactory {
public:
    Result<std::unique_ptr<application::exporting::AtomicExportTarget>> create(const std::string &) const override;
};
Status write_pattern_export_atomic(const QString &, const qualpf::exporting::PatternExportSnapshot &,
                                   qualpf::exporting::PatternFormat);
}
