#pragma once
#include "gui/state/search_session.h"
#include "qualpf/application/product_files.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include <QString>

namespace qualpf::gui::exporting {
using namespace application; using namespace qualpf::gui;
using namespace application::exporting;


// One Application owner for canonical diffraction and aligned background export.
// The immutable source handle keeps private Legacy preview buffers outside this
// read-only transaction. GUI owns target/format choice and overwrite confirmation.
class PatternExportService final {
  public:
    explicit PatternExportService(SessionViewHandle source, const AtomicExportTargetFactory &factory) : source_(std::move(source)), factory_(factory) {}
    Status save(const QString &target, qualpf::exporting::PatternFormat format) const;
    Status saveTo(AtomicExportTarget &target, qualpf::exporting::PatternFormat format) const;

  private:
    Result<qualpf::exporting::PatternExportSnapshot> snapshot(qualpf::exporting::PatternFormat format) const;
    SessionViewHandle source_;
    const AtomicExportTargetFactory &factory_;
};
} // namespace qualpf::gui::exporting
