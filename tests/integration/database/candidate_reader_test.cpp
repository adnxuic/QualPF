#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v, std::source_location where = std::source_location::current()) {
    if (!v)
        throw std::runtime_error("CandidateReader contract failed at line " + std::to_string(where.line()));
}
static void file(const QString &path, const QStringList &statements) {
    const QString name = "fixture-writer";
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        check(db.open());
        for (const auto &sql : statements) {
            QSqlQuery q(db);
            if (!q.exec(sql))
                throw std::runtime_error(q.lastError().text().toStdString());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    check(dir.isValid());
    const auto base = dir.path() + "/cards";
    file(base + ".sq", {"CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup "
                        "TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER)",
                        "INSERT INTO id VALUES('10','first','m','F','P1','A','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('20','second','m','F','P1','A','1',1,'2','100',1)",
                        "INSERT INTO id VALUES('30','deleted','m','F','P1',' D ','1',1,'2','100',1)"});
    file(base + ".sq.info", {"CREATE TABLE info(id TEXT)"});
    file(base + ".sq.infostat", {"CREATE TABLE unused(id TEXT)"});
    file(base + ".sq.search",
         {"CREATE TABLE top(id TEXT,n INTEGER,dval TEXT)", "INSERT INTO top VALUES('10',1,'3')",
          "INSERT INTO top VALUES('20',1,'9')", "INSERT INTO top VALUES('30',1,'3')"});
    qualpf::infrastructure::SqlReferenceRepository reader;
    check(reader.openDatabases(base));
    ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "repository-test"};
    CandidateQuery query;
    query.wavelength = 1.54;
    auto all = reader.read(query, context, {});
    check(bool(all) && all.value().size() == 3);
    check(all.value()[0].id == "10" && !all.value()[0].fomCalculated && all.value()[0].fom == 0 &&
          all.value()[0].tth.size() == 1);
    query.excludeDeleted = true;
    query.route = CandidateQuery::Route::stored_strongest;
    query.d = {3};
    query.deltaD = {.1};
    auto strongest = reader.read(query, context, {});
    check(bool(strongest) && strongest.value().size() == 1 && strongest.value()[0].id == "10");
    // Stored top controls filtering; decoding d=2 does not replace its saved d=3.
    query.route = CandidateQuery::Route::constrained_browse;
    query.ids = {"20"};
    query.decodeReflections = false;
    auto metadata = reader.read(query, context, {});
    check(bool(metadata) && metadata.value().size() == 1 && metadata.value()[0].tth.empty() &&
          metadata.value()[0].d.empty());
    std::stop_source stop;
    context.cancellation = stop.get_token();
    auto cancelled = reader.read(query, context, [&](auto, auto) { stop.request_stop(); });
    check(!cancelled && cancelled.error().code == ErrorCode::cancelled);
    context.cancellation = {};
    query.elements = "Fe";
    query.ids.clear();
    // Missing chemical table is a failed query, never successful empty output.
    auto failed = reader.read(query, context, {});
    check(!failed);
    reader.closeDatabeses();
    std::cout << "PASS: owned unscored rows, saved strongest index, metadata browse, cancellation and SQL "
                 "failure\n";
} catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
}
