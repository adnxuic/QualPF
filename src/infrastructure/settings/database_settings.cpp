#include "infrastructure/settings/database_settings.h"
#include "infrastructure/settings/progkeysettings.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
namespace qualpf::infrastructure {
// static
void DatabaseSettings::saveSettings(const QList<DatabaseEntry> &databases) {
    QSettings s;
    s.setValue(DB_COUNT_KEY, databases.size());
    for (int i = 0; i < databases.size(); ++i) {
        s.setValue(QString(DB_INUSE_KEY).arg(i), databases[i].inUse);
        s.setValue(QString(DB_NAME_KEY).arg(i), databases[i].name);
        s.setValue(QString(DB_PATH_KEY).arg(i), databases[i].path);
    }
}

// static
QList<DatabaseEntry> DatabaseSettings::loadSettings() {
    QSettings s;
    const int count = s.value(DB_COUNT_KEY, 0).toInt();
    QList<DatabaseEntry> list;
    list.reserve(count);
    for (int i = 0; i < count; ++i) {
        DatabaseEntry e;
        e.inUse = s.value(QString(DB_INUSE_KEY).arg(i), false).toBool();
        e.name = s.value(QString(DB_NAME_KEY).arg(i)).toString();
        e.path = s.value(QString(DB_PATH_KEY).arg(i)).toString();
        e.entries = ReferenceDatabaseInfo::queryEntries(e.path);
        e.contentType = ReferenceDatabaseInfo::queryContentType(e.path);
        list.append(e);
    }
    return list;
}

QString ReferenceDatabaseInfo::queryContentType(const QString &basePath) {
    const QString connName =
        QStringLiteral("dbbuilder_type_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    QString type;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(basePath + ".sq");
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (db.open()) {
            QSqlQuery q(db);
            if (q.exec("SELECT type FROM infodb") && q.first())
                type = q.value(0).toString().trimmed();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return type;
}

int ReferenceDatabaseInfo::queryEntries(const QString &basePath) {
    // Use a unique connection name to avoid conflicts with open connections.
    const QString connName = QStringLiteral("dbbuilder_%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    int ncard = -1;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
        db.setDatabaseName(basePath + ".sq");
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (db.open()) {
            QSqlQuery q(db);
            if (q.exec("SELECT ncard FROM infodb") && q.first())
                ncard = q.value(0).toInt();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connName);
    return ncard;
}

QString DatabaseSettings::defaultDbDir() {
    QSettings s;
    return s.value(DB_DEFAULT_DIR_KEY, QDir::homePath() + "/QualXDB").toString();
}

void DatabaseSettings::setDefaultDbDir(const QString &dir) {
    QSettings s;
    s.setValue(DB_DEFAULT_DIR_KEY, dir);
}

void DatabaseSettings::discover(QList<DatabaseEntry> &databases, const QString &dir) {
    QSet<QString> known;
    for (const DatabaseEntry &e : databases)
        known.insert(e.path);

    const int initialSize = databases.size();

    QDirIterator it(dir, QStringList{QStringLiteral("*.sq")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString sqFile = it.next();
        const QString base = sqFile.chopped(3); // strip ".sq"
        if (known.contains(base))
            continue;

        DatabaseEntry e;
        // The first newly discovered database becomes active only if the list
        // was empty before this scan (no databases were configured at all).
        e.inUse = (initialSize == 0) && (databases.size() == initialSize);
        e.name = QFileInfo(sqFile).baseName();
        e.path = base;
        e.entries = qualpf::infrastructure::ReferenceDatabaseInfo::queryEntries(base);
        e.contentType = qualpf::infrastructure::ReferenceDatabaseInfo::queryContentType(base);
        databases.append(e);
    }

    if (databases.size() > initialSize)
        qualpf::infrastructure::DatabaseSettings::saveSettings(databases);
}

bool DatabaseSettings::directoryExists(const QString &dir) { return QDir(dir).exists(); }
} // namespace qualpf::infrastructure
