#include "infrastructure/database/cif_database_product.h"
#include "infrastructure/database/cif_database_build.h"
using namespace qualpf::infrastructure;
#include "cif_database_build.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>
#include <iostream>
#include <stdexcept>

using namespace qualpf::application;
namespace {
int checks = 0;
void check(bool ok, const char *message) {
  ++checks;
  if (!ok)
    throw std::runtime_error(message);
}
const QByteArray valid =
    "data_build\n_cell_length_a 5\n_cell_length_b 5\n_cell_length_c 5\n"
    "_cell_angle_alpha 90\n_cell_angle_beta 90\n_cell_angle_gamma 90\n"
    "_symmetry_space_group_name_H-M 'P "
    "1'\nloop_\n_atom_site_label\n_atom_site_type_symbol\n"
    "_atom_site_fract_x\n_atom_site_fract_y\n_atom_site_fract_z\nCu1 Cu .1 .2 "
    ".3\n";
void write(const QString &path, const QByteArray &bytes) {
  QFile file(path);
  check(file.open(QIODevice::WriteOnly), "fixture open");
  check(file.write(bytes) == bytes.size(), "fixture write");
}
qint64 query(const QString &path, const QString &sql, bool read_only = true) {
  const auto name = QUuid::createUuid().toString();
  qint64 result = -1;
  {
    auto db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(path);
    if (read_only)
      db.setConnectOptions("QSQLITE_OPEN_READONLY");
    check(db.open(), "observer open");
    QSqlQuery q(db);
    check(q.exec(sql), "observer query");
    if (q.next())
      result = q.value(0).toLongLong();
    db.close();
  }
  QSqlDatabase::removeDatabase(name);
  return result;
}
void committed(const QString &base, std::size_t count) {
  check(query(base + ".sq", "SELECT count(*) FROM id") ==
            static_cast<qint64>(count),
        "main committed count");
  check(query(base + ".sq.info", "SELECT count(*) FROM info") ==
            static_cast<qint64>(count),
        "info committed count");
  check(query(base + ".sq.search", "SELECT count(*) FROM top") ==
            static_cast<qint64>(count),
        "top committed count");
  check(query(base + ".sq", "SELECT ncard FROM infodb") ==
            static_cast<qint64>(count),
        "published metadata count");
}
void absent(const QString &base) {
  for (const auto *suffix : {".sq", ".sq.info", ".sq.search", ".sq.infostat"})
    check(!QFile::exists(base + suffix), "no database artifact");
}
} // namespace
int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  try {
    QTemporaryDir root;
    check(root.isValid(), "temporary root");
    const auto multi = root.path() + "/multi";
    check(QDir().mkpath(multi), "multi block input directory");
    write(multi + "/500.cif",
          valid + QByteArray(valid)
                      .replace("data_build", "data_alternative")
                      .replace("Cu1 Cu", "O1 O"));
    auto ambiguous = build_cif_directory(root.path() + "/ambiguous", multi);
    check(ambiguous.committed == 0 && ambiguous.failed == 1 &&
              ambiguous.issues[0].code == "multiple_structure_blocks",
          "batch requires unique importable block by default");
    CifBuildOptions selection;
    selection.selection = {
        qualpf::io::cif::BlockSelectionPolicy::specified_block, "alternative"};
    auto explicitResult =
        build_cif_directory(root.path() + "/selected", multi, selection);
    check(explicitResult.succeeded() && explicitResult.committed == 1,
          "batch explicit block policy");
    check(query(root.path() + "/selected.sq.info",
                "SELECT count(*) FROM cif_provenance WHERE block='alternative' "
                "AND length(source_sha256)=64 AND length(ledger)>0") == 1,
          "batch card keeps selected block provenance");
    const auto inputs = root.path() + "/inputs";
    check(QDir().mkpath(inputs + "/nested"), "input directories");
    write(inputs + "/101.cif", valid);
    write(inputs + "/102.cif", "data_bad\n_cell_length_a 'unterminated");
    write(inputs + "/103.cif", QByteArray(valid).replace("Cu1 Cu", "C1 C"));
    write(inputs + "/namedA.cif", valid);
    write(inputs + "/namedB.cif", valid);
    write(inputs + "/nested/101.cif", valid);
    write(inputs + "/nested/104.cif", valid);
    write(inputs + "/ignored.txt", valid);
    const auto all = root.path() + "/all";
    std::size_t notifications = 0;
    auto report = build_cif_directory(
        all, inputs, {true, false}, {},
        [&](const QString &, bool starting, const CifBuildReport &current) {
          ++notifications;
          committed(all, current.committed);
          if (!starting)
            check(current.committed + current.skipped + current.failed ==
                      current.discovered,
                  "each finished input has exactly one outcome");
        });
    check(report.completed && !report.succeeded() && !report.cancelled &&
              !report.fatal,
          "scan completion does not hide per-card failures");
    check(report.discovered == 7 && report.committed == 4 &&
              report.failed == 3 && report.skipped == 0,
          "recursive counts, duplicate identities and nonnumeric zero "
          "collisions");
    check(notifications == 14 && report.issues.size() == 3,
          "progress and issues for every file");
    int duplicates = 0, syntax = 0;
    for (const auto &error : report.issues) {
      check(!error.source.empty() && !error.field.empty() &&
                !error.reason.empty(),
            "locatable build issue");
      duplicates += error.code == "duplicate_identity";
      syntax += error.stage == "syntax" && error.line == 2;
    }
    check(duplicates == 2 && syntax == 1,
          "distinct duplicate and syntax diagnostics");
    committed(all, 4);
    check(query(all + ".sq", "SELECT count(*) FROM id WHERE id=0") == 1,
          "one nonnumeric identity survives");
    auto recreate = build_cif_directory(all, inputs);
    check(recreate.fatal && recreate.fatal->code == "database_exists" &&
              recreate.discovered == 0,
          "existing database refused before processing files");
    committed(all, 4);

    const auto inorganic = root.path() + "/inorganic";
    auto filtered = build_cif_directory(inorganic, inputs, {true, true});
    check(filtered.completed && filtered.committed == 3 &&
              filtered.skipped == 1 && filtered.failed == 3,
          "only valid non-inorganic cards skipped; malformed CIF remains an "
          "error");
    committed(inorganic, 3);
    auto shallow = build_cif_directory(root.path() + "/shallow", inputs);
    check(shallow.discovered == 5 && shallow.committed == 3 &&
              shallow.failed == 2,
          "nonrecursive scan");

    std::stop_source before;
    before.request_stop();
    auto stopped = build_cif_directory(root.path() + "/before", inputs, {},
                                       before.get_token());
    check(stopped.cancelled && !stopped.database_ready && !stopped.completed,
          "pre-cancelled request");
    absent(root.path() + "/before");
    std::stop_source during;
    const auto partial = root.path() + "/partial";
    auto prefix = build_cif_directory(
        partial, inputs, {true, false}, during.get_token(),
        [&](const QString &, bool starting, const CifBuildReport &current) {
          if (!starting && current.committed == 1)
            during.request_stop();
        });
    check(prefix.cancelled && !prefix.completed && prefix.committed == 1,
          "cancel preserves committed prefix");
    committed(partial, 1);
    std::stop_source in_flight;
    const auto empty = root.path() + "/empty";
    auto early = build_cif_directory(
        empty, inputs, {}, in_flight.get_token(),
        [&](const QString &, bool starting, const CifBuildReport &) {
          if (starting)
            in_flight.request_stop();
        });
    check(early.cancelled && early.discovered == 1 && early.committed == 0 &&
              early.failed == 0,
          "in-flight cancellation is not a failed or committed card");
    committed(empty, 0);

    auto missing =
        build_cif_directory(root.path() + "/missing", root.path() + "/absent");
    check(missing.fatal && missing.fatal->code == "invalid_cif_directory",
          "missing input directory");
    absent(root.path() + "/missing");
    const auto vanished_dir = root.path() + "/vanished-input";
    check(QDir().mkpath(vanished_dir), "vanished input directory");
    write(vanished_dir + "/1.cif", valid);
    auto vanished = build_cif_directory(
        root.path() + "/vanished", vanished_dir, {}, {},
        [](const QString &path, bool starting, const CifBuildReport &) {
          if (starting)
            QFile::remove(path);
        });
    check(vanished.failed == 1 && vanished.issues[0].code == "cif_read_failure",
          "read race reports file error");
    committed(root.path() + "/vanished", 0);
    auto callback = build_cif_directory(
        root.path() + "/callback", inputs, {}, {},
        [](const QString &, bool, const CifBuildReport &) {
          throw std::runtime_error("injected callback failure");
        });
    check(callback.fatal &&
              callback.fatal->code == "progress_callback_failure" &&
              !callback.completed,
          "callback failure is structured and closes writer");
    committed(root.path() + "/callback", 0);
    std::cout << checks << " directory build checks passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
