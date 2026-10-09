#include "qualpf/application/import/powder_cif_import_service.h"
#include "qualpf/application/import/powder_cif_provenance.h"
namespace qualpf::application {
namespace {
using namespace io::powder_cif;
ImportError applicationError(Error error, std::shared_ptr<const SourceArtifact> source) {
    return {std::move(error), std::move(source), {}, {}, {}, {}, {}};
}
} // namespace
io::powder_cif::ImportResult<PowderCifImportDraft>
preparePowderImport(ProductHandle source, std::string bytes, std::string sourceId) {
    using namespace io::powder_cif;
    auto prepared = PowderCifImporter::prepare(std::move(bytes), std::move(sourceId));
    if (auto e = std::get_if<ImportError>(&prepared))
        return *e;
    auto draft = PowderCifImportDraft{source, std::move(std::get<PowderCifPreparation>(prepared))};
    if (!source)
        return applicationError({ErrorCode::missing_data, "session", "Missing canonical import source"},
                                draft.preparation.source().artifact);
    if (source->editing)
        return applicationError({ErrorCode::invalid_input, "session.edit",
                                 "Finish the preprocessing edit before opening a pattern"},
                                draft.preparation.source().artifact);
    return draft;
}
io::powder_cif::ImportResult<PreparedPowderCifImport>
completePowderImport(const PowderCifImportDraft &draft,
                     std::optional<io::powder_cif::UserSuppliedRadiation> supplied, bool synchrotron, const PowderCifSourceReader *codec) {
    using namespace io::powder_cif;
    ImportResult<PowderCifImportSnapshot> snapshot =
        applicationError({ErrorCode::missing_data, "pattern.cif.wavelength",
                          "Choose a wavelength before importing", "wavelength_required"},
                         draft.preparation.source().artifact);
    if (draft.preparation.selected().wavelength_state == WavelengthState::SOURCE_WAVELENGTH)
        snapshot = *draft.preparation.snapshot();
    else if (supplied)
        snapshot = PowderCifImporter::resolve(draft.preparation, *supplied);
    if (auto e = std::get_if<ImportError>(&snapshot))
        return *e;
    auto imported =
        PowderCifImporter::to_pattern(std::move(std::get<PowderCifImportSnapshot>(snapshot)), {1});
    if (auto e = std::get_if<ImportError>(&imported))
        return *e;
    auto value = std::move(std::get<ImportedPowderPattern>(imported));
    auto next = prepareProductPatternOpen(draft.source, value.pattern, {}, synchrotron);
    if (!next)
        return applicationError(next.error(), value.provenance.source);
    next.value().powderCifProvenance =
        std::make_shared<const PowderCifProvenance>(powderCifProvenance(value.provenance));
    if (codec) next.value().projectMetadata["powder_cif_provenance"]=codec->encodeProvenance(*next.value().powderCifProvenance);
    return PreparedPowderCifImport{draft.source, std::move(value), std::move(next).value()};
}
Status commitPowderImport(ProductSearchSession &session, PreparedPowderCifImport pending,
                          const std::function<Status(const ProductState &)> &preflight,
                          const std::function<void(const ProductHandle &)> &effects) {
    if (session.snapshot() != pending.source)
        return Status{Error{ErrorCode::stale_snapshot, "session", "Search input changed"}};
    if (preflight) {
        const auto status = preflight(pending.next);
        if (!status)
            return status;
    }
    auto committed = session.commit(pending.source, std::move(pending.next));
    if (committed && effects)
        effects(session.snapshot());
    return committed;
}
} // namespace qualpf::application
