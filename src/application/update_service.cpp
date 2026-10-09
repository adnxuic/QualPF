#include "qualpf/application/update_service.h"
#include <algorithm>

namespace qualpf::application {
UpdateOutcome evaluateUpdateReply(const UpdateServiceProvider &provider, const UpdateReply &reply,
                                 const std::string &platform, const std::string &currentVersion) {
    UpdateOutcome outcome;
    if (!reply.redirect.empty()) {
        outcome.redirect = reply.redirect;
        return outcome;
    }
    if (reply.networkError) return outcome;
    outcome.document = provider.decode(reply.body, platform, currentVersion);
    outcome.hasDocument = outcome.document.valid;
    if (!outcome.hasDocument) return outcome;
    const auto &latest = outcome.document.latestParts;
    const auto &current = outcome.document.currentParts;
    for (std::size_t i = 0; i < std::min(latest.size(), current.size()); ++i) {
        if (latest[i] != current[i]) {
            outcome.available = latest[i] > current[i];
            return outcome;
        }
    }
    outcome.available = current.size() < latest.size();
    return outcome;
}
} // namespace qualpf::application
