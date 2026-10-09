#pragma once
#include "qualpf/application/environment.h"
#include <functional>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace qualpf::application {
struct ProgramRequest { std::string name,settingsKey,folder; };
struct ProgramLocation { bool found=false; std::string path; };
struct ProcessRequest { std::string executable; std::vector<std::string> arguments; };
struct ProcessResult { int exitCode=0,exitStatus=0; std::string standardOutput,standardError; };
struct ProcessCallbacks {
    std::function<void()> started;
    std::function<void(ProcessResult)> finished;
    std::function<void(int)> error;
};
class ProcessSession {
public:
    virtual ~ProcessSession() = default;
    virtual bool running() const = 0;
    virtual void start(ProcessRequest,ProcessCallbacks) = 0;
    virtual void detach(ProcessRequest) = 0;
};
struct BuildPlatformInfo { std::string creationDate, architecture, buildType, compiler, qtVersion; };
struct FileInformation {
    bool exists = false;
    std::optional<std::int64_t> lastModifiedMilliseconds;
};
class PlatformServices : public EnvironmentReader {
public:
    virtual ~PlatformServices() = default;
    virtual std::unique_ptr<ProcessSession> makeProcess() = 0;
    virtual std::string maintenanceExecutable() const = 0;
    virtual ProgramLocation locateProgram(ProgramRequest) = 0;
    virtual bool fileExists(const std::string &) const = 0;
    virtual FileInformation fileInformation(const std::string &) const = 0;
    virtual bool makeDirectory(const std::string &) = 0;
    virtual std::string documentationPath() = 0;
    virtual std::string newDatabaseDirectory() const = 0;
    virtual BuildPlatformInfo buildInformation() const = 0;
};
struct MaintenanceOutcome {
    bool notify=false,warning=false,available=false;
    std::string details,latestVersion;
};
ProcessRequest maintenanceRequest(const PlatformServices &,bool check);
MaintenanceOutcome maintenanceOutcome(const ProcessResult &);
} // namespace qualpf::application
