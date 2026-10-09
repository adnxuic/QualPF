#pragma once
#include <QList>
#include <QString>
namespace qualpf::infrastructure {
struct DatabaseEntry {
    bool inUse = false;
    QString name;
    int entries = 0;
    QString contentType;
    QString path;
};
class ReferenceDatabaseInfo {
  public:
    static int queryEntries(const QString &basePath);
    static QString queryContentType(const QString &basePath);
};
class DatabaseSettings {
  public:
    static QString defaultDbDir();
    static void setDefaultDbDir(const QString &);
    static bool directoryExists(const QString &);
    static void discover(QList<DatabaseEntry> &, const QString &);
    static void saveSettings(const QList<DatabaseEntry> &);
    static QList<DatabaseEntry> loadSettings();
};
} // namespace qualpf::infrastructure
