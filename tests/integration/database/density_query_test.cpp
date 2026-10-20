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
static void check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("Density query at line " + std::to_string(at.line()));
}
static void write(const QString &path, const QStringList &statements) {
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", "density-fixture");
        db.setDatabaseName(path);
        if (!db.open()) throw std::runtime_error("Density fixture open: " + db.lastError().text().toStdString());
        for (const auto &sql : statements) {
            QSqlQuery q(db);
            if (!q.exec(sql)) throw std::runtime_error(q.lastError().text().toStdString());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase("density-fixture");
}
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    check(dir.isValid());
    const auto base = dir.path() + "/density";
    write(base + ".sq", {
        "CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER)",
        "INSERT INTO id VALUES('20','other','m','F','P1','A','1',1,'2','100',1),('10','wanted','m','F','P1','A','1',1,'2','100',1),('30','wanted','m','F','P1',' D ','1',1,'2','100',1),('40','missing','m','F','P1','A','1',1,'2','100',1),('50','invalid','m','F','P1','A','1',1,'2','100',1),('60','null','m','F','P1','A','1',1,'2','100',1)",
        "CREATE TABLE chemical(id TEXT,chemical_element TEXT)",
        "INSERT INTO chemical VALUES('20','Fe'),('20','O'),('20','O'),('10','Fe'),('30','O')"});
    write(base + ".sq.info", {"CREATE TABLE spgrstat(spacegroup TEXT,ids TEXT,n INTEGER)",
        "INSERT INTO spgrstat VALUES('P1','10,30',2)"});
    write(base + ".sq.infostat", {
        "CREATE TABLE stat_cdens(val REAL,ids TEXT,n INTEGER)",
        "INSERT INTO stat_cdens VALUES(2,'10,10',2),(4,'20,30',2),(-2,'50',1),(NULL,'60',1),('invalid','50',1)",
        "CREATE TABLE stat_dens(val REAL,ids TEXT,n INTEGER)",
        "INSERT INTO stat_dens VALUES(8,'10',1),(9,'20',1),(10,'30',1)",
        "CREATE TABLE stat_a(val REAL,ids TEXT,n INTEGER)",
        "INSERT INTO stat_a VALUES(5,'10,30',2)",
        "CREATE TABLE stat_color(val TEXT,ids TEXT,n INTEGER)",
        "INSERT INTO stat_color VALUES('red','10',1)"});
    write(base + ".sq.search", {"CREATE TABLE top(id TEXT,n INTEGER,dval TEXT)",
        "INSERT INTO top VALUES('20',1,'3'),('10',1,'3'),('10',1,'3'),('30',1,'3'),('40',1,'3'),('50',1,'3'),('60',1,'3')"});
    infrastructure::SqlReferenceRepository reader;
    check(reader.openDatabases(base));
    const ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "density"};
    // Same criteria through matching, legacy strongest, selected IDs and browse.
    for (int route = 0; route != 4; ++route) {
        CandidateQuery q;
        q.wavelength = 1.54; q.d = {3}; q.deltaD = {.1};
        q.route = route == 1 ? CandidateQuery::Route::stored_strongest :
                  route == 3 ? CandidateQuery::Route::constrained_browse : CandidateQuery::Route::matching;
        if (route == 2) q.preselectedIds = std::vector<std::string>{"30","10","20","10","40","50","60"};
        const auto expect = [&](std::initializer_list<const char *> ids) {
            auto found = reader.read(q, context, {});
            check(bool(found));
            std::vector<std::string> actual;
            for (const auto &card : found.value()) actual.push_back(card.id);
            std::vector<std::string> expected;
            for (const char *id : ids) expected.emplace_back(id);
            check(actual == expected);
        };
        q.calculatedDensityMinimum = 2; q.calculatedDensityMaximum = 4;
        expect({"20","10","30"}); // Inclusive endpoints; original row order, no duplicate rows.
        q.minimumSpecies = 2;
        expect({"20"}); // Distinct species, not duplicate chemical rows.
        q.minimumSpecies = 0; q.maximumSpecies = 1;
        expect({"10","30"});
        q.maximumSpecies = 0;
        q.calculatedDensityMinimum = q.calculatedDensityMaximum = 2;
        expect({"10"});
        q.calculatedDensityMinimum = q.calculatedDensityMaximum = -1;
        q.measuredDensityMinimum = q.measuredDensityMaximum = 9;
        expect({"20"}); // Separate measured table; no choice/substitution between fields.
        q.calculatedDensityMinimum = 2; q.calculatedDensityMaximum = 4;
        q.measuredDensityMinimum = 8; q.measuredDensityMaximum = 9;
        expect({"20","10"});
        q.cellMinimum[0] = q.cellMaximum[0] = 5;
        expect({"10"});
        q.names = "wanted"; q.elements = "Fe"; q.spaceGroups = {"P1"}; q.colors = {"red"};
        expect({"10"});
        q.calculatedDensityMinimum = 6; q.calculatedDensityMaximum = 7;
        expect({}); // No-hit restraint must never be replaced by later positive restraints.
        q.calculatedDensityMinimum = 2; q.calculatedDensityMaximum = 4;
        q.colors = {"absent"}; expect({});
        q.colors = {"red"}; q.spaceGroups = {"absent"}; expect({});
        q.spaceGroups = {"P1"}; q.cellMinimum[0] = q.cellMaximum[0] = 6; expect({});
        q.cellMinimum[0] = q.cellMaximum[0] = 5; q.measuredDensityMinimum = 20; q.measuredDensityMaximum = 21; expect({});
        q.measuredDensityMinimum = 8; q.measuredDensityMaximum = 10;
        q.cellMinimum[0] = q.cellMaximum[0] = -1; q.names.clear(); q.elements.clear(); q.spaceGroups.clear(); q.colors.clear();
        q.excludeDeleted = true; expect({"20","10"});
        q.ids = {"30"}; expect({});
    }
    std::cout << "PASS: density intersections, boundaries, missing/invalid values, no hits and four real SQLite routes\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
