#include "gui/search/search_task_controller.h"
#include "gui/model/cardtype.h"
#include <QEventLoop>
#include <semaphore>
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include "infrastructure/database/qt_sql/sql_schema_builder.h"
#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include <QCoreApplication>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <atomic>
#include <iostream>
#include <latch>
#include <mutex>
#include <set>
#include <source_location>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::infrastructure;
static void check(bool value, std::source_location where = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("Threaded repository contract at line " + std::to_string(where.line()));
}
static void file(const QString &path, const QStringList &sql) {
    const QString name = "threaded-fixture";
    {
        auto db = QSqlDatabase::addDatabase("QSQLITE", name);
        db.setDatabaseName(path);
        check(db.open());
        for (const auto &statement : sql) {
            QSqlQuery query(db);
            if (!query.exec(statement))
                throw std::runtime_error(query.lastError().text().toStdString());
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}
static void fixture(const QString &base, const QString &label) {
    file(base + ".sq", {"CREATE TABLE id(id TEXT,name TEXT,mineralname TEXT,chemical_formula TEXT,spacegroup "
                        "TEXT,quality TEXT,rir TEXT,nrec INTEGER,dvalue TEXT,intensita TEXT,n INTEGER,nd INTEGER)",
                        "INSERT INTO id VALUES('10','" + label + "','m','F','P1','A','1',1,'2','100',1,1)"});
    file(base + ".sq.info", {"CREATE TABLE info(id TEXT)"});
    file(base + ".sq.infostat", {"CREATE TABLE unused(id TEXT)"});
    file(base + ".sq.search", {"CREATE TABLE top(id TEXT,n INTEGER,dval TEXT)"});
}
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid());
    QDir().mkpath(directory.path() + "/a");
    QDir().mkpath(directory.path() + "/b");
    const auto a = directory.path() + "/a/cards", b = directory.path() + "/b/cards";
    fixture(a, "first");
    fixture(b, "second");
    const auto original = QSqlDatabase::connectionNames();
    ExecutionContext context{{}, NumericalPolicy::legacy_mixed_precision, "thread-test"};
    CandidateQuery query;
    query.wavelength = 1.54;
    {
        qualpf::infrastructure::SqlReferenceRepository first, second;
        check(first.openDatabases(a) && second.openDatabases(b));
        auto left = first.read(query, context, {}), right = second.read(query, context, {});
        check(left && right && left.value()[0].chemicalName == "first" &&
              right.value()[0].chemicalName == "second");
        first.closeDatabeses();
        check(bool(second.read(query, context, {})));
        bool rejected = false;
        std::thread wrong([&] {
            auto r = second.read(query, context, {});
            rejected = !r && r.error().field == "database.thread";
        });
        wrong.join();
        check(rejected);
    }
    check(QSqlDatabase::connectionNames() == original);
    std::mutex mutex;
    std::vector<ConnectionEvent> events;
    SqlCandidateReaderFactory factory([&](const auto &event) {
        std::lock_guard lock(mutex);
        events.push_back(event);
    });
    std::latch querying(1), release(1);
    std::atomic<bool> announced = false;
    auto announce = [&] {
        if (!announced.exchange(true))
            querying.count_down();
    };
    std::stop_source stop;
    std::exception_ptr failure;
    std::thread::id workerId;
    std::thread worker([&] {
        workerId = std::this_thread::get_id();
        try {
            auto reader = factory.open({a.toStdString(), 5}, 77,
                                       {stop.get_token(), context.numerical_policy, "worker"});
            check(bool(reader));
            auto result = reader.value()->read(query, {stop.get_token(), context.numerical_policy, "worker"},
                                               [&](auto, auto) {
                                                   announce();
                                                   release.wait();
                                               });
            check(!result && result.error().code == ErrorCode::cancelled);
            reader.value().reset();
        } catch (...) {
            failure = std::current_exception();
            announce();
        }
    });
    querying.wait();
    const bool writeExcluded = !DatabaseLease::write(a);
    qualpf::infrastructure::SqlSchemaBuilder blockedWriter;
    const bool creatorExcluded = !blockedWriter.create(a); // Admission precedes its historical overwrite.
    stop.request_stop();
    release.count_down();
    worker.join();
    if (failure)
        std::rethrow_exception(failure);
    check(writeExcluded && creatorExcluded);
    check(bool(DatabaseLease::write(a)));
    std::set<std::string> opened, removed;
    for (const auto &event : events) {
        check(event.thread == workerId && event.name.find("_task_77_") != std::string::npos);
        if (event.action == ConnectionAction::opened)
            opened.insert(event.name);
        if (event.action == ConnectionAction::removed)
            removed.insert(event.name);
    }
    check(opened.size() == 4 && opened == removed && QSqlDatabase::connectionNames() == original);
    auto cancelled =
        factory.open({a.toStdString(), 5}, 78, {stop.get_token(), context.numerical_policy, "cancelled"});
    check(!cancelled && cancelled.error().code == ErrorCode::cancelled);
    std::stop_source openingStop;
    int openedCount = 0, removedCount = 0;
    SqlCandidateReaderFactory openingFactory([&](const auto &event) {
        if (event.action == ConnectionAction::opened) {
            ++openedCount;
            openingStop.request_stop();
        }
        if (event.action == ConnectionAction::removed)
            ++removedCount;
    });
    auto duringOpen = openingFactory.open({a.toStdString(), 5}, 82,
                                          {openingStop.get_token(), context.numerical_policy, "open"});
    check(!duringOpen && duringOpen.error().code == ErrorCode::cancelled && openedCount == 1 &&
          removedCount == 1);
    check(QSqlDatabase::connectionNames() == original);
    const auto incomplete = directory.path() + "/incomplete";
    file(incomplete + ".sq", {"CREATE TABLE id(id TEXT)"});
    auto failed = factory.open({incomplete.toStdString(), 5}, 79, context);
    check(!failed && QSqlDatabase::connectionNames() == original);
    {
        auto outer = DatabaseLease::read(a), inner = DatabaseLease::read(a);
        check(bool(outer) && bool(inner) && !DatabaseLease::write(a));
        inner.reset();
        check(!DatabaseLease::write(a));
    }
    auto written = DatabaseLease::write(a);
    check(bool(written));
    auto busy = factory.open({a.toStdString(), 5}, 80, context);
    check(!busy && busy.error().reason == "busy");
    written.reset();
    auto reader = factory.open({a.toStdString(), 5}, 81, context);
    check(bool(reader));
    query.elements = "Fe";
    auto sqlFailure = reader.value()->read(query, context, {});
    check(!sqlFailure && sqlFailure.error().field == "database.query");
    reader.value().reset();
    check(QSqlDatabase::connectionNames() == original);
    {
        auto guarded = factory.protect({a.toStdString(),5});
        check(bool(guarded) && !DatabaseLease::write(a));
        auto lookup = factory.open({a.toStdString(),5},83,context);
        check(bool(lookup));
        auto card = lookup.value()->findById("10",1.54,context);
        CardType reference; reference.setD({2},1.54);
        const auto angles = reference.getTth();
        check(card && card.value().tth == std::vector<double>(angles.begin(),angles.end()) &&
              card.value().d == std::vector<double>{2} && card.value().rir == "1.0000");
        check(!lookup.value()->findById("99",1.54,context));
    }
    check(bool(DatabaseLease::write(a)));
    {
        Workspace workspace;
        std::binary_semaphore queued{0}, allowReturn{0};
        qualpf::gui::SearchTaskController controller(workspace.searches(),
            std::make_shared<SqlCandidateReaderFactory>(),nullptr,
            [&](auto event,auto,auto){
                if(event == qualpf::gui::SearchTaskController::TaskEvent::completion_queued) {
                    queued.release(); allowReturn.acquire();
                }
            });
        SearchInput input; input.operation=SearchOperation::metadata_browse;
        auto source=workspace.session().snapshot();
        check(bool(controller.start(input,{a.toStdString(),0})));
        queued.acquire();
        // SQL handles have already left the worker. The pending result still
        // excludes a project-managed writer until owner delivery is retired.
        check(QSqlDatabase::connectionNames() == original && !DatabaseLease::write(a));
        controller.cancel(); allowReturn.release();
        QEventLoop loop;
        QObject::connect(&controller,&qualpf::gui::SearchTaskController::finished,&loop,&QEventLoop::quit);
        loop.exec();
        check(workspace.session().snapshot() == source && bool(DatabaseLease::write(a)));
    }
    std::cout << "PASS: same-name isolation, worker SQL ownership/removal, cancel barriers, writer "
                 "exclusion, failure cleanup\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
