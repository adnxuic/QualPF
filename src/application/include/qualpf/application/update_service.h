#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace qualpf::application {
struct UpdateRequest { std::string url, userAgent; };
struct UpdateReply {
    std::string redirect;
    bool networkError = false;
    std::string body;
    std::string errorDescription;
};
struct UpdateDocument {
    bool valid = false;
    std::string openUrl, changelog, downloadUrl, latestVersion;
    std::vector<int> latestParts, currentParts;
};
class UpdateSession {
public:
    virtual ~UpdateSession() = default;
    virtual void get(UpdateRequest, std::function<void(UpdateReply)>) = 0;
};
class UpdateServiceProvider {
public:
    virtual ~UpdateServiceProvider() = default;
    virtual std::unique_ptr<UpdateSession> makeSession() = 0;
    virtual UpdateDocument decode(const std::string &body, const std::string &platform,
                                  const std::string &currentVersion) const = 0;
};
struct UpdateOutcome {
    std::string redirect;
    bool hasDocument = false, available = false;
    UpdateDocument document;
};
UpdateOutcome evaluateUpdateReply(const UpdateServiceProvider &, const UpdateReply &,
                                 const std::string &platform, const std::string &currentVersion);
} // namespace qualpf::application
