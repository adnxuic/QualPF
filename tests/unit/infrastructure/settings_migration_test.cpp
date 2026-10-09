#include "infrastructure/settings/settings_migration.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSettings>
#include <QTemporaryDir>
#include <QUuid>

#include <cstdlib>
#include <iostream>

void opaquePreferenceChecks(const QString &);

namespace {
void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

QByteArray bytes(const QString &path)
{
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "read retained legacy settings");
    return file.readAll();
}

void fileStoreChecks(const QString &root)
{
    using qualpf::infrastructure::migrateUserSettings;
    const auto sourcePath = root + "/old.ini";
    const auto targetPath = root + "/new.ini";
    {
        QSettings source(sourcePath, QSettings::IniFormat);
        source.setValue("databases/defaultDir", QStringLiteral("C:/Unicode/晶体"));
        source.setValue("search/delta", 0.125);
        source.setValue("flags/enabled", true);
        source.setValue("window/state", QByteArray::fromHex("0001ff"));
        source.beginWriteArray("databases/list");
        source.setArrayIndex(0);
        source.setValue("name", "COD");
        source.setArrayIndex(1);
        source.setValue("name", "custom");
        source.endArray();
        source.sync();
        require(source.status() == QSettings::NoError, "prepare typed legacy preferences");
    }
    const auto retained = bytes(sourcePath);
    {
        QSettings source(sourcePath, QSettings::IniFormat);
        QSettings destination(targetPath, QSettings::IniFormat);
        destination.setValue("search/delta", 0.5);
        const auto result = migrateUserSettings(source, destination);
        require(result.completed && result.error.isEmpty(), "migrate typed settings");
        require(destination.value("search/delta").toDouble() == 0.5, "new preference wins");
        for (const auto &key : source.allKeys()) {
            if (key != "search/delta")
                require(destination.value(key) == source.value(key), "preserve key value and type");
        }
        require(destination.beginReadArray("databases/list") == 2, "preserve array length");
        destination.setArrayIndex(1);
        require(destination.value("name").toString() == "custom", "preserve array entry");
        destination.endArray();
        destination.remove("flags/enabled");
        destination.sync();
        require(migrateUserSettings(source, destination).completed, "repeat startup succeeds");
        require(!destination.contains("flags/enabled"), "do not reimport deleted preferences");
    }
    require(bytes(sourcePath) == retained, "legacy INI bytes unchanged");
    {
        QSettings absent(root + "/absent.ini", QSettings::IniFormat);
        QSettings fresh(root + "/fresh.ini", QSettings::IniFormat);
        require(migrateUserSettings(absent, fresh).completed, "fresh installation succeeds");
        require(!QFile::exists(absent.fileName()), "fresh migration does not create old store");
    }
    const auto blockedPath = root + "/blocked.ini";
    require(QDir().mkpath(blockedPath), "create deterministic unwritable destination");
    {
        QSettings source(sourcePath, QSettings::IniFormat);
        QSettings blocked(blockedPath, QSettings::IniFormat);
        const auto result = migrateUserSettings(source, blocked);
        require(!result.completed && !result.error.isEmpty(), "report destination failure");
    }
    require(QDir(blockedPath).removeRecursively(), "remove test-owned destination obstacle");
    {
        QSettings source(sourcePath, QSettings::IniFormat);
        QSettings retry(blockedPath, QSettings::IniFormat);
        require(migrateUserSettings(source, retry).completed, "retry after write failure");
        require(retry.value("flags/enabled").toBool(), "retry imports missing preferences");
    }
    require(bytes(sourcePath) == retained, "failed migration never writes old store");
    const auto malformedPath = root + "/malformed.ini";
    {
        QFile malformed(malformedPath);
        require(malformed.open(QIODevice::WriteOnly), "prepare malformed source");
        malformed.write("[broken\nkey=value\n");
    }
    {
        QSettings malformed(malformedPath, QSettings::IniFormat);
        QSettings destination(root + "/malformed-target.ini", QSettings::IniFormat);
        const auto result = migrateUserSettings(malformed, destination);
        require(!result.completed && !result.error.isEmpty(), "report unreadable source format");
        require(!destination.contains("maintenance/settingsMigrationV1"), "do not seal failed read");
    }
}

