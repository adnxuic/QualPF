#pragma once
#include "qualpf/application/workflow_settings.h"
namespace qualpf::infrastructure {
class QtDefaultSettingsStore final : public application::DefaultSettingsStore {
  public:
    Result<application::ProjectSettings> load() const override;
    Status save(const application::ProjectSettings &) override;
    Status migrate();
};
}
