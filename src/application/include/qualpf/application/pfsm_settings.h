#pragma once
#include "qualpf/application/product_values.h"
#include "qualpf/profile/pfsm_candidate_recall.h"
namespace qualpf::application {
enum class PfsmResourceFallback { bounded_integral, full_pool };
struct PfsmSettings {
    enum class Weighting { automatic, equal };
    double lower_shift = -.5, upper_shift = .5, minimum_relative_fom = 0;
    Weighting weighting = Weighting::automatic;
    int threads = 0;
    bool template_cache = true;
    bool candidate_recall_enabled = true;
    PfsmResourceFallback candidate_recall_resource_fallback = PfsmResourceFallback::bounded_integral;
    profile::PfsmCandidateRecallLimits candidate_recall_limits;
};
Status validatePfsmSettings(const PfsmSettings &);
PfsmSettings pfsmSettings(const MetadataObject &, std::optional<Error> *recallWarning = nullptr);
void storePfsmSettings(MetadataObject &, const PfsmSettings &);
} // namespace qualpf::application
