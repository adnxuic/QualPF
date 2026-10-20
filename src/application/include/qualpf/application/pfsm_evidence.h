#pragma once
#include "qualpf/profile/pfsm.h"

namespace qualpf::application {
// Publishable evidence from the formal candidate fit; no recall diagnostics.
struct PfsmCandidateEvidence {
    std::size_t original_index = 0;
    std::uint64_t generation = 0;
    profile::ProfileFitParameters parameters;
    profile::ProfileFitDiagnostics diagnostics;
    profile::ProfileFitTermination termination = profile::ProfileFitTermination::fixed_shifts;
    double q = 0, rwp_net = 0, delta_q = 0, delta_rwp = 0;
};
} // namespace qualpf::application
