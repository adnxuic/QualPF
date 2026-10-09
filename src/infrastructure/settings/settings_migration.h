#pragma once

#include <QString>

class QSettings;

namespace qualpf::infrastructure {

struct SettingsMigrationResult {
    bool completed = false;
    QString error;
};

// Internal Qt persistence boundary; numerical Core never depends on this API.
SettingsMigrationResult migrateUserSettings(QSettings &source, QSettings &destination);
SettingsMigrationResult migrateLegacyUserSettings();

} // namespace qualpf::infrastructure
