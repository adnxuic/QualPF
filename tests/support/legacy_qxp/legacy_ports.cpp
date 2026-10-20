#include "project_codec.h"
#include "infrastructure/serialization/quantification.h"
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
namespace qualpf::infrastructure::legacy_fixture {
Status validateLegacyProjectSaveSource(ProductHandle source) {
    if (!source) return Status{Error{ErrorCode::missing_data, "project.source", "Missing project source"}};
    if (source->editing) return Status{Error{ErrorCode::invalid_input, "session.edit", "Finish the preprocessing edit before saving a project."}};
    if (!source->algorithms) return Status{Error{ErrorCode::missing_data, "project.algorithms", "Missing project algorithms"}};
    if (source->pfsm || source->algorithms->spec.method() == SearchMethod::pfsm)
        return Status{Error{ErrorCode::unsupported,"project.pfsm","QXP v1/v2 cannot preserve PFSM problem and fitted-state evidence","qxp_pfsm_unsupported"}};
    auto method = preflightSearchMethod(source->algorithms->spec.method());
    if (!method) return method;
    const AlgorithmSpec defaults;
    const auto nondefault = [&](const AlgorithmRunSource &origin) { return !origin.algorithms || origin.algorithms->spec != defaults; };
    bool unsafe = hasDerivedProjectData(*source) && source->algorithms->spec != defaults;
    for (const auto &origin : source->provenance.residualHistory) unsafe |= nondefault(origin);
    for (const auto &origin : source->provenance.retainedPeakSources) unsafe |= nondefault(origin);
    for (const auto &origin : source->provenance.subtractedBackgroundHistory) unsafe |= nondefault(origin);
    for (const auto &origin : {source->provenance.backgroundSource, source->provenance.peakSource})
        if (origin) unsafe |= nondefault(*origin);
    for (const auto *cards : {&source->candidates, &source->phases})
        for (const auto &card : *cards)
            if (card.algorithmSource) unsafe |= nondefault(card.algorithmSource->score) || nondefault(card.algorithmSource->scale);
    if (unsafe) return Status{Error{ErrorCode::unsupported, "project.algorithms",
        "QXP v1/v2 cannot preserve non-default algorithm provenance; start a compatible analysis or use another export",
        "qxp_algorithm_provenance_unsupported"}};
    return Status{std::monostate{}};
}
Result<std::string> ProjectFileWriter::prepare(application::ProductHandle source,const application::MetadataObject &defaults,const std::string &created) const {
    auto valid = application::validateProjectSaveSource(source);
    if (!valid) return Result<std::string>{valid.error()};
    auto prepared = application::prepareProjectSerialization(source, quantificationInputs(source->phases));
    if (!prepared) return Result<std::string>{prepared.error()};
    auto encoded=encodeProject(prepared.value(),defaults,QString::fromStdString(created));
    if (!encoded) return Result<std::string>{encoded.error()};
    return Result<std::string>{QJsonDocument(encoded.value()).toJson().toStdString()};
}
Status ProjectFileWriter::commit(const std::string &path,const std::string &bytes) const {
    QSaveFile file(QString::fromStdString(path));
    if (!file.open(QIODevice::WriteOnly|QIODevice::Text))
        return Status{Error{ErrorCode::invalid_input,"project.file",path,"project_open_failed"}};
    if (file.write(bytes.data(),qint64(bytes.size()))!=qint64(bytes.size()) || !file.commit())
        return Status{Error{ErrorCode::invalid_input,"project.file",path,"project_save_failed"}};
    return Status{std::monostate{}};
}
Status saveProjectInput(ProductHandle source, const ProjectTargetWriter &writer, const std::string &path,
                        const MetadataObject &defaults, const std::string &created) {
    auto valid = validateProjectSaveSource(source);
    if (!valid) return valid;
    auto bytes=writer.prepare(std::move(source),defaults,created);
    if (!bytes) return Status{bytes.error()};
    return writer.commit(path,bytes.value());
}
Status openProjectInput(ProductSearchSession &session, const ProjectSourceReader &reader, const std::string &path, const MetadataObject &defaults) {
    auto source=session.snapshot();
    if (source->editing) return Status{Error{ErrorCode::invalid_input,"session.edit","Finish the preprocessing edit before loading a project."}};
    // Self-contained projects must not read or depend on global defaults.
    // Legacy files receive a one-time migration below after their saved fields are known.
    auto loaded=reader.read(path,source->backend,session.defaultSettingsStore()?MetadataObject{}:defaults);
    if (!loaded) return Status{loaded.error()};
    if (!loaded.value().projectSettings && session.defaultSettingsStore()) {
        auto settings = session.projectDefaults();
        if (!settings) return Status{settings.error()};
        const auto &old = loaded.value().preprocessing;
        for (auto id : workflows) {
            auto &p = settings.value().profile(id);
            if (old.background) p.background = *old.background;
            if (old.initial_peaks) p.initial_peaks = *old.initial_peaks;
            if (old.fwhm && (id != WorkflowId::pfsm || old.fwhm->use_fixed)) p.fwhm = *old.fwhm;
            if (old.peaks) p.peaks = *old.peaks;
            if (old.selection) p.selection = *old.selection;
            if (old.smoothing) p.smoothing = *old.smoothing;
        }
        auto options = csmSettingValues(settings.value().profile(WorkflowId::csm).csm);
        storePfsmSettings(options,settings.value().profile(WorkflowId::pfsm).pfsm);
        for (const auto &[key, value] : loaded.value().searchOptions) options[key] = value;
        loaded.value().searchOptions = options;
        settings.value().profile(WorkflowId::csm).csm = csmSettings(options);
        settings.value().profile(WorkflowId::pfsm).pfsm = pfsmSettings(options);
        loaded.value().projectSettings = std::move(settings).value();
    }
    if (!loaded.value().restored_project) loaded.value().algorithms = source->algorithms;
    // The handle is active configuration, never an attestation of a legacy file's results.
    if (loaded.value().provenance.legacyUnverified && loaded.value().algorithms->spec != AlgorithmSpec{}) {
        for (auto &card : loaded.value().candidates) card.fomCalculated = false;
        for (auto &phase : loaded.value().phases) phase.fomCalculated = false;
    }
    return session.restoreProject(std::move(source),std::move(loaded).value());
}

}
