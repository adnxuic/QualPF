#include "infrastructure/database/qt_sql/sql_connection.h"

#include <QDebug>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <limits>

namespace qualpf::infrastructure {

SqlConnection::SqlConnection() : SqlConnection(QUuid::createUuid().toString(QUuid::Id128), "reference", {}) {}

SqlConnection::SqlConnection(const QString &path) : SqlConnection() { openDb(path); }
SqlConnection::SqlConnection(QString scope, QString role, qualpf::infrastructure::ConnectionTrace trace)
    : scope_(std::move(scope)), role_(std::move(role)), trace_(std::move(trace)) {}
void SqlConnection::record(qualpf::infrastructure::ConnectionAction action) const noexcept {
    if (trace_)
        try {
            trace_({action, m_connName.toStdString(), role_.toStdString(), std::this_thread::get_id()});
        } catch (...) {
        }
}

void SqlConnection::closeDb() {
    if (m_connName.isEmpty())
        return;
    Q_ASSERT_X(onOwnerThread(), "SqlConnection", "SQL connections must be destroyed on their execution thread");
    if (!onOwnerThread())
        return;
    {
        QSqlDatabase db = QSqlDatabase::database(m_connName, /*open=*/false);
        if (db.isOpen())
            db.close();
    } // db copy destroyed here, before removeDatabase()
    QSqlDatabase::removeDatabase(m_connName);
    record(qualpf::infrastructure::ConnectionAction::removed);
    m_connName.clear();
}

SqlConnection::~SqlConnection() { closeDb(); }

bool SqlConnection::openDb(const QString &path) {
    if (!onOwnerThread() || generation_ == std::numeric_limits<std::uint64_t>::max())
        return false;
    closeDb(); // close any previously open connection
    if (!QFileInfo(path).isFile()) {
        qWarning() << "Database file does not exist:" << path;
        return false;
    }
    m_connName = "qualpf_" + scope_ + "_" + role_ + "_" + QString::number(++generation_);
    QString error;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_connName);
        db.setDatabaseName(path);
        // This manager only queries reference databases; creation has a
        // separate writer. Never create a missing database during search.
        db.setConnectOptions("QSQLITE_OPEN_READONLY");
        if (!db.open()) {
            error = db.lastError().text();
        }
    }
    if (!error.isEmpty()) {
        qWarning() << "Database open failed:" << path << error;
        QSqlDatabase::removeDatabase(m_connName);
        m_connName.clear();
        return false;
    }
    record(qualpf::infrastructure::ConnectionAction::opened);
    return true;
}

bool SqlConnection::isOpen() const {
    if (!onOwnerThread() || m_connName.isEmpty())
        return false;
    return QSqlDatabase::database(m_connName, /*open=*/false).isOpen();
}

QSqlDatabase SqlConnection::db() const {
    if (!onOwnerThread() || m_connName.isEmpty())
        return QSqlDatabase();
    record(qualpf::infrastructure::ConnectionAction::accessed);
    return QSqlDatabase::database(m_connName, /*open=*/false);
}

void SqlConnection::debugDatabaseInfo() {
    qDebug() << "\n=== DATABASE DEBUG INFO ===";
    qDebug() << "Connection name:" << db().connectionName();
    qDebug() << "Database name:" << db().databaseName();
    qDebug() << "Is open?" << db().isOpen();
    qDebug() << "Last error:" << db().lastError().text();

    QSqlQuery query(db());
    if (query.exec("PRAGMA database_list")) {
        qDebug() << "\nAttached databases:";
        while (query.next()) {
            qDebug() << "Seq:" << query.value(0).toInt() << "Name:" << query.value(1).toString()
                     << "File:" << query.value(2).toString();
        }
    }

    if (query.exec("SELECT * FROM sqlite_master WHERE name='id'")) {
        if (query.next()) {
            qDebug() << "\nTable 'id' exists, type:" << query.value("type").toString();
            qDebug() << "SQL:" << query.value("sql").toString();
        } else {
            qDebug() << "\nTable 'id' DOES NOT EXIST in this database connection";
        }
    }

    qDebug() << "\n=== END DEBUG INFO ===\n";
}

int SqlConnection::queryForCount(const QString &queryString) {
    int count = -1;
    if (queryString.isEmpty())
        return count;

    qInfo() << "Start query count";
    QSqlQuery querycount(db());
    querycount.prepare("SELECT COUNT(*) FROM (" + queryString + ")");
    if (querycount.exec()) {
        if (querycount.first()) {
            count = querycount.value(0).toInt();
        }
    } else {
        // Log the error if exec() fails
        qCritical() << "Query execution failed:" << querycount.lastError().text();
    }
    qInfo() << "End query count";

    return count;
}

}
