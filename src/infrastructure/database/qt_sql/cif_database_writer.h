#pragma once

#include "qualpf/application/database/cif_record_writer.h"
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <optional>
#include <thread>
#include "infrastructure/database/database_access.h"

namespace qualpf::database {

// Thread-confined writer. A single connection ATTACHes all four on-disk stores;
// rollback-journal transactions commit one complete card, including all indexes.
// Creation refuses existing files. Replacing a database is a caller lifecycle
// operation, not an implicit side effect of this writer.
class CifDatabaseWriter : public application::CifRecordWriter {
  public:
    CifDatabaseWriter();
    ~CifDatabaseWriter();
    CifDatabaseWriter(const CifDatabaseWriter &) = delete;
    CifDatabaseWriter &operator=(const CifDatabaseWriter &) = delete;

    [[nodiscard]] std::optional<WriteError> create(const QString &basePath);
    std::optional<WriteError> create(const std::string &basePath) override { return create(QString::fromUtf8(basePath.data(), static_cast<qsizetype>(basePath.size()))); }
    [[nodiscard]] std::optional<WriteError> write(const CifRecord &record) override;
    void close() override;

  private:
    void execute(const QString &sql, const QVariantList &values = {});
    void statistic(const QString &table, const QVariant &value, const QString &identity);
    void spaceGroupStatistic(const CifRecord &record);
    QString connection_;
    QSqlDatabase database_;
    QString path_;
    std::thread::id owner_ = std::this_thread::get_id();
    std::unique_ptr<infrastructure::DatabaseLease> writeLease_;
};

} // namespace qualpf::database
