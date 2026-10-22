#include "infrastructure/settings/default_settings.h"
#include "infrastructure/settings/search_settings.h"
#include "infrastructure/serialization/workflow_settings.h"
#include "../../support/legacy_qxp/project_codec.h"
#include "qualpf/application/search_coordinator.h"
#include <QCoreApplication>
#include <QSettings>
#include <QTemporaryDir>
#include <QJsonArray>
#include <iostream>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::infrastructure;
using namespace qualpf::infrastructure::legacy_fixture;
static void check(bool v, const char *why) { if(!v) throw std::runtime_error(why); }
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    QCoreApplication::setOrganizationName("qualpf-tests"); QCoreApplication::setApplicationName("workflow-settings");
    QTemporaryDir temp; check(temp.isValid(),"temporary store");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    QSettings::setPath(QSettings::IniFormat,QSettings::SystemScope,temp.path());
    QSettings legacy; legacy.setValue("SearchOptions/minFom",.42); legacy.setValue("PFSMOptions/lowerShift",-.25);
    legacy.setValue("unknown/value",QByteArray("opaque")); legacy.sync();
    QtDefaultSettingsStore store;
    auto factory=store.load(); check(bool(factory),"load factory defaults");
    check(factory.value().profile(WorkflowId::pfsm).pfsm.template_cache,"missing cache preference defaults on");
    legacy.setValue("PFSMOptions/templateCache",false); legacy.sync();
    check(bool(store.migrate()),"migration");
    auto loaded=store.load(); check(bool(loaded),"load migrated defaults");
    check(loaded.value().profile(WorkflowId::csm).csm.minimum_fom==.42,"CSM migration");
    check(loaded.value().profile(WorkflowId::pfsm).pfsm.lower_shift==-.25,"PFSM migration");
    check(!loaded.value().profile(WorkflowId::pfsm).pfsm.template_cache,"explicit legacy cache off preserved");
    check(legacy.value("unknown/value").toByteArray()=="opaque","unrelated preference");
    check(loaded.value().profile(WorkflowId::pfsm).pfsm.candidate_recall_resource_fallback==PfsmResourceFallback::bounded_integral,"missing resource policy defaults to integral");
    auto settings=loaded.value();
    settings.profile(WorkflowId::pfsm).pfsm.candidate_recall_resource_fallback=PfsmResourceFallback::full_pool;
    settings.profile(WorkflowId::preprocessing).background.coefficients=3;
    settings.profile(WorkflowId::csm).background.coefficients=7;
    settings.profile(WorkflowId::pfsm).background.coefficients=9;
    settings.profile(WorkflowId::peak_search).background.coefficients=11;
    settings.extensions["future"]=MetadataObject{{"integer",std::int64_t(9007199254740993LL)},{"null",nullptr}};
    check(bool(store.save(settings)),"save typed defaults");
    Workspace a(SearchBackend::cpp,defaultAlgorithms(),&store);
    auto opening=a.session().snapshot(); check(opening->projectSettings.has_value(),"project initialized");
    auto next=*opening; next.projectSettings->profile(WorkflowId::csm).csm.minimum_fom=.73;
    check(bool(a.session().commit(opening,std::move(next))),"project options");
    check(store.load().value().profile(WorkflowId::csm).csm.minimum_fom==.42,"options do not write defaults");
    settings.profile(WorkflowId::csm).csm.minimum_fom=.61; check(bool(store.save(settings)),"change defaults");
    check(a.session().snapshot()->projectSettings->profile(WorkflowId::csm).csm.minimum_fom==.73,"defaults do not change project");
    Workspace b(SearchBackend::cpp,defaultAlgorithms(),&store);
    check(b.session().snapshot()->projectSettings->profile(WorkflowId::csm).csm.minimum_fom==.61,"new project gets defaults");
    const auto encoded=encodeWorkflowSettings(settings); auto decoded=decodeWorkflowSettings(encoded);
    check(bool(decoded),"decode all profiles");
    check(!decoded.value().profile(WorkflowId::pfsm).pfsm.template_cache,"cache off roundtrip");
    check(decoded.value().profile(WorkflowId::pfsm).pfsm.candidate_recall_resource_fallback==PfsmResourceFallback::full_pool,"resource policy roundtrip");
    check(encodeWorkflowSettings(decoded.value())==encoded,"unknown fields and complete settings roundtrip");
    for(auto id:workflows) check(decoded.value().profile(id).background.coefficients==settings.profile(id).background.coefficients,"independent preparation profiles");
    auto invalid=settings; invalid.profile(WorkflowId::csm).csm.delta=-1;
    check(!store.save(invalid),"invalid write rejected"); check(store.load().value().profile(WorkflowId::csm).csm.delta==.08,"failed write retains old defaults");
    auto bad=encoded; bad["version"]=2; check(!decodeWorkflowSettings(bad),"unknown schema rejected");
    legacy.setValue("SearchOptions/minFom",.99); legacy.sync();
    check(store.load().value().profile(WorkflowId::csm).csm.minimum_fom==.61,"migration is one-time");
    ProductState isolated;
    check(qualpf::infrastructure::settings::SearchSettings::value(isolated,"SearchOptions/minFom",.35).toDouble()==.35,"execution never falls back to global settings");
    auto saved=encodeProject(a.session().snapshot(),{},"test"); check(bool(saved),"project encode");
    auto reopened=decodeProject(saved.value(),SearchBackend::cpp,{}); check(bool(reopened),"project decode");
    check(reopened.value().projectSettings->profile(WorkflowId::csm).csm.minimum_fom==.73,"saved project restores its options");
    a.session().reset(); check(a.session().snapshot()->projectSettings->profile(WorkflowId::csm).csm.minimum_fom==.61,"New reloads defaults");
    std::cout<<"workflow settings contracts passed\n";
}
