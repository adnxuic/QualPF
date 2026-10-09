#include "cif_database_writer.h"
#include "sql_schema_builder.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>

namespace qualpf::database {
namespace {
QString text(const std::string &value) {
  return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}
QVariant nullable(const std::string &value) {
  return value.empty() ? QVariant{} : QVariant{text(value)};
}
WriteError error(std::string code, std::string field, std::string reason) {
  return {std::move(code), {}, {}, std::move(field), std::move(reason)};
}
bool positive(double value) { return std::isfinite(value) && value > 0; }

std::optional<WriteError> validate(const CifRecord &record) {
  if (record.lines.empty() || record.lines.size() > 500)
    return error("invalid_card", "lines",
                 "A database card requires between one and 500 retained lines");
  if (record.atom_count <= 0 ||
      record.reflection_count <
          static_cast<std::int64_t>(record.lines.size()) ||
      record.formula_units <= 0)
    return error("invalid_card", "counts", "Card counts are inconsistent");
  if (!positive(record.volume) || !positive(record.density) ||
      !positive(record.rir) || !std::isfinite(record.linear_absorption) ||
      record.linear_absorption < 0)
    return error("invalid_card", "physical",
                 "Card scalars must be finite and physical");
  if (!std::all_of(record.cell.begin(), record.cell.end(), positive))
    return error("invalid_card", "cell",
                 "Card cell parameters must be finite and positive");
  double preceding = std::numeric_limits<double>::infinity();
  for (const auto &line : record.lines) {
    if (!positive(line.d) || !positive(line.relative_intensity) ||
        line.multiplicity <= 0 || line.relative_intensity > preceding)
      return error("invalid_card", "lines",
                   "Lines must have finite positive values in existing "
                   "descending intensity order");
    preceding = line.relative_intensity;
  }
  if (record.formula.empty() || record.space_group.empty() ||
      record.crystal_system.empty() || record.elements.empty())
    return error("invalid_card", "metadata",
                 "Calculated formula, group, system and elements are required");
  return std::nullopt;
}
} // namespace

CifDatabaseWriter::CifDatabaseWriter()
    : connection_(QStringLiteral("qualpf_cif_") +
                  QUuid::createUuid().toString(QUuid::Id128)) {}
CifDatabaseWriter::~CifDatabaseWriter() { close(); }

void CifDatabaseWriter::close() {
  Q_ASSERT_X(owner_ == std::this_thread::get_id(), "CifDatabaseWriter", "SQL writer must close on its execution thread");
  if (owner_ != std::this_thread::get_id()) return;
  if (database_.isValid()) {
    database_.close();
    database_ = QSqlDatabase{};
    QSqlDatabase::removeDatabase(connection_);
  }
  path_.clear();
  writeLease_.reset();
}

void CifDatabaseWriter::execute(const QString &sql,
                                const QVariantList &values) {
  QSqlQuery query(database_);
  if (!query.prepare(sql))
    throw error("db_prepare_failure", "database",
                query.lastError().text().toStdString());
  for (const auto &value : values)
    query.addBindValue(value);
  if (!query.exec())
    throw error("db_write_failure", "database",
                query.lastError().text().toStdString());
}

std::optional<WriteError> CifDatabaseWriter::create(const QString &basePath) {
  if (owner_ != std::this_thread::get_id()) return error("invalid_thread", "database", "Writer requires its execution thread");
  close();
  if (basePath.isEmpty())
    return error("invalid_database_path", "database",
                 "Database base path is empty");
  const QStringList suffixes{QStringLiteral(".sq"), QStringLiteral(".sq.info"),
                             QStringLiteral(".sq.search"),
                             QStringLiteral(".sq.infostat")};
  for (const auto &suffix : suffixes)
    if (QFileInfo::exists(basePath + suffix))
      return error("database_exists", "database",
                   "Creation refuses to overwrite an existing database file");
  auto lease = infrastructure::DatabaseLease::write(basePath);
  if (!lease) return error("database_busy", "database", "Reference database is being read");
  {
    // The old creator overwrites paths. Run it only in a fresh owned staging
    // directory; QFile::rename then publishes without replacing existing files.
    QTemporaryDir staging(QFileInfo(basePath).absolutePath() +
                          QStringLiteral("/.qualpf-cif-XXXXXX"));
    if (!staging.isValid())
      return error("schema_creation_failure", "database",
                   "Could not create schema staging directory");
    const auto temporaryBase = staging.path() + QStringLiteral("/database");
    qualpf::infrastructure::SqlSchemaBuilder creator;
    if (!creator.create(temporaryBase, qualpf::infrastructure::SqlSchemaBuilder::DbType::CifFiles))
      return error("schema_creation_failure", "database",
                   "Could not create all four original database schemas");
    creator.close();
    QStringList published;
    for (const auto &suffix : suffixes) {
      if (!QFile::rename(temporaryBase + suffix, basePath + suffix)) {
        bool recovered = true;
        for (const auto &old : published)
          recovered =
              QFile::rename(basePath + old, temporaryBase + old) && recovered;
        return error("schema_publication_failure", "database",
                     recovered ? "Could not publish schema files; existing "
                                 "destination files were preserved"
                               : "Could not publish or recover every newly "
                                 "created empty schema file");
      }
      published.append(suffix);
    }
  }
  path_ = basePath;
  database_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection_);
  database_.setDatabaseName(basePath + QStringLiteral(".sq"));
  if (!database_.open()) {
    const auto result = error("db_open_failure", "database",
                              database_.lastError().text().toStdString());
    close();
    return result;
  }
  try {
    execute(QStringLiteral("ATTACH DATABASE ? AS details"),
            {basePath + QStringLiteral(".sq.info")});
    execute(QStringLiteral("ATTACH DATABASE ? AS lookup"),
            {basePath + QStringLiteral(".sq.search")});
    execute(QStringLiteral("ATTACH DATABASE ? AS statistics"),
            {basePath + QStringLiteral(".sq.infostat")});
    // Atomic attached-file commits require a disk main DB and rollback
    // journals. Refuse any provider that cannot enforce these settings.
    for (const auto *schema : {"main", "details", "lookup", "statistics"}) {
      QSqlQuery query(database_);
      if (!query.exec(QStringLiteral("PRAGMA %1.journal_mode=DELETE")
                          .arg(QString::fromLatin1(schema))) ||
          !query.next() ||
          query.value(0).toString().compare(QStringLiteral("delete"),
                                            Qt::CaseInsensitive) != 0)
        throw error("transaction_configuration_failure",
                    "database.journal_mode",
                    "Rollback-journal mode is required");
      execute(QStringLiteral("PRAGMA %1.synchronous=FULL")
                  .arg(QString::fromLatin1(schema)));
    }
    execute(QStringLiteral("INSERT INTO infodb (id,date,ncard,type,source) "
                           "VALUES (?,?,?,?,?)"),
            {QStringLiteral("0"), QDate::currentDate().toString(Qt::ISODate), 0,
             QStringLiteral("COD"), QStringLiteral("you")});
    execute(QStringLiteral(
        "CREATE TABLE details.cif_provenance (id INTEGER PRIMARY KEY, "
        "source TEXT NOT NULL, source_sha256 TEXT NOT NULL, block TEXT NOT "
        "NULL, "
        "structure_identity TEXT NOT NULL, ledger TEXT NOT NULL)"));
  } catch (const WriteError &failure) {
    close();
    return failure;
  }
  writeLease_ = std::move(lease);
  return std::nullopt;
}

