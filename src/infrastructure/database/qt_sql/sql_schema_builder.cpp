#include "infrastructure/database/qt_sql/sql_schema_builder.h"
#include "sql_schema.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QFile>
#include <QDebug>
#include <QUuid>

namespace qualpf::infrastructure {

// -----------------------------------------------------------------------
// Construction / destruction
// -----------------------------------------------------------------------

SqlSchemaBuilder::SqlSchemaBuilder(QObject *parent)
    : QObject(parent)
    , m_instanceId(QUuid::createUuid().toString(QUuid::Id128))
{}

SqlSchemaBuilder::~SqlSchemaBuilder()
{
    close();
}

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

bool SqlSchemaBuilder::create(const QString &basePath, DbType type)
{
    if (owner_ != std::this_thread::get_id()) return false;
    close();
    writeLease_ = qualpf::infrastructure::DatabaseLease::write(basePath);
    if (!writeLease_) return false;
    m_basePath = basePath;
    m_type     = type;

    const QStringList suffixes = { ".sq", ".sq.info", ".sq.infostat", ".sq.search" };
    for (const QString &s : suffixes)
        QFile::remove(basePath + s);

    auto openDb = [&](const QString &suffix) -> bool {
        const QString name = connName(suffix);
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(basePath + suffix);
        if (!db.open()) {
            qWarning() << "QualxDbCreator: cannot open" << basePath + suffix
                       << db.lastError().text();
            return false;
        }
        return true;
    };

    if (!openDb(".sq") || !openDb(".sq.info") || !openDb(".sq.infostat") || !openDb(".sq.search")) {
        close(); return false;
    }

    if (!createMainSchema())     { close(); return false; }
    if (!createInfoSchema())     { close(); return false; }
    if (!createInfoStatSchema()) { close(); return false; }
    if (!createSearchSchema())   { close(); return false; }

    return true;
}

void SqlSchemaBuilder::close()
{
    Q_ASSERT_X(owner_ == std::this_thread::get_id(), "SqlSchemaBuilder", "SQL writer must close on its execution thread");
    if (owner_ != std::this_thread::get_id()) return;
    const QStringList suffixes = { ".sq", ".sq.info", ".sq.infostat", ".sq.search" };
    for (const QString &s : suffixes) {
        const QString name = connName(s);
        if (QSqlDatabase::contains(name)) {
            QSqlDatabase::database(name).close();
            QSqlDatabase::removeDatabase(name);
        }
    }
    writeLease_.reset();
}

bool SqlSchemaBuilder::isOpen() const
{
    if (owner_ != std::this_thread::get_id()) return false;
    return QSqlDatabase::contains(connName(".sq"))
        && QSqlDatabase::database(connName(".sq")).isOpen();
}

QSqlDatabase SqlSchemaBuilder::mainDb()     const { if (owner_ != std::this_thread::get_id()) return {}; return QSqlDatabase::database(connName(".sq")); }
QSqlDatabase SqlSchemaBuilder::infoDb()     const { if (owner_ != std::this_thread::get_id()) return {}; return QSqlDatabase::database(connName(".sq.info")); }
QSqlDatabase SqlSchemaBuilder::infoStatDb() const { if (owner_ != std::this_thread::get_id()) return {}; return QSqlDatabase::database(connName(".sq.infostat")); }
QSqlDatabase SqlSchemaBuilder::searchDb()   const { if (owner_ != std::this_thread::get_id()) return {}; return QSqlDatabase::database(connName(".sq.search")); }

// -----------------------------------------------------------------------
// Schema creation – main (.sq)
//
// Differences between Pdf2 and CifFiles:
//   id table:  Pdf2 has bestdval; CifFiles has natoms + nreflections instead.
//   infodb.id: Pdf2 is INTEGER; CifFiles is VARCHAR(100).
//   warn / warningcif: CifFiles only.
// -----------------------------------------------------------------------

bool SqlSchemaBuilder::createMainSchema() { return execStatements(mainDb(), mainSchema(m_type == DbType::Pdf2)); }

// -----------------------------------------------------------------------
// Schema creation – info (.sq.info)
//
// Pdf2: contains only spgrstat.
// CifFiles:  contains the full info table (with natoms, nreflections, REAL
//       types for density/volume) plus spgrstat.
// -----------------------------------------------------------------------

bool SqlSchemaBuilder::createInfoSchema() { return execStatements(infoDb(), infoSchema()); }

// -----------------------------------------------------------------------
// Schema creation – infostat (.sq.infostat)   [identical for both types]
// -----------------------------------------------------------------------

bool SqlSchemaBuilder::createInfoStatSchema() { return execStatements(infoStatDb(), infoStatSchema()); }

// -----------------------------------------------------------------------
// Schema creation – search (.sq.search)   [identical for both types]
// -----------------------------------------------------------------------

bool SqlSchemaBuilder::createSearchSchema() { return execStatements(searchDb(), searchSchema()); }

// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

bool SqlSchemaBuilder::execStatements(const QSqlDatabase &db, const QStringList &statements)
{
    QSqlQuery q(db);
    for (const QString &sql : statements) {
        if (!q.exec(sql)) {
            qWarning() << "QualxDbCreator SQL error:" << q.lastError().text()
                       << "\nStatement:" << sql.left(120);
            return false;
        }
    }
    return true;
}

QString SqlSchemaBuilder::connName(const QString &suffix) const
{
    return QStringLiteral("qualx_") + m_instanceId + suffix;
}

} // namespace qualpf::infrastructure
