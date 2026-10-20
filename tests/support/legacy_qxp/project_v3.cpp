#include "project_v3.h"
#include "infrastructure/serialization/powder_cif_provenance.h"
#include "project_v3_archive.h"
#include "qualpf/domain/validation.h"
#include "infrastructure/serialization/quantification.h"
#include <QJsonDocument>
namespace qualpf::infrastructure::legacy_fixture {
namespace {
using namespace qxp3;
// Preserve unknown typed fields independently of the current scientific state.
QJsonValue unknownFields(const QJsonValue &saved, const QJsonValue &known) {
    if (saved.isObject() && known.isObject()) {
        auto out = saved.toObject();
        const auto k = known.toObject();
        for (auto i = k.begin(); i != k.end(); ++i) {
            const auto child = unknownFields(out[i.key()], i.value());
            if (child.isUndefined())
                out.remove(i.key());
            else
                out[i.key()] = child;
        }
        return out.isEmpty() ? QJsonValue(QJsonValue::Undefined) : QJsonValue(out);
    }
    if (saved.isArray() && known.isArray() && saved.toArray().size() == known.toArray().size()) {
        auto a = saved.toArray(), b = known.toArray();
        QJsonArray out;
        bool any = false;
        for (qsizetype i = 0; i < a.size(); ++i) {
            auto child = unknownFields(a[i], b[i]);
            any |= !child.isUndefined();
            out.append(child.isUndefined() ? QJsonValue(QJsonValue::Null) : child);
        }
        if (any)
            return out;
    }
    return QJsonValue::Undefined;
}
QJsonValue mergeUnknown(QJsonValue known, const QJsonValue &extra) {
    if (known.isObject() && extra.isObject()) {
        auto out = known.toObject();
        const auto e = extra.toObject();
        for (auto i = e.begin(); i != e.end(); ++i)
            out[i.key()] = out.contains(i.key()) ? mergeUnknown(out[i.key()], i.value()) : i.value();
        return out;
    }
    if (known.isArray() && extra.isArray() && known.toArray().size() == extra.toArray().size()) {
        auto out = known.toArray();
        const auto e = extra.toArray();
        for (qsizetype i = 0; i < out.size(); ++i)
            out[i] = mergeUnknown(out[i], e[i]);
        return out;
    }
    return known;
}
void validateRecords(const ProductState &s) {
    if (s.workflow.imported_pattern &&
        (!s.workflow.imported_pattern->two_theta.empty() || !s.workflow.imported_pattern->observed.empty())) {
        const auto issues = validate(*s.workflow.imported_pattern);
        if (!issues.empty())
            throw Failure{issues.front()};
    }
    for (const auto &r : s.workflow.records) {
        ProjectSettings settings;
        settings.profile(r.workflow) = r.parameters;
        take(validateProjectSettings(settings));
        if (r.input) {
            const auto issues = validate(*r.input);
            if (!issues.empty())
                throw Failure{issues.front()};
        }
        const bool finished =
            r.status == WorkflowStepStatus::completed || r.status == WorkflowStepStatus::skipped;
        if (r.status == WorkflowStepStatus::running)
            invalid("project.workflow", "A running step cannot be restored");
        if (!finished)
            continue;
        if (!r.input || r.input->id != s.pattern.id || r.input->source_id != s.pattern.source_id)
            invalid("project.workflow.input", "Workflow input identity disagrees with the project");
        for (auto d : r.dependencies) {
            auto found = std::find_if(s.workflow.records.begin(), s.workflow.records.end(),
                                      [&](const auto &v) { return v.id == d; });
            if (found == s.workflow.records.end() || (found->status != WorkflowStepStatus::completed &&
                                                      found->status != WorkflowStepStatus::skipped))
                invalid("project.workflow.dependencies", "Completed result has an invalid dependency");
        }
        if (r.step == WorkflowStep::background && !s.pattern.background)
            invalid("project.workflow.background");
        if (r.step == WorkflowStep::initial_peaks && !s.preprocessing.initial_peak_usage)
            invalid("project.workflow.initial");
        if (r.step == WorkflowStep::fwhm && !s.preprocessing.fwhm_usage)
            invalid("project.workflow.fwhm");
        if (r.step == WorkflowStep::peaks && !s.peaks.valid &&
            !(r.manual && r.actual_parameters.contains("cleared") &&
              r.actual_parameters.at("cleared").toBool()))
            invalid("project.workflow.peaks");
        if (r.step == WorkflowStep::search && r.workflow == WorkflowId::pfsm && !s.pfsm)
            invalid("project.workflow.pfsm");
    }
}
} // namespace
Result<QJsonObject> encodeProjectV3(application::ProductHandle source, const QString &created) try {
    take(application::validateProjectSaveSource(source));
    validateRecords(*source);
    auto root = jsonValue(application::MetadataValue(source->projectEnvelope)).toObject();
    const auto extra = jsonValue(source->projectStateExtensions);
    root["version"] = 3;
    root["created"] = created;
    root["state"] = mergeUnknown(qxp3::encode(*source), extra);
    const auto quantitative =
        application::prepareQuantification(source, quantificationInputs(source->phases));
    QJsonArray percentages;
    for (double value : quantitative.percentages())
        percentages.append(value);
    root["quantification"] = mergeUnknown(QJsonObject{{"method", int(quantitative.method)},
                                                      {"availability", int(quantitative.availability)},
                                                      {"percentages", percentages}},
                                          root["quantification"]);
    if (source->powderCifProvenance)
        root["powder_cif_provenance"] = encodePowderCifProvenance(*source->powderCifProvenance);
    else
        root.remove("powder_cif_provenance");
    return Result<QJsonObject>{std::move(root)};
} catch (const qxp3::Failure &failure) {
    return Result<QJsonObject>{failure.error};
}
Result<application::ProductState> decodeProjectV3(const QJsonObject &root,
                                                  application::SearchBackend backend) try {
    qxp3::Context context;
    auto state = qxp3::decode<application::ProductState>(root["state"], context, "project.state");
    state.backend = backend;
    state.restored_project = true;
    if (state.workflow.last_attempt && state.workflow.last_attempt->status == WorkflowStepStatus::running)
        invalid("project.workflow.activity", "A running attempt cannot be restored");
    state.powderCifProvenance =
        take(decodePowderCifProvenance(root["powder_cif_provenance"], state.pattern.source_id));
    take(application::restorePfsmEvidence(state));
    take(application::validateProductState(state));
    validateRecords(state);
    auto extra = unknownFields(root["state"], qxp3::encode(state));
    auto envelope = root;
    for (const char *key : {"version", "created", "state", "powder_cif_provenance", "quantification"})
        envelope.remove(key);
    state.projectStateExtensions = metadataValue(extra);
    const auto quantitative = application::prepareQuantification(std::make_shared<const ProductState>(state),
                                                                 quantificationInputs(state.phases));
    QJsonArray percentages;
    for (double value : quantitative.percentages())
        percentages.append(value);
    const auto saved = root["quantification"].toObject();
    if (!root["quantification"].isObject() || saved["method"] != QJsonValue(int(quantitative.method)) ||
        saved["availability"] != QJsonValue(int(quantitative.availability)) ||
        saved["percentages"] != QJsonValue(percentages))
        invalid("project.quantification", "Saved quantification disagrees with the phase evidence");
    auto quantitativeExtra = saved;
    for (const char *key : {"method", "availability", "percentages"})
        quantitativeExtra.remove(key);
    if (!quantitativeExtra.isEmpty())
        envelope["quantification"] = quantitativeExtra;
    state.projectEnvelope = metadataValue(envelope).toObject();
    return Result<application::ProductState>{std::move(state)};
} catch (const qxp3::Failure &failure) {
    return Result<application::ProductState>{failure.error};
}
} // namespace qualpf::infrastructure
