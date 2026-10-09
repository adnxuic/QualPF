#include "infrastructure/database/database_build_provider.h"
#include "infrastructure/database/cif_database_product.h"
#include "infrastructure/database/cif_database_build.h"
using namespace qualpf::infrastructure;
// Reuse only the frozen observation serializer, never its isolated entry point.
#define main archived_record_main
#include "cif_record_replay.cpp"
#undef main
#include "gui/database/databasebuilder.h"
#include "tests/support/retired_cif_call_counter.h"
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPushButton>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <sstream>
using qualpf::infrastructure::CifDatabaseImportService;
using qualpf::infrastructure::cif_database_backend_from_environment;

using namespace qualpf::application;
namespace cif = qualpf::io::cif;
namespace {
DatabaseBuildUseCases &buildUseCases() {
  static qualpf::infrastructure::QtDatabaseBuildProvider provider;
  static DatabaseBuildUseCases useCases(provider);
  return useCases;
}
int checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value)
    throw std::runtime_error(message);
}
QByteArray bytes(const QString &path) {
  QFile f(path);
  check(f.open(QIODevice::ReadOnly), "open fixture");
  return f.readAll();
}
void save(const QString &path, const QByteArray &value) {
  QFile f(path);
  check(f.open(QIODevice::WriteOnly | QIODevice::NewOnly), "exclusive output");
  check(f.write(value) == value.size(), "complete output");
}
QJsonObject counters(const CifImportCounters &c) {
  return {
      {"parser_calls", qint64(c.parser_calls)},
      {"importer_calls", qint64(c.importer_calls)},
      {"card_pipeline_calls", qint64(c.card_pipeline_calls)},
      {"completed_phase13_18_chains", qint64(c.completed_phase13_18_chains)},
      {"writer_calls", qint64(c.writer_calls)},
      {"committed_cards", qint64(c.committed_cards)}};
}
QJsonObject failure(const CifImportFailure &e) {
  QJsonArray candidates;
  for (const auto &c : e.candidates) {
    QJsonArray cell;
    for (const auto &v : c.cell_parameters)
      cell.append(v ? QString::fromStdString(v->raw) : QString{});
    QJsonObject adp;
    for (const auto &[key, n] : c.adp_summary)
      adp[QString::fromStdString(key)] = qint64(n);
    QJsonArray notices;
    if (c.provenance)
      for (const auto &n : c.provenance->notices)
        notices.append(QString::fromStdString(n));
    candidates.append(QJsonObject{
        {"block", QString::fromStdString(c.block_id)},
        {"cell", cell},
        {"space_group", QString::fromStdString(c.normalized_identifier)},
        {"site_count", qint64(c.atom_site_count)},
        {"completeness",
         QString::fromUtf8(cif::classification_name(c.classification).data())},
        {"importable", c.importable},
        {"adp", adp},
        {"warnings", notices}});
  }
  return {{"stage", QString::fromStdString(e.stage)},
          {"code", QString::fromStdString(e.code)},
          {"source", QString::fromStdString(e.source)},
          {"block", QString::fromStdString(e.block)},
          {"field", QString::fromStdString(e.field)},
          {"message", QString::fromStdString(e.message)},
          {"core_reason", QString::fromStdString(e.core_reason)},
          {"candidates", candidates},
          {"core_field", QString::fromStdString(e.core_field)},
          {"line", qint64(e.location.line)},
          {"column", qint64(e.location.column)}};
}
QJsonObject report(const CifBatchReport &r) {
  QJsonArray errors, sources;
  for (const auto &e : r.failures)
    errors.append(failure(e));
  for (const auto &s : r.sources)
    sources.append(QJsonObject{
        {"source", QString::fromStdString(s.path)},
        {"identity", s.identity},
        {"block", QString::fromStdString(s.selected_block)},
        {"source_sha256", QString::fromStdString(s.source_sha256)},
        {"structure_identity", QString::fromStdString(s.structure_identity)}});
  QJsonObject result{
      {"backend", r.backend == CifDatabaseBackend::cpp ? "cpp" : "legacy"},
      {"discovered", qint64(r.discovered)},
      {"committed", qint64(r.committed)},
      {"skipped", qint64(r.skipped)},
      {"completed", r.completed},
      {"cancelled", r.cancelled},
      {"failures", errors},
      {"sources", sources},
      {"cpp", counters(r.cpp)},
      {"legacy_native_calls", qint64(r.legacy_native_calls)}};
  if (r.fatal)
    result["fatal"] = failure(*r.fatal);
  return result;
}
class Observer {
public:
  QSqlDatabase db;
  QString name = QUuid::createUuid().toString();
  explicit Observer(const QString &path) {
    db = QSqlDatabase::addDatabase("QSQLITE", name);
    db.setDatabaseName(path);
    check(db.open(), "open observer");
  }
  ~Observer() {
    db.close();
    db = {};
    QSqlDatabase::removeDatabase(name);
  }
  QVariant scalar(const QString &sql) {
    QSqlQuery q(db);
    check(q.exec(sql) && q.next(), "observer scalar");
    return q.value(0);
  }
  void exec(const QString &sql) {
    QSqlQuery q(db);
    check(q.exec(sql), "observer mutation");
  }
};
QJsonObject snapshot(const QString &base) {
  QJsonObject all;
  for (const auto &suffix : {".sq", ".sq.info", ".sq.search", ".sq.infostat"}) {
    Observer o(base + suffix);
    QSqlQuery names(o.db);
    check(
        names.exec(
            "SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"),
        "table names");
    QJsonObject tables;
    while (names.next()) {
      const auto name = names.value(0).toString();
      QSqlQuery rows(o.db);
      check(rows.exec("SELECT * FROM \"" + name + "\" ORDER BY rowid"),
            "table snapshot");
      QJsonArray values;
      while (rows.next()) {
        QJsonArray row;
        for (int c = 0; c < rows.record().count(); ++c) {
          const auto v = rows.value(c);
          row.append(QJsonObject{
              {"type",
               QString::fromLatin1(v.typeName() ? v.typeName() : "null")},
              {"null", v.isNull()},
              {"value", QJsonValue::fromVariant(v)}});
        }
        values.append(row);
      }
      tables[name] = values;
    }
    all[suffix] = tables;
  }
  return all;
}
void count(const QString &base, int n) {
  for (const auto &[suffix, table] :
       std::array<std::pair<const char *, const char *>, 4>{
           {{".sq", "id"},
            {".sq.info", "info"},
            {".sq.info", "cif_provenance"},
            {".sq.search", "top"}}}) {
    Observer o(base + suffix);
    check(o.scalar(QString("SELECT count(*) FROM %1").arg(table)).toInt() == n,
          "card/provenance/search counts");
  }
}
void lifecycle(const QString &root) {
  qunsetenv("QUALPF_CIF_DB_BACKEND");
  const auto before = qualpf::legacy::cif_native_calls();
  QTemporaryDir temp;
  check(temp.isValid(), "temporary product directory");
  const auto fixtures =
      root +
      "/tests/data/cif/";
  const auto zinc = fixtures + "cod-2300112.cif";
  const auto formula = fixtures + "cod-4343150.cif";
  CifDatabaseImportService owner;
  check(!owner.create(temp.path() + "/lifecycle"), "production create");
  auto p = owner.prepare_file(zinc, 2300112);
  check(std::holds_alternative<PreparedCifImport>(p), "production A1 prepare");
  const auto &card = std::get<PreparedCifImport>(p);
  const auto &provenance = card.calculated().imported().provenance;
  check(provenance.displacement_choices.size() == 1 &&
            !provenance.strict_attempt_ledger.empty(),
        "retained A1 tensor and strict failure");
  check(provenance.displacement_choices[0].site == "O",
        "exact A1 selected site");
  auto restored = owner.reload(card.ticket(), 2300112);
  check(std::holds_alternative<PreparedCifImport>(restored),
        "production ticket reload");
  const auto &reload = std::get<PreparedCifImport>(restored);
  check(reload.record().provenance == card.record().provenance &&
            reload.record().density == card.record().density &&
            reload.record().rir == card.record().rir,
        "reload exact ledger/physics");
  check(std::holds_alternative<CifImportFailure>(
            owner.reload(card.ticket() + "x", 2300112)),
        "tampered ticket rejection");
  check(!owner.commit(card), "production commit A1");
  const auto base = temp.path() + "/lifecycle";
  count(base, 1);
  auto f = owner.prepare_file(formula, 1900004);
  check(std::holds_alternative<PreparedCifImport>(f), "production F1 prepare");
  const auto &fcard = std::get<PreparedCifImport>(f);
  check(fcard.calculated().imported().provenance.formula_z.has_value(),
        "formula Z ledger retained");
  for (const auto &[suffix, table] :
       std::array<std::pair<const char *, const char *>, 4>{
           {{".sq", "chemical"},
            {".sq.info", "cif_provenance"},
            {".sq.search", "top"},
            {".sq.infostat", "stat_a"}}}) {
    const auto old = snapshot(base);
    {
      Observer o(base + suffix);
      o.exec(QString("CREATE TRIGGER injected_failure BEFORE INSERT ON %1 "
                     "BEGIN SELECT RAISE(ABORT,'product lifecycle'); END")
                 .arg(table));
    }
    const auto error = owner.commit(fcard);
    check(error && error->code == "db_write_failure",
          "production transaction rejection");
    check(snapshot(base) == old, "all four stores unchanged after rollback");
    {
      Observer o(base + suffix);
      o.exec("DROP TRIGGER injected_failure");
    }
  }
  std::stop_source stopped;
  stopped.request_stop();
  check(owner.commit(fcard, stopped.get_token())->code == "cancelled",
        "cancel before transaction");
  count(base, 1);
  check(!owner.commit(fcard), "F1 recovery commit");
  check(owner.commit(card)->code == "duplicate_identity",
        "duplicate atomic rejection");
  owner.close();
  count(base, 2);
  {
    Observer o(base + ".sq.info");
    check(o.scalar("SELECT ledger FROM cif_provenance WHERE id=2300112")
                  .toString()
                  .toStdString() == card.record().provenance,
          "reopen full A1 ledger");
  }

  const std::vector<std::pair<QByteArray, QByteArray>> mutations{
      {"0.0075(11)", "?"},
      {"0.0075(11)", "."},
      {"0.0075(11)", "0"},
      {"0.0075(11)", "-0.0075"},
      {"0.0075(11)", "nan"},
      {"0.0075(11)", "2"},
      {"data_2300112", "data_different"},
      {"Uani", "Bani"},
      {"0.0036(9)", "0.0037(9)"},
      {"_atom_site_U_iso_or_equiv", "_unknown_unit"},
      {"O 0.007(2)", "O -0.007(2)"},
      {"O 0.007(2)", "Zn 0.007(2)"},
      {"O 0.007(2)", "Other 0.007(2)"}};
  for (const auto &[from, to] : mutations) {
    auto b = bytes(zinc);
    check(b.contains(from), "negative mutation target");
    b.replace(from, to);
    auto negative = owner.prepare_bytes(
        std::string_view(b.constData(), b.size()), "mutated.cif", 2300112);
    if (const auto *valid = std::get_if<PreparedCifImport>(&negative))
      check(valid->calculated()
                .imported()
                .provenance.displacement_choices.empty(),
            "changed input cannot activate A1");
    else
      check(std::get<CifImportFailure>(negative).code != "",
            "structured negative failure");
  }
  for (const auto &name :
       {"repository-KYbSe2", "repository-YInO3", "conflicting-hall-HM",
        "number-HM-conflict", "duplicate-tag", "missing-cell"}) {
    const auto rejected = owner.prepare_file(fixtures + name + ".cif", 1);
    check(std::holds_alternative<CifImportFailure>(rejected),
          "strict rejection retained");
  }
  check(std::holds_alternative<CifImportFailure>(
            owner.prepare_file(temp.path() + "/missing.cif", 1)),
        "missing source structured failure");
  const std::vector<CifBatchRequest> requests{
      {zinc.toStdString(), 2300112, {}},
      {(fixtures + "repository-CoO.cif").toStdString(), 1900001, {}},
      {formula.toStdString(), 1900004, {}}};
  auto batch =
      DatabaseBuilder::buildCifSources(buildUseCases(), temp.path() + "/batch", requests);
  check(batch.succeeded() && batch.committed == 2 && batch.failures.size() == 1,
        "default batch explicit skip policy");
  check(batch.failures[0].code == "multiple_structure_blocks" &&
            batch.failures[0].candidates.size() > 1,
        "complete ambiguity candidates");
  check(batch.cpp.parser_calls == 3 && batch.cpp.importer_calls == 3 &&
            batch.cpp.completed_phase13_18_chains == 2 &&
            batch.cpp.writer_calls == 2,
        "default product counters");
  check(batch.legacy_native_calls == 0, "default native calls zero");
  count(temp.path() + "/batch", 2);
  qputenv("QUALPF_CIF_DB_BACKEND", "invalid");
  auto invalid =
      DatabaseBuilder::buildCifSources(buildUseCases(), temp.path() + "/invalid", requests);
  check(invalid.fatal &&
            invalid.fatal->code == "invalid_cif_database_backend" &&
            !QFile::exists(temp.path() + "/invalid.sq"),
        "invalid backend fails before SQL");
  qunsetenv("QUALPF_CIF_DB_BACKEND");
  check(qualpf::legacy::cif_native_calls() == before,
        "no silent native fallback for failures");
  std::cout << "LIFECYCLE_JSON "
            << QJsonDocument(QJsonObject{{"status", "PASS"},
                                         {"checks", checks},
                                         {"batch", report(batch)}})
                   .toJson(QJsonDocument::Compact)
                   .constData()
            << '\n';
}
void legacy_probe(const QString &root) {
  QTemporaryDir temp;
  check(temp.isValid(), "canonical compatibility probe directory");
  const auto path = temp.path() + "/1900008.cif";
  save(path, bytes(root + "/tests/data/cif/CoO.cif"));
  qputenv("QUALPF_CIF_DB_BACKEND", "legacy");
  const auto selected = cif_database_backend_from_environment();
  check(std::holds_alternative<CifDatabaseBackend>(selected) &&
            std::get<CifDatabaseBackend>(selected) == CifDatabaseBackend::cpp,
        "literal legacy selects explicit canonical C++ compatibility owner");
  const std::vector<CifBatchRequest> requests{{path.toStdString(), 1900008,
      {cif::BlockSelectionPolicy::specified_block, "sm_isp_sd1835648-standardized_unitcell"}}};
  const auto value = DatabaseBuilder::buildCifSources(buildUseCases(), temp.path() + "/canonical", requests);
  check(value.succeeded() && value.committed == 1 && value.failures.empty(), "canonical compatibility complete card");
  check(value.backend == CifDatabaseBackend::cpp && value.legacy_native_calls == 0 &&
            value.cpp.parser_calls > 0 && value.cpp.completed_phase13_18_chains == 1,
        "canonical compatibility has truthful C++ ownership and native zero");
  for (const auto *invalid : {"invalid", "LEGACY", "Cpp", "native"}) {
    qputenv("QUALPF_CIF_DB_BACKEND", invalid);
    const auto output = temp.path() + "/invalid-" + invalid;
    const auto failure = DatabaseBuilder::buildCifSources(buildUseCases(), output, requests);
    check(failure.fatal && failure.fatal->code == "invalid_cif_database_backend" &&
              !QFileInfo::exists(output + ".sq") && failure.legacy_native_calls == 0,
          "unknown selectors preserve deterministic error before SQL/native");
  }
  qunsetenv("QUALPF_CIF_DB_BACKEND");
  std::cout << "LEGACY_JSON " << QJsonDocument(report(value)).toJson(QJsonDocument::Compact).constData() << '\n';
}
void gui_probe(const QString &root, const QString &block) {
  qunsetenv("QUALPF_CIF_DB_BACKEND");
  QTemporaryDir temp;
  check(temp.isValid(), "GUI probe directory");
  save(
      temp.path() + "/1900008.cif",
      bytes(root + "/tests/data/cif/CoO.cif"));
  bool sawSelection = false, explicitlySelected = false,
       initiallyDisabled = false;
  QTimer choose;
  QObject::connect(&choose, &QTimer::timeout, [&] {
    for (auto *widget : QApplication::topLevelWidgets()) {
      if (widget->objectName() != "cifBlockSelectionDialog")
        continue;
      auto *dialog = qobject_cast<QDialog *>(widget);
      auto *table = dialog->findChild<QTableWidget *>("cifBlockCandidates");
      auto *buttons = dialog->findChild<QDialogButtonBox *>();
      if (!table || !buttons)
        continue;
      sawSelection = true;
      initiallyDisabled = !buttons->button(QDialogButtonBox::Ok)->isEnabled() &&
                          table->selectedItems().isEmpty();
      for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0)->text() != block)
          continue;
        table->selectRow(row);
        if (buttons->button(QDialogButtonBox::Ok)->isEnabled()) {
          explicitlySelected = true;
          choose.stop();
          buttons->button(QDialogButtonBox::Ok)->click();
          return;
        }
      }
      choose.stop();
      dialog->reject();
    }
  });
  choose.start(10);
  bool cancelled = false;
  check(DatabaseBuilder::buildCifDatabase(buildUseCases(),
            temp.path() + "/gui", temp.path(), false,
            static_cast<QWidget *>(nullptr), &cancelled),
        "GUI production entry");
  check(!cancelled && sawSelection && explicitlySelected && initiallyDisabled,
        "GUI requires explicit selection; no default selected row");
  Observer o(temp.path() + "/gui.sq.info");
  check(o.scalar("SELECT block FROM cif_provenance WHERE id=1900008")
                .toString()
                .compare(block, Qt::CaseInsensitive) == 0,
        "GUI selected block persisted");
  std::cout << "GUI_JSON "
            << QJsonDocument(
                   QJsonObject{{"status", "PASS"},
                               {"block", block},
                               {"explicit_selection_required", true},
                               {"legacy_native_calls",
                                qint64(qualpf::legacy::cif_native_calls())}})
                   .toJson(QJsonDocument::Compact)
                   .constData()
            << '\n';
}
} // namespace

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  try {
    if (argc == 6 && std::string_view(argv[1]) == "--interrupt-writer") {
      CifDatabaseImportService service;
      const auto root = QString::fromLocal8Bit(argv[2]);
      check(!service.create(QString::fromLocal8Bit(argv[3])),
            "interrupt database create");
      auto result = service.prepare_file(
          root + "/tests/data/cif/cod-2300112.cif",
          2300112);
      check(std::holds_alternative<PreparedCifImport>(result),
            "interrupt preparation");
      save(QString::fromLocal8Bit(argv[4]), "ready");
      QElapsedTimer timer;
      timer.start();
      while (!QFile::exists(QString::fromLocal8Bit(argv[5])) &&
             timer.elapsed() < 30000)
        QThread::msleep(10);
      check(QFile::exists(QString::fromLocal8Bit(argv[5])),
            "interrupt start signal");
      auto error = service.commit(std::get<PreparedCifImport>(result));
      if (error)
        std::cerr << error->code << ": " << error->message << '\n';
      return error ? 1 : 0;
    }
    if (argc >= 3 && std::string_view(argv[1]) == "--lifecycle") {
      lifecycle(QString::fromLocal8Bit(argv[2]));
      return 0;
    }
    if (argc == 3 && std::string_view(argv[1]) == "--legacy") {
      legacy_probe(QString::fromLocal8Bit(argv[2]));
      return 0;
    }
    if (argc == 4 && std::string_view(argv[1]) == "--gui") {
      gui_probe(QString::fromLocal8Bit(argv[2]),
                QString::fromLocal8Bit(argv[3]));
      return 0;
    }
    if (argc == 5 && std::string_view(argv[1]) == "--build") {
      const auto input =
          QJsonDocument::fromJson(bytes(QString::fromLocal8Bit(argv[2])))
              .object();
      std::vector<CifBatchRequest> requests;
      for (const auto &v : input["cards"].toArray()) {
        const auto obj = v.toObject();
        cif::BlockSelection selection;
        if (!obj["block"].toString().isEmpty())
          selection = {cif::BlockSelectionPolicy::specified_block,
                       obj["block"].toString().toStdString()};
        requests.push_back(
            {obj["source"].toString().toStdString(), obj["identity"].toInt(), selection});
      }
      auto value = DatabaseBuilder::buildCifSources(buildUseCases(),
          QString::fromLocal8Bit(argv[3]), requests);
      save(QString::fromLocal8Bit(argv[4]),
           QJsonDocument(report(value)).toJson());
      return value.succeeded() ? 0 : 1;
    }
    if (argc == 5 && std::string_view(argv[1]) == "--prepare") {
      CifDatabaseImportService service;
      cif::BlockSelection selection;
      if (std::string_view(argv[4]) != "-")
        selection = {cif::BlockSelectionPolicy::specified_block, argv[4]};
      auto prepared = service.prepare_file(QString::fromLocal8Bit(argv[2]),
                                           std::stoi(argv[3]), selection);
      if (const auto *e = std::get_if<CifImportFailure>(&prepared)) {
        std::cout << "FAILURE_JSON "
                  << QJsonDocument(failure(*e))
                         .toJson(QJsonDocument::Compact)
                         .constData()
                  << '\n';
      } else {
        const auto &p = std::get<PreparedCifImport>(prepared);
        CifDatabaseImportService reopened;
        auto roundtrip = reopened.reload(p.ticket(), std::stoi(argv[3]));
        check(std::holds_alternative<PreparedCifImport>(roundtrip),
              "production ticket reload");
        const auto &r = std::get<PreparedCifImport>(roundtrip);
        std::ostringstream first, second;
        auto *previous = std::cout.rdbuf(first.rdbuf());
        dump(p.record());
        std::cout.rdbuf(second.rdbuf());
        dump(r.record());
        std::cout.rdbuf(previous);
        check(first.str() == second.str() &&
                  p.record().provenance == r.record().provenance,
              "full DTO and ledger exact ticket roundtrip");
        dump(p.record());
        std::cout
            << "PROVENANCE_JSON "
            << QJsonDocument(
                   QJsonObject{
                       {"source_sha256",
                        QString::fromStdString(p.record().source_sha256)},
                       {"structure_identity",
                        QString::fromStdString(p.record().structure_identity)},
                       {"block", QString::fromStdString(p.record().block)},
                       {"ledger",
                        QString::fromStdString(p.record().provenance)}})
                   .toJson(QJsonDocument::Compact)
                   .constData()
            << '\n';
      }
      std::cout << "COUNTERS_JSON "
                << QJsonDocument(counters(service.counters()))
                       .toJson(QJsonDocument::Compact)
                       .constData()
                << '\n';
      return 0;
    }
    return 2;
  } catch (const std::exception &e) {
    std::cerr << "FAIL " << checks << ": " << e.what() << '\n';
    return 1;
  }
}
