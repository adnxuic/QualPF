#include "qualpf/application/import/powder_cif_import_service.h"


#include "powder_cif_import_service.h"


namespace qualpf::gui {
using namespace application;
io::powder_cif::ImportResult<PowderCifImportDraft>
PowderCifImportService::prepare(const QString &filename) const {
    using namespace io::powder_cif;
    auto loaded = reader_.read(filename.toStdString());
    if (auto error = std::get_if<ImportError>(&loaded))
        return *error;
    auto owned = std::move(std::get<SourceBytes>(loaded));
    return prepareBytes(std::move(owned.bytes), std::move(owned.identity));
}
io::powder_cif::ImportResult<PowderCifImportDraft>
PowderCifImportService::prepareBytes(std::string bytes, std::string sourceId) const {
    const auto source = session_.snapshot();
    auto result = application::preparePowderImport(source->canonical, std::move(bytes), std::move(sourceId));
    if (const auto *error = std::get_if<io::powder_cif::ImportError>(&result))
        return *error;
    return PowderCifImportDraft{source,
                                std::move(std::get<application::PowderCifImportDraft>(result).preparation)};
}
io::powder_cif::ImportResult<PreparedPowderCifImport>
PowderCifImportService::complete(const PowderCifImportDraft &draft,
                                 std::optional<io::powder_cif::UserSuppliedRadiation> supplied,
                                 bool synchrotron) const {
    auto result = application::completePowderImport({draft.source->canonical, draft.preparation}, supplied,
                                                    synchrotron, &reader_);
    if (const auto *error = std::get_if<io::powder_cif::ImportError>(&result))
        return *error;
    auto prepared = std::move(std::get<application::PreparedPowderCifImport>(result));
    auto next = searchView(std::make_shared<const ProductState>(std::move(prepared.next)));

    return PreparedPowderCifImport{draft.source, std::move(prepared.imported), std::move(next)};
}
Status PowderCifImportService::commit(PreparedPowderCifImport pending,
                                      const std::function<Status(const SessionStateView &)> &preflight,
                                      const std::function<void(const SessionViewHandle &)> &effects) {
    application::PreparedPowderCifImport prepared{pending.source->canonical, std::move(pending.imported),
                                                  productState(pending.next)};
    return application::commitPowderImport(
        session_.canonicalSession(), std::move(prepared),
        preflight ? std::function<Status(const ProductState &)>([&](const auto &next) {
            return preflight(searchView(std::make_shared<const ProductState>(next)));
        })
                  : std::function<Status(const ProductState &)>{},
        effects
            ? std::function<void(const ProductHandle &)>([&](const auto &) { effects(session_.snapshot()); })
            : std::function<void(const ProductHandle &)>{});
}
} // namespace qualpf::gui
