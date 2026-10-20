#include "workflow_settings.h"
#include "preprocessing_config.h"
#include "metadata_values.h"
#include <QJsonArray>
namespace qualpf::infrastructure {
using namespace application;
namespace {
constexpr std::array keys{"preprocessing", "csm", "pfsm", "peak_search"};
}
QJsonObject encodeWorkflowSettings(const ProjectSettings &s) {
    auto root=jsonValue(MetadataValue{s.extensions}).toObject(); root["version"]=1;
    for(auto id:workflows) {
        const auto &p=s.profile(id);
        auto object=jsonValue(MetadataValue{p.extensions}).toObject();
        PreprocessingConfiguration c;
        c.background=p.background; c.initial_peaks=p.initial_peaks; c.fwhm=p.fwhm;
        c.peaks=p.peaks; c.selection=p.selection; c.smoothing=p.smoothing;
        // Retain unknown preprocessing fields without importing runtime evidence.
        if(object["preparation"].isObject()) {
            auto previous=decodePreprocessing(object["preparation"].toObject());
            if(previous) c.extensions=previous.value().extensions;
        }
        object["preparation"]=encodePreprocessing(c);
        if(p.range) object["range"]=QJsonArray{p.range->minimum.value,p.range->maximum.value};
        else object["range"]=QJsonValue::Null;
        auto cs=metadataValue(object["csm"].toObject()).toObject();
        for(const auto &[k,v]:csmSettingValues(p.csm)) cs[k]=v;
        object["csm"]=jsonValue(MetadataValue{cs});
        auto ps=metadataValue(object["pfsm"].toObject()).toObject(); storePfsmSettings(ps,p.pfsm);
        object["pfsm"]=jsonValue(MetadataValue{ps});
        root[keys[static_cast<std::size_t>(id)]]=object;
    }
    return root;
}
Result<ProjectSettings> decodeWorkflowSettings(const QJsonObject &root) {
    const auto fail=[] {return Result<ProjectSettings>{Error{ErrorCode::invalid_input,"settings.workflows","Invalid workflow settings","invalid_workflow_settings"}};};
    if(root.value("version").toInt(-1)!=1) return fail();
    ProjectSettings s; s.extensions=metadataValue(root).toObject();
    for(auto id:workflows) {
        const auto key=keys[static_cast<std::size_t>(id)];
        if(!root[key].isObject()) return fail();
        const auto object=root[key].toObject(); auto &p=s.profile(id);
        p.extensions=metadataValue(object).toObject();
        if(!object["preparation"].isObject()||!object["csm"].isObject()||!object["pfsm"].isObject()) return fail();
        auto decoded=decodePreprocessing(object["preparation"].toObject());
        if(!decoded) return Result<ProjectSettings>{decoded.error()};
        const auto &c=decoded.value();
        p.background=c.background.value_or(p.background); p.initial_peaks=c.initial_peaks.value_or(p.initial_peaks);
        p.fwhm=c.fwhm.value_or(p.fwhm); p.peaks=c.peaks.value_or(p.peaks);
        p.selection=c.selection.value_or(p.selection); p.smoothing=c.smoothing.value_or(p.smoothing);
        if(!object["range"].isNull()&&!object["range"].isUndefined()) {
            const auto r=object["range"].toArray();
            if(r.size()!=2||!r[0].isDouble()||!r[1].isDouble()) return fail();
            p.range=AngularRange{TwoThetaDegrees{r[0].toDouble()},TwoThetaDegrees{r[1].toDouble()}};
        }
        const auto cs=metadataValue(object["csm"].toObject()).toObject();
        for(const auto &[k,v]:csmSettingValues({})) {
            if(auto i=cs.find(k);i!=cs.end()) {
                if(std::holds_alternative<bool>(v.data) ? !std::holds_alternative<bool>(i->second.data) :
                    !(std::holds_alternative<double>(i->second.data)||std::holds_alternative<std::int64_t>(i->second.data))) return fail();
                if(std::holds_alternative<std::int64_t>(v.data) && i->second.toDouble()!=double(i->second.toInt(-1))) return fail();
            }
        }
        p.csm=csmSettings(cs);
        const auto ps=metadataValue(object["pfsm"].toObject()).toObject();
        MetadataObject expected; storePfsmSettings(expected,{});
        for(const auto &[k,v]:expected) if(auto i=ps.find(k);i!=ps.end()) {
            if(std::holds_alternative<bool>(v.data) && !std::holds_alternative<bool>(i->second.data)) return fail();
            if(std::holds_alternative<std::string>(v.data) && !std::holds_alternative<std::string>(i->second.data)) return fail();
            if(std::holds_alternative<double>(v.data)||std::holds_alternative<std::int64_t>(v.data)) {
                if(!std::holds_alternative<double>(i->second.data)&&!std::holds_alternative<std::int64_t>(i->second.data)) return fail();
                if(std::holds_alternative<std::int64_t>(v.data) && i->second.toDouble()!=double(i->second.toInt(-1))) return fail();
            }
        }
        std::optional<Error> warning;
        p.pfsm=pfsmSettings(ps,&warning);
        if(warning) return Result<ProjectSettings>{*warning};
        MetadataObject checked; storePfsmSettings(checked,p.pfsm);
        for(const auto &[k,v]:checked) if(auto i=ps.find(k);i!=ps.end()) {
            if(std::holds_alternative<double>(v.data)||std::holds_alternative<std::int64_t>(v.data)) {
                if(v.toDouble()!=i->second.toDouble()) return fail();
            } else if(v!=i->second) return fail();
        }
    }
    auto valid=validateProjectSettings(s);
    return valid ? Result<ProjectSettings>{std::move(s)} : Result<ProjectSettings>{valid.error()};
}
}
