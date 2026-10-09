#include "infrastructure/database/qt_sql/cif_database_writer.h"
#include <QCoreApplication>
#include <QDataStream>
#include <QFile>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>

using namespace qualpf::database;
namespace {
int checks = 0;
void check(bool ok, const char *label) {
  ++checks;
  if (!ok)
    throw std::runtime_error(label);
}
CifRecord card(int identity = 1900001) {
  CifRecord r;
  r.identity = identity;
  r.source = "controlled.cif";
  r.block = "test";
  r.source_sha256 = std::string(64, 'a');
  r.structure_identity = "sha256:" + r.source_sha256 + "/block:4:test";
  r.provenance = "exact ledger including ? unknown ADP and quoted text";
  r.name = "O'Brien native writer fixture";
  r.mineral = "Writer oracle";
  r.formula = "O2 Si";
  r.space_group = "P 1";
  r.crystal_system = "Triclinic";
  r.subfile = "inorganic";
  r.elements = {"Si", "O"};
  r.atom_count = 2;
  r.reflection_count = 3;
  r.formula_units = 1;
  r.cell = {5, 5, 5, 90, 90, 90};
  r.volume = 125;
  r.density = 2.5;
  r.linear_absorption = 12.25;
  r.rir = 4.5;
  r.lines = {{{1, 0, 0}, 2, 3.25, 1000},
             {{2, -1, 2}, 4, 2.5, 500},
             {{3, -2, 4}, 6, 1, 100}};
  return r;
}
class Observer {
public:
  explicit Observer(const QString &path) {
    db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                   QStringLiteral("test_observer"));
    db.setDatabaseName(path);
    check(db.open(), "observer open");
  }
  ~Observer() {
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(QStringLiteral("test_observer"));
  }
  QVariant scalar(const QString &sql) {
    QSqlQuery q(db);
    check(q.exec(sql) && q.next(), "observer query");
    return q.value(0);
  }
  void execute(const QString &sql) {
    QSqlQuery q(db);
    if (!q.exec(sql))
      throw std::runtime_error(q.lastError().text().toStdString());
  }
  QSqlDatabase db;
};
void counts(const QString &base, int expected) {
  const std::array<std::pair<const char *, const char *>, 4> files{
      {{".sq", "id"},
       {".sq.info", "info"},
       {".sq.search", "top"},
       {".sq.infostat", "stat_a"}}};
  for (const auto &[suffix, table] : files) {
    Observer read(base + QString::fromLatin1(suffix));
    const auto sql =
        QString::fromLatin1(table) == "stat_a"
            ? QStringLiteral("SELECT coalesce(sum(n),0) FROM stat_a")
            : QStringLiteral("SELECT count(*) FROM %1")
                  .arg(QString::fromLatin1(table));
    check(read.scalar(sql).toInt() == expected,
          "all stores contain only complete cards");
  }
  Observer main(base + QStringLiteral(".sq"));
  check(main.scalar(QStringLiteral("SELECT ncard FROM infodb")).toInt() ==
            expected,
        "metadata count matches committed cards");
}
QByteArray snapshot(const QString &base) {
  QByteArray bytes;
  QDataStream output(&bytes, QIODevice::WriteOnly);
  for (const auto *suffix : {".sq", ".sq.info", ".sq.search", ".sq.infostat"}) {
    Observer read(base + QString::fromLatin1(suffix));
    QSqlQuery tables(read.db);
    check(
        tables.exec(QStringLiteral(
            "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name")),
        "snapshot tables");
    while (tables.next()) {
      const auto name = tables.value(0).toString();
      output << name;
      QSqlQuery rows(read.db);
      auto quoted = name;
      quoted.replace(QLatin1Char('"'), QStringLiteral("\"\""));
      check(rows.exec(QStringLiteral("SELECT * FROM \"%1\" ORDER BY rowid")
                          .arg(quoted)),
            "snapshot rows");
      while (rows.next()) {
        QVariantList values;
        for (int i = 0; i < rows.record().count(); ++i)
          values.append(rows.value(i));
        output << values;
      }
    }
  }
  return bytes;
}
} // namespace
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    QTemporaryDir directory;
    check(directory.isValid(), "test output directory");
    const auto base = directory.path() + QStringLiteral("/cards");
    CifDatabaseWriter writer;
    check(!writer.create(base), "create schemas");
    counts(base, 0);
    check(!writer.write(card()), "valid card");
    counts(base, 1);
    {
      Observer read(base + QStringLiteral(".sq.info"));
      check(
          read.scalar(QStringLiteral(
                          "SELECT ledger FROM cif_provenance WHERE id=1900001"))
                  .toString()
                  .toStdString() == card().provenance,
          "provenance ledger survives SQLite roundtrip");
      check(
          read.scalar(QStringLiteral(
                          "SELECT block FROM cif_provenance WHERE id=1900001"))
                  .toString() == "test",
          "selected block persists independently of card display name");
      check(read.scalar(QStringLiteral("SELECT source_sha256 FROM "
                                       "cif_provenance WHERE id=1900001"))
                    .toString()
                    .toStdString() == card().source_sha256,
            "source hash persists");
    }
    {
      Observer read(base + QStringLiteral(".sq.search"));
      check(read.scalar(QStringLiteral("SELECT dval FROM top")).toString() ==
                "3.250000,2.500000,1.000000",
            "exact top serialization");
    }
    const auto duplicate = writer.write(card());
    check(duplicate && duplicate->code == "duplicate_identity" &&
              duplicate->source == "controlled.cif" &&
              duplicate->block == "test",
          "duplicate error context");
    counts(base, 1);
    auto empty = card(1900002);
    empty.lines.clear();
    check(writer.write(empty)->code == "invalid_card",
          "zero-line card rejected before transaction");
    counts(base, 1);
    // Inject a real SQLite statement failure into every store. Every earlier
    // statement and every statistic must roll back; the next card must work.
    int identity = 1900002;
    for (const auto &[suffix, table] :
         std::array<std::pair<const char *, const char *>, 4>{
             {{".sq", "chemical"},
              {".sq.info", "info"},
              {".sq.search", "top"},
              {".sq.infostat", "stat_a"}}}) {
      const auto before = snapshot(base);
      {
        Observer change(base + QString::fromLatin1(suffix));
        const auto event = QString::fromLatin1(table) == "stat_a"
                               ? QStringLiteral("UPDATE")
                               : QStringLiteral("INSERT");
        change.execute(
            QStringLiteral(
                "CREATE TRIGGER injected_failure BEFORE %1 ON %2 BEGIN SELECT "
                "RAISE(ABORT,'injected failure'); END")
                .arg(event, QString::fromLatin1(table)));
      }
      const auto failed = writer.write(card(identity));
      check(failed && failed->code == "db_write_failure",
            "injected statement failure reported");
      check(snapshot(base) == before,
            "failed card preserves every typed row in all four files");
      counts(base, identity - 1900001);
      {
        Observer change(base + QString::fromLatin1(suffix));
        change.execute(QStringLiteral("DROP TRIGGER injected_failure"));
      }
      check(!writer.write(card(identity)), "next card after rollback succeeds");
      counts(base, identity - 1900000);
      ++identity;
    }
    writer.close();
    counts(base, 5);
    const auto existing = writer.create(base);
    check(existing && existing->code == "database_exists",
          "existing database preserved");
    counts(base, 5);
    check(writer.write(card(2000000))->code == "database_closed",
          "closed writer does not mutate");
    std::cout << "PASS " << checks << " checks\n";
  } catch (const std::exception &error) {
    std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
    return 1;
  }
}
