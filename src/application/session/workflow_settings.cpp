#include "qualpf/application/workflow_settings.h"
#include "qualpf/application/settings_keys.h"
#include <cmath>
namespace qualpf::application {
using namespace settings;
const char *workflowName(WorkflowId id) {
    switch (id) {
    case WorkflowId::preprocessing: return "Preprocessing";
    case WorkflowId::csm: return "CSM";
    case WorkflowId::pfsm: return "PFSM";
    case WorkflowId::peak_search: return "Peak Search";
    }
    return "Unknown";
}
ProjectSettings::ProjectSettings() { profile(WorkflowId::pfsm).fwhm.method = FwhmMethod::geometric; }
MetadataObject csmSettingValues(const CsmSettings &c) {
    return {{SK_FOM,c.minimum_fom},{SK_2THETA,c.weight_position},{SK_INT,c.weight_intensity},
        {SK_PHASES,c.weight_phases},{SK_DELTA,c.delta},{SK_AUTO,c.automatic_delta},
        {SK_RESIDUAL,c.residual},{SK_STRONGEST,c.strongest},{SK_DELETED,c.exclude_deleted},{SK_MAXENT,c.maximum_entries}};
}
CsmSettings csmSettings(const MetadataObject &m) {
    CsmSettings c;
    const auto real = [&](const char *key, double &v) { if (auto i=m.find(key); i!=m.end()) v=i->second.toDouble(v); };
    const auto boolean = [&](const char *key, bool &v) { if (auto i=m.find(key); i!=m.end()) v=i->second.toBool(v); };
    real(SK_FOM,c.minimum_fom); real(SK_2THETA,c.weight_position); real(SK_INT,c.weight_intensity);
    real(SK_PHASES,c.weight_phases); real(SK_DELTA,c.delta); boolean(SK_AUTO,c.automatic_delta);
    boolean(SK_RESIDUAL,c.residual); boolean(SK_STRONGEST,c.strongest); boolean(SK_DELETED,c.exclude_deleted);
    if(auto i=m.find(SK_MAXENT);i!=m.end()) c.maximum_entries=i->second.toInt(c.maximum_entries);
    return c;
}
Status validateProjectSettings(const ProjectSettings &settings) {
    for (auto id : workflows) {
        const auto &p=settings.profile(id);
        PreprocessingConfiguration c;
        c.background=p.background; c.initial_peaks=p.initial_peaks; c.fwhm=p.fwhm;
        c.peaks=p.peaks; c.selection=p.selection; c.smoothing=p.smoothing;
        auto valid=validatePreprocessing(c); if(!valid) return valid;
        valid=validatePfsmSettings(p.pfsm); if(!valid) return valid;
        const auto &s=p.csm;
        for(auto value : {s.minimum_fom,s.weight_position,s.weight_intensity,s.weight_phases})
            if(!std::isfinite(value)||value<0||value>1)
                return Status{Error{ErrorCode::invalid_input,"settings.csm","CSM weights and minimum FOM must be between 0 and 1"}};
        if(!std::isfinite(s.delta)||s.delta<0||s.delta>180||s.maximum_entries<1)
            return Status{Error{ErrorCode::invalid_input,"settings.csm","Invalid CSM tolerance or maximum entries"}};
        if(p.range && (!std::isfinite(p.range->minimum.value)||!std::isfinite(p.range->maximum.value)||
            p.range->minimum.value<0||p.range->maximum.value>180||p.range->minimum.value>=p.range->maximum.value))
            return Status{Error{ErrorCode::invalid_input,"settings.range","Range must be ordered within 0 to 180 degrees"}};
    }
    return Status{std::monostate{}};
}
} // namespace qualpf::application
