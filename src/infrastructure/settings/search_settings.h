#pragma once
#include "qualpf/application/workspace_ports.h"
#include "qualpf/application/settings_keys.h"
#include <QVariant>
namespace qualpf::infrastructure::settings {
using namespace application::settings;
class SearchSettings final : public application::SearchSettingsProvider {
  public:
    application::PreferencePayload option(application::ProductHandle, const std::string &, const application::PreferencePayload &) const override;
    static QVariant value(const application::ProductState &, const char *, const QVariant &);
    Result<application::SearchSettingsSnapshot> load(application::ProductHandle) const override;
};
} // namespace qualpf::infrastructure::settings
