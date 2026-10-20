#include "infrastructure/settings/database_catalog_store.h"
#include "infrastructure/settings/database_settings.h"
#include "infrastructure/database/database_access.h"
#include <QFile>
#include "database_identity.h"
namespace qualpf::infrastructure {
namespace {
class QtDatabaseRemoval final : public application::DatabaseRemoval {
    QString base_;
    std::unique_ptr<DatabaseLease> lease_;
public:
    QtDatabaseRemoval(QString base, std::unique_ptr<DatabaseLease> lease) : base_(std::move(base)), lease_(std::move(lease)) {}
    std::vector<std::string> removeFiles() override {
        std::vector<std::string> failed;
        for (const auto *suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"}) {
            QFile file(base_ + suffix);
            if (file.exists() && !file.remove()) failed.push_back(file.fileName().toStdString());
        }
        return failed;
    }
};
std::vector<application::ReferenceDatabaseEntry> values(const QList<DatabaseEntry> &entries) {
    std::vector<application::ReferenceDatabaseEntry> out;
    for (const auto &e : entries)
        out.push_back({e.inUse, e.name.toStdString(), e.entries, e.contentType.toStdString(), e.path.toStdString()});
    return out;
}
QList<DatabaseEntry> qtValues(const std::vector<application::ReferenceDatabaseEntry> &entries) {
    QList<DatabaseEntry> out;
    for (const auto &e : entries)
        out.append({e.inUse, QString::fromStdString(e.name), e.entries,
                    QString::fromStdString(e.contentType), QString::fromStdString(e.path)});
    return out;
}
}
std::vector<application::ReferenceDatabaseEntry> DatabaseCatalogSettings::load() const { return values(DatabaseSettings::loadSettings()); }
void DatabaseCatalogSettings::save(const std::vector<application::ReferenceDatabaseEntry> &v) { DatabaseSettings::saveSettings(qtValues(v)); }
std::string DatabaseCatalogSettings::defaultDirectory() const { return DatabaseSettings::defaultDbDir().toStdString(); }
void DatabaseCatalogSettings::setDefaultDirectory(const std::string &s) { DatabaseSettings::setDefaultDbDir(QString::fromStdString(s)); }
bool DatabaseCatalogSettings::directoryExists(const std::string &s) const { return DatabaseSettings::directoryExists(QString::fromStdString(s)); }
void DatabaseCatalogSettings::discover(std::vector<application::ReferenceDatabaseEntry> &v, const std::string &s) {
    auto qt = qtValues(v); DatabaseSettings::discover(qt, QString::fromStdString(s)); v = values(qt);
}
std::unique_ptr<application::DatabaseRemoval> DatabaseCatalogSettings::prepareRemoval(const std::string &base) {
    const auto path = QString::fromStdString(base);
    auto lease = DatabaseLease::write(path);
    return lease ? std::make_unique<QtDatabaseRemoval>(path, std::move(lease)) : nullptr;
}
std::string DatabaseCatalogSettings::identity(const std::string &path) const { return databaseFileIdentity(path); }
std::pair<int, std::string> DatabaseCatalogSettings::describe(const std::string &s) const {
    const auto path = QString::fromStdString(s);
    return {ReferenceDatabaseInfo::queryEntries(path), ReferenceDatabaseInfo::queryContentType(path).toStdString()};
}
} // namespace qualpf::infrastructure