void CifDatabaseWriter::statistic(const QString &table, const QVariant &value,
                                  const QString &identity) {
  QSqlQuery query(database_);
  if (!query.prepare(
          QStringLiteral("SELECT ids,n FROM statistics.%1 WHERE val=?")
              .arg(table)))
    throw error("db_prepare_failure", "statistics",
                query.lastError().text().toStdString());
  query.addBindValue(value);
  if (!query.exec())
    throw error("db_read_failure", "statistics",
                query.lastError().text().toStdString());
  if (query.next()) {
    const auto ids = query.value(0).toString() + QLatin1Char(',') + identity;
    const auto n = query.value(1).toLongLong() + 1;
    query.finish();
    execute(QStringLiteral("UPDATE statistics.%1 SET ids=?,n=? WHERE val=?")
                .arg(table),
            {ids, n, value});
  } else {
    query.finish();
    execute(
        QStringLiteral("INSERT INTO statistics.%1 (val,ids,n) VALUES (?,?,?)")
            .arg(table),
        {value, identity, 1});
  }
}

void CifDatabaseWriter::spaceGroupStatistic(const CifRecord &record) {
  QSqlQuery query(database_);
  query.prepare(QStringLiteral(
      "SELECT ids,n FROM details.spgrstat WHERE spacegroup=? AND type=?"));
  query.addBindValue(text(record.space_group));
  query.addBindValue(text(record.crystal_system));
  if (!query.exec())
    throw error("db_read_failure", "spgrstat",
                query.lastError().text().toStdString());
  if (query.next()) {
    const auto ids = query.value(0).toString() + QLatin1Char(',') +
                     QString::number(record.identity);
    const auto n = query.value(1).toLongLong() + 1;
    query.finish();
    execute(QStringLiteral("UPDATE details.spgrstat SET ids=?,n=? WHERE "
                           "spacegroup=? AND type=?"),
            {ids, n, text(record.space_group), text(record.crystal_system)});
  } else {
    query.finish();
    execute(QStringLiteral("INSERT INTO details.spgrstat "
                           "(spacegroup,type,ids,n) VALUES (?,?,?,?)"),
            {text(record.space_group), text(record.crystal_system),
             QString::number(record.identity), 1});
  }
}

