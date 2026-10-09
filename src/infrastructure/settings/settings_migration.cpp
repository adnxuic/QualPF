#include "infrastructure/settings/settings_migration.h"

#include <QCoreApplication>
#include <QSettings>

namespace qualpf::infrastructure {
namespace {
const QString migrationKey = QStringLiteral("maintenance/settingsMigrationV1");

SettingsMigrationResult failure(const QSettings &settings)
{
    return {false, QStringLiteral("Could not migrate user settings (%1). "
                                  "The old settings are retained; migration will retry on startup.")
                       .arg(settings.fileName())};
}
} // namespace

SettingsMigrationResult migrateUserSettings(QSettings &source, QSettings &destination)
{
    // Only the application-specific user stores participate. Organization and
    // system defaults must not become permanent user preferences during a rename.
    source.setFallbacksEnabled(false);
    destination.setFallbacksEnabled(false);
    destination.sync();
    if (destination.status() != QSettings::NoError)
        return failure(destination);
    if (destination.value(migrationKey, false).toBool())
        return {true, {}};

    source.sync();
    if (source.status() != QSettings::NoError)
        return failure(source);

    for (const auto &key : source.allKeys()) {
        if (key != migrationKey && !destination.contains(key))
            destination.setValue(key, source.value(key));
    }
    destination.sync();
    if (destination.status() != QSettings::NoError)
        return failure(destination);

    destination.setValue(migrationKey, true);
    destination.sync();
    if (destination.status() != QSettings::NoError) {
        destination.remove(migrationKey);
        return failure(destination);
    }
    return {true, {}};
}

SettingsMigrationResult migrateLegacyUserSettings()
{
    // This is the only production use of the old product identity: read-only
    // import from the same platform/format/path selected for the current user.
    QSettings source(QSettings::defaultFormat(), QSettings::UserScope,
                     QCoreApplication::organizationName(), QStringLiteral("qualxpf"));
    QSettings destination;
    return migrateUserSettings(source, destination);
}

} // namespace qualpf::infrastructure
