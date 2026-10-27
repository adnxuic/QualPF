#pragma once
#include "qualpf/application/platform_services.h"
#include <QTemporaryDir>
namespace qualpf::infrastructure {
class QtPlatformServices final : public application::PlatformServices {
    // Keep external viewers' copies available for this provider's lifetime.
    std::unique_ptr<QTemporaryDir> documentationDirectory_;
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