std::optional<WriteError> CifDatabaseWriter::write(const CifRecord &record) {
  if (owner_ != std::this_thread::get_id()) return error("invalid_thread", "database", "Writer requires its execution thread");
  const auto annotate = [&](WriteError error) {
    error.source = record.source;
    error.block = record.block;
    return std::optional<WriteError>(std::move(error));
  };
  if (!database_.isOpen())
    return annotate(error("database_closed", "database", "Writer is not open"));
  if (auto invalid = validate(record))
    return annotate(*invalid);
  bool transaction = false;
  try {
    execute(QStringLiteral("BEGIN IMMEDIATE"));
    transaction = true;
    QSqlQuery check(database_);
    check.prepare(QStringLiteral("SELECT count(*) FROM id WHERE id=?"));
    check.addBindValue(record.identity);
    if (!check.exec() || !check.next())
      throw error("db_read_failure", "identity",
                  check.lastError().text().toStdString());
    if (check.value(0).toLongLong() != 0)
      throw error("duplicate_identity", "identity",
                  "A complete card with this identity already exists");
    check.finish();
    if (!record.provenance.empty())
      execute(QStringLiteral("INSERT INTO details.cif_provenance "
                             "(id,source,source_sha256,block,structure_"
                             "identity,ledger) VALUES (?,?,?,?,?,?)"),
              {record.identity, text(record.source), text(record.source_sha256),
               text(record.block), text(record.structure_identity),
               text(record.provenance)});
    if (!check.exec(QStringLiteral("SELECT coalesce(max(n),0)+1 FROM id")) ||
        !check.next())
      throw error("db_read_failure", "ordinal",
                  check.lastError().text().toStdString());
    const auto ordinal = check.value(0).toLongLong();
    check.finish();
    std::vector<std::size_t> order(record.lines.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) {
      return record.lines[a].d > record.lines[b].d;
    });
    QString d, intensity, h, k, l, mult;
    for (auto i : order) {
      const auto &line = record.lines[i];
      d += QString::asprintf("%f,", line.d);
      intensity += QString::asprintf("%f,", line.relative_intensity);
      h += QString::number(line.hkl[0]) + QLatin1Char(',');
      k += QString::number(line.hkl[1]) + QLatin1Char(',');
      l += QString::number(line.hkl[2]) + QLatin1Char(',');
      mult += QString::number(line.multiplicity) + QLatin1Char(',');
    }
    execute(
        QStringLiteral("INSERT INTO id "
                       "(id,name,mineralname,chemical_formula,spacegroup,"
                       "quality,rir,nrec,natoms,"
                       "nreflections,nd,dvalue,intensita,n) VALUES "
                       "(?,?,?,?,?,?,?,?,?,?,?,?,?,?)"),
        {record.identity, nullable(record.name), nullable(record.mineral),
         text(record.formula), text(record.space_group), QStringLiteral("C"),
         record.rir, 0, record.atom_count, record.reflection_count,
         static_cast<qlonglong>(record.lines.size()), d, intensity, ordinal});
    for (const auto &element : record.elements)
      execute(QStringLiteral("INSERT OR IGNORE INTO chemical "
                             "(id,chemical_element) VALUES (?,?)"),
              {record.identity, text(element)});
    if (!record.subfile.empty())
      execute(QStringLiteral("INSERT INTO subfiles (id,subfile) VALUES (?,?)"),
              {record.identity, text(record.subfile)});
    QVariantList info{QString::number(record.identity)};
    for (int i = 0; i < 8; ++i)
      info.append(QStringLiteral(""));
    info << 0.0 << QString::number(record.formula_units)
         << text(record.space_group) << text(record.crystal_system)
         << record.volume << record.density
         << QString::number(record.linear_absorption, 'f', 4)
         << record.atom_count << record.reflection_count;
    for (double v : record.cell)
      info.append(v);
    info << QString::number(record.rir, 'f', 4) << h << k << l << mult;
    execute(
        QStringLiteral(
            "INSERT INTO details.info "
            "(id,authors,journal,journal_year,journal_volume,journal_issue,"
            "page_start,"
            "page_end,color,crystal_density,z,spacegroup,type,volume,density,"
            "\"mu(CuKa)\","
            "natoms,nreflections,a,b,c,alpha,beta,gamma,rir,h,k,l,mul) VALUES "
            "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"),
        info);
    const auto topCount = std::min<std::size_t>(record.lines.size(), 3);
    QStringList top;
    for (std::size_t i = 0; i < topCount; ++i)
      top.append(QString::asprintf("%f", record.lines[i].d));
    execute(QStringLiteral("INSERT INTO lookup.top (id,n,dval) VALUES (?,?,?)"),
            {record.identity, static_cast<int>(topCount),
             top.join(QLatin1Char(','))});
    const auto identity = QString::number(record.identity);
    static constexpr std::array<const char *, 6> tables{
        "stat_a", "stat_b", "stat_c", "stat_alpha", "stat_beta", "stat_gamma"};
    for (std::size_t i = 0; i < 6; ++i)
      if (i < 3 || record.cell[i] != 90)
        statistic(QString::fromLatin1(tables[i]), record.cell[i], identity);
    statistic(QStringLiteral("stat_vol"), std::round(record.volume * 100) / 100,
              identity);
    statistic(QStringLiteral("stat_dens"), record.density, identity);
    statistic(QStringLiteral("stat_spgr"),
              text(record.space_group).left(7).remove(QLatin1Char(' ')),
              identity);
    statistic(QStringLiteral("stat_type"), text(record.crystal_system),
              identity);
    spaceGroupStatistic(record);
    execute(
        QStringLiteral("UPDATE infodb SET ncard=(SELECT count(*) FROM id)"));
    execute(QStringLiteral("COMMIT"));
    transaction = false;
  } catch (const WriteError &failure) {
    if (transaction) {
      QSqlQuery rollback(database_);
      if (!rollback.exec(QStringLiteral("ROLLBACK"))) {
        auto fatal = error("rollback_failure", "database",
                           failure.reason + "; " +
                               rollback.lastError().text().toStdString());
        close();
        return annotate(std::move(fatal));
      }
    }
    return annotate(failure);
  }
  return std::nullopt;
}

} // namespace qualpf::database