void systemFallbackChecks(const QString &root)
{
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, root + "/user");
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, root + "/system");
    const QString organization = "QualPF.SystemFallback.Test";
    QSettings defaults(QSettings::IniFormat, QSettings::SystemScope, organization, "previous");
    defaults.setValue("systemOnly", 19);
    defaults.sync();
    QSettings source(QSettings::IniFormat, QSettings::UserScope, organization, "previous");
    QSettings destination(QSettings::IniFormat, QSettings::UserScope, organization, "qualpf");
    require(source.value("systemOnly").toInt() == 19, "prepare actual system fallback");
    require(qualpf::infrastructure::migrateUserSettings(source, destination).completed,
            "migration with system fallback succeeds");
    require(!destination.contains("systemOnly"), "exclude system defaults from user import");
    require(!QFile::exists(source.fileName()), "do not create missing legacy user store");
}

void nativeStoreChecks()
{
    const auto organization = "QualPF.SettingsMigration.Test." + QUuid::createUuid().toString();
    QSettings organizationDefaults(QSettings::NativeFormat, QSettings::UserScope, organization);
    QSettings source(QSettings::NativeFormat, QSettings::UserScope, organization, "qualxpf");
    QSettings destination(QSettings::NativeFormat, QSettings::UserScope, organization, "qualpf");
    organizationDefaults.setValue("organizationOnly", 17);
    organizationDefaults.sync();
    source.setValue("owned", QByteArray("native typed value"));
    source.sync();
    const auto result = qualpf::infrastructure::migrateUserSettings(source, destination);
    require(result.completed && result.error.isEmpty(), "native platform store migration");
    require(destination.value("owned") == source.value("owned"), "native value retained");
    require(!destination.contains("organizationOnly"), "exclude organization fallback");
    // Only this UUID-owned fixture is removed; no actual user preferences participate.
    source.clear();
    destination.clear();
    organizationDefaults.clear();
    source.sync();
    destination.sync();
    organizationDefaults.sync();
}

void actualStartupChecks(const QString &root)
{
    const auto settingsRoot = root + "/cli-settings";
    require(QDir().mkpath(settingsRoot + "/IC"), "prepare explicit settings directory");
    const auto sourcePath = settingsRoot + "/IC/qualxpf.ini";
    const auto targetPath = settingsRoot + "/IC/qualpf.ini";
    {
        QSettings source(sourcePath, QSettings::IniFormat);
        source.setValue("migration-test/sentinel", QByteArray("actual process"));
        source.sync();
    }
    const auto retained = bytes(sourcePath);
    const auto run = [&](const QStringList &arguments, int expected) {
        QProcess process;
        process.start(QString::fromUtf8(QUALPF_PRODUCT_EXECUTABLE), arguments);
        require(process.waitForFinished(30000) && process.exitStatus() == QProcess::NormalExit,
                "actual current executable finishes");
        const auto output = process.readAllStandardOutput() + process.readAllStandardError();
        if (process.exitCode() != expected)
            std::cerr << output.constData() << '\n';
        require(process.exitCode() == expected, "actual startup exit code");
        return output;
    };
    require(run({"--nogui", "--settings-dir", settingsRoot, "--version"}, 0).contains("qualpf Version"),
            "version uses current executable identity");
    run({"--nogui", "--settings-dir", settingsRoot, "--help"}, 0);
    run({"--nogui", "--settings-dir", settingsRoot, "--invalid-option"}, 1);
    require(!QFile::exists(targetPath), "early exits do not migrate settings");
    const auto input = root + "/cifs";
    require(QDir().mkpath(input), "prepare actual builder input");
    require(QFile::copy(QString::fromUtf8(QUALPF_TEST_CIF), input + "/CoO.cif"),
            "copy unchanged CIF fixture");
    run({"--nogui", "--settings-dir", settingsRoot, "--createdb", "--cifdir",
         input, "--dbout", root + "/output"}, 0);
    QSettings destination(targetPath, QSettings::IniFormat);
    require(destination.value("migration-test/sentinel").toByteArray() == "actual process",
            "explicit settings directory honored before application reads");
    require(bytes(sourcePath) == retained, "actual startup preserves old settings bytes");
}
} // namespace

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary;
    require(temporary.isValid(), "isolated migration fixtures");
    fileStoreChecks(temporary.path());
    systemFallbackChecks(temporary.path());
    nativeStoreChecks();
    actualStartupChecks(temporary.path());
    opaquePreferenceChecks(temporary.path());
    std::cout << "PASS: user settings migration, types/arrays, precedence, retry, native "
                 "fallback exclusion and actual startup/early-exit isolation\n";
}
