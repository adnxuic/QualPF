#include "default_settings.h"
#include "infrastructure/serialization/workflow_settings.h"
#include "infrastructure/serialization/metadata_values.h"
#include <QSettings>
#include <QJsonDocument>
namespace qualpf::infrastructure {
using namespace application;
namespace {
constexpr auto key="workflowDefaults/v1";
Error failure() {return {ErrorCode::invalid_input,"settings.defaults","Could not read or save workflow defaults","default_settings_io"};}
}
Result<ProjectSettings> QtDefaultSettingsStore::load() const {
    QSettings store; store.sync();
    if(store.status()!=QSettings::NoError) return Result<ProjectSettings>{failure()};
    if(store.contains(key)) {
        QJsonParseError error;
        const auto doc=QJsonDocument::fromJson(store.value(key).toByteArray(),&error);
        if(error.error!=QJsonParseError::NoError||!doc.isObject()) return Result<ProjectSettings>{failure()};
        return decodeWorkflowSettings(doc.object());
    }
    ProjectSettings s;
    // Import established QVariant conversions once; do not remove original keys.
    QJsonObject old;
    for(const auto &[k,v]:csmSettingValues({})) {
        auto value=store.value(QString::fromStdString(k),jsonValue(v).toVariant());
        if(std::holds_alternative<bool>(v.data)) old[QString::fromStdString(k)]=value.toBool();
        else if(std::holds_alternative<std::int64_t>(v.data)) old[QString::fromStdString(k)]=value.toInt();
        else old[QString::fromStdString(k)]=value.toDouble();
    }
    s.profile(WorkflowId::csm).csm=csmSettings(metadataValue(old).toObject());
    MetadataObject ps; storePfsmSettings(ps,{});
    for(auto &[k,v]:ps) if(store.contains(QString::fromStdString(k)))
        v=metadataValue(QJsonValue::fromVariant(store.value(QString::fromStdString(k))));
    s.profile(WorkflowId::pfsm).pfsm=pfsmSettings(ps);
    auto valid=validateProjectSettings(s);
    return valid ? Result<ProjectSettings>{std::move(s)} : Result<ProjectSettings>{valid.error()};
}
Status QtDefaultSettingsStore::save(const ProjectSettings &s) {
    auto valid=validateProjectSettings(s); if(!valid) return valid;
    const auto bytes=QJsonDocument(encodeWorkflowSettings(s)).toJson(QJsonDocument::Compact);
    QSettings store; store.sync();
    if(store.status()!=QSettings::NoError) return Status{failure()};
    const bool existed=store.contains(key); const auto previous=store.value(key);
    store.setValue(key,bytes); store.sync();
    if(store.status()==QSettings::NoError) return Status{std::monostate{}};
    if(existed) store.setValue(key,previous); else store.remove(key);
    store.sync(); return Status{failure()};
}
Status QtDefaultSettingsStore::migrate() {
    QSettings store;
    if(store.contains(key)) {auto current=load(); return current ? Status{std::monostate{}} : Status{current.error()};}
    auto imported=load(); return imported ? save(imported.value()) : Status{imported.error()};
}
}
