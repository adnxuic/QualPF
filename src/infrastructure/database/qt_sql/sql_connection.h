#ifndef QUALPF_SQL_CONNECTION_H
#define QUALPF_SQL_CONNECTION_H

#include "connection_trace.h"
#include <QString>
#include <cstdint>



class QSqlDatabase;

namespace qualpf::infrastructure {
class SqlConnection {
  public:
    SqlConnection();
    SqlConnection(const QString &path);
    SqlConnection(QString scope, QString role, qualpf::infrastructure::ConnectionTrace trace);
    ~SqlConnection();
    SqlConnection(const SqlConnection &) = delete;
    SqlConnection &operator=(const SqlConnection &) = delete;

    bool openDb(const QString &path);
    void closeDb();
    bool isOpen() const;
    int queryForCount(const QString &queryString);
    QSqlDatabase db() const;
    bool onOwnerThread() const { return owner_ == std::this_thread::get_id(); }

  private:
    // Store only the connection name, not a QSqlDatabase value member.
    // A QSqlDatabase value member in a class with static storage duration
    // would be constructed before QApplication exists, causing Qt warnings.
    QString m_connName;
    QString scope_, role_;
    std::uint64_t generation_ = 0;
    std::thread::id owner_ = std::this_thread::get_id();
    qualpf::infrastructure::ConnectionTrace trace_;
    void record(qualpf::infrastructure::ConnectionAction) const noexcept;
    QString queryElementString(const QString &elString);
    void debugDatabaseInfo();
};

} // namespace qualpf::infrastructure

#endif // QUALPF_SQL_CONNECTION_H
