#pragma once
#include "qualpf/profile/pfsm.h"

namespace qualpf::profile {
// Paired approximate value/derivative sampling for numerical diagnostics and recall.
// This does not replace IProfileTemplate::evaluate/evaluateExact formal semantics.
Result<ProfileSamples> evaluateProfilePairedSamples(const ProfileTemplate &, std::span<const double>,
    double shift, const ExecutionContext &);
} // namespace qualpf::profile
