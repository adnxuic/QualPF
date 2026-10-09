#pragma once
#include "qualpf/application/platform_services.h"
namespace qualpf::infrastructure {
class QtPlatformServices final : public application::PlatformServices {
public:
    std::string environmentValue(const std::string &) const override;
    std::unique_ptr<application::ProcessSession> makeProcess() override;
    std::string maintenanceExecutable() const override;
    application::ProgramLocation locateProgram(application::ProgramRequest) override;
    bool fileExists(const std::string &) const override;
    application::FileInformation fileInformation(const std::string &) const override;
    bool makeDirectory(const std::string &) override;
    std::string documentationPath() override;
    std::string newDatabaseDirectory() const override;
    application::BuildPlatformInfo buildInformation() const override;
};
}
