#pragma once
#include "gui/state/search_session.h"
#include "qualpf/application/powder_cif_source.h"
#include <QString>
#include <functional>

namespace qualpf::gui {
using namespace application;
struct PowderCifImportDraft {
    SessionViewHandle source;
    io::powder_cif::PowderCifPreparation preparation;
};
struct PreparedPowderCifImport {
    SessionViewHandle source;
    io::powder_cif::ImportedPowderPattern imported;
    SessionStateView next;
};
class PowderCifImportService {
  public:
    explicit PowderCifImportService(SessionView &session, const application::PowderCifSourceReader &reader) : session_(session), reader_(reader) {}
    io::powder_cif::ImportResult<PowderCifImportDraft> prepare(const QString &path) const;
    io::powder_cif::ImportResult<PowderCifImportDraft> prepareBytes(std::string bytes,std::string sourceId) const;
    io::powder_cif::ImportResult<PreparedPowderCifImport> complete(const PowderCifImportDraft &,
        std::optional<io::powder_cif::UserSuppliedRadiation> supplied={},bool synchrotron=false) const;
    Status commit(PreparedPowderCifImport,
        const std::function<Status(const SessionStateView &)> &preflight={},
        const std::function<void(const SessionViewHandle &)> &effects={});
  private:
    SessionView &session_;
    const application::PowderCifSourceReader &reader_;
};
} // namespace qualpf::gui
