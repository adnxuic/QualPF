#include "infrastructure/database/database_access.h"
#include <QFile>
#include "infrastructure/database/database_build_provider.h"
#include "infrastructure/network/update_service.h"
#include "infrastructure/platform/platform_services.h"
#include "infrastructure/settings/preference_store.h"
#include "infrastructure/settings/search_settings.h"
#include "infrastructure/io/product_files.h"
#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "infrastructure/settings/database_catalog_store.h"
#include "gui/mainwindow.h"
#include "gui/presentation/product_plot_refresh.h"
#include "gui/search/search_request.h"
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include "infrastructure/settings/database_settings.h"
#include "infrastructure/settings/progkeysettings.h"
#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::gui;
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void isolation() {
    infrastructure::QtPreferenceStore preferencesStore;
    infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    infrastructure::QtUpdateServiceProvider updates;
    infrastructure::QtPlatformServices platform;
    infrastructure::ProductFileProviders files;
    Workspace first, second;
    auto factory = std::make_shared<infrastructure::SqlCandidateReaderFactory>();
    infrastructure::DatabaseCatalogSettings settings;
    infrastructure::SqlReferenceRepository firstRepository, secondRepository;
    DatabaseCatalog firstCatalog(first.searches(), settings, firstRepository), secondCatalog(second.searches(), settings, secondRepository);
    WorkspaceContext left(first, firstCatalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, factory), right(second, secondCatalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, factory);
    left.presentation().configure(PresentationMode::automatic_gui);
    right.presentation().configure(PresentationMode::automatic_gui);
    const auto beforeLeft = first.session().snapshot(), beforeRight = second.session().snapshot();
    {
        MainWindow one(left), two(right);
        check(&workspaceContext(one.xpdViewer()) == &left && &workspaceContext(two.xpdViewer()) == &right,
              "Designer children must resolve their own root context");
        auto next = *beforeLeft;
        next.projectMetadata["context_marker"] = std::int64_t{17};
        check(bool(first.session().commit(beforeLeft, std::move(next))), "isolated state commit");
        check(first.session().snapshot() != beforeLeft && second.session().snapshot() == beforeRight,
              "one workspace commit must not change another canonical owner");
        check(left.searchSession().snapshot()->canonical == first.session().snapshot() &&
                  right.searchSession().snapshot()->canonical == beforeRight,
              "Qt views must derive from their own canonical state");
        check(bool(qualpf::gui::presentation::request_product_plot_refresh(left)) &&
                  bool(qualpf::gui::presentation::request_product_plot_refresh(right)),
              "instance presentation binding");
    }
    check(!qualpf::gui::presentation::request_product_plot_refresh(left) &&
              !qualpf::gui::presentation::request_product_plot_refresh(right),
          "window destruction unbinds presentation");
    int observed = 0;
    {
        SessionView extra(first.session());
        QObject::connect(&extra, &SessionView::changed, &extra, [&] { ++observed; });
        auto source = first.session().snapshot();
        auto next = *source;
        next.projectMetadata["context_marker"] = std::int64_t{18};
        check(bool(first.session().commit(source, std::move(next))) && observed == 1, "view subscription");
    }
    auto source = first.session().snapshot();
    auto next = *source;
    next.projectMetadata["context_marker"] = std::int64_t{19};
    check(bool(first.session().commit(source, std::move(next))) && observed == 1,
          "destroyed view subscription cannot be called");
    QObject orphan;
    bool rejected = false;
    try {
        workspaceContext(&orphan);
    } catch (const std::logic_error &) {
        rejected = true;
    }
    check(rejected, "an unbound object cannot silently acquire a global workspace");
}
void presenterLifetime() {
    infrastructure::QtPreferenceStore preferencesStore;
    infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    infrastructure::QtUpdateServiceProvider updates;
    infrastructure::QtPlatformServices platform;
    infrastructure::ProductFileProviders files;
    Workspace product;
    infrastructure::DatabaseCatalogSettings settings;
    infrastructure::SqlReferenceRepository repository;
    DatabaseCatalog catalog(product.searches(), settings, repository);
    WorkspaceContext context(product, catalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, std::make_shared<infrastructure::SqlCandidateReaderFactory>());
    context.presentation().configure(PresentationMode::automatic_gui);
    int calls = 0;
    {
        QObject owner, foreign;
        context.bindPresenter(&owner, [&] {
            ++calls;
            return Status{std::monostate{}};
        });
        context.unbindPresenter(&foreign);
        check(bool(context.refreshPresentation()) && calls == 1, "foreign owner cannot unbind a presenter");
    }
    check(!context.refreshPresentation() && calls == 1, "expired QObject presenter is never invoked");
    QObject owner;
    context.bindPresenter(&owner, [&] {
        ++calls;
        return Status{std::monostate{}};
    });
    product.searches().close();
    const auto closing = context.refreshPresentation();
    check(!closing && closing.error().reason == "closing" && calls == 1, "Closing rejects presentation");
}
void selectionAndRequest(const QString &directory) {
    QSettings preferences;
    preferences.setValue(DB_COUNT_KEY, 17);
    infrastructure::QtPreferenceStore preferencesStore;
    infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    infrastructure::QtUpdateServiceProvider updates;
    infrastructure::QtPlatformServices platform;
    infrastructure::ProductFileProviders files;
    Workspace product;
    infrastructure::DatabaseCatalogSettings settings;
    infrastructure::SqlReferenceRepository repository;
    DatabaseCatalog catalog(product.searches(), settings, repository, std::numeric_limits<std::uint64_t>::max());
    WorkspaceContext exhausted(product, catalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, std::make_shared<infrastructure::SqlCandidateReaderFactory>());
    gui::DatabaseEntry entry;
    entry.inUse = true;
    entry.name = "independent database";
    entry.path = directory + "/must-not-open";
    const auto source = product.session().snapshot();
    auto failed = exhausted.setDatabases({entry});
    check(!failed && failed.error().reason == "revision_exhausted", "bounded database selection epoch");
    check(exhausted.databases().isEmpty() && exhausted.referenceDatabase().basePath.empty() &&
              exhausted.referenceDatabase().selectionVersion == std::numeric_limits<std::uint64_t>::max() &&
              preferences.value(DB_COUNT_KEY).toInt() == 17 && !QFileInfo::exists(entry.path + ".sq") &&
              product.session().snapshot() == source,
          "exhaustion cannot partly change settings, SQL or Session");
    Workspace active;
    infrastructure::SqlReferenceRepository activeRepository;
    DatabaseCatalog activeCatalog(active.searches(), settings, activeRepository);
    WorkspaceContext context(active, activeCatalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, std::make_shared<infrastructure::SqlCandidateReaderFactory>());
    auto task = active.searches().begin(context.referenceDatabase());
    check(bool(task), "active selection fixture admission");
    failed = context.setDatabases({entry});
    check(!failed && active.searches().cancelled() && context.databases().isEmpty() &&
              preferences.value(DB_COUNT_KEY).toInt() == 17,
          "Busy selection cancels without partial persistence");
    auto retired = active.searches().complete(
        task.value().id, Result<PreparedSearchResult>{Error{ErrorCode::cancelled, "test", "cancelled"}});
    check(!retired && !active.searches().running(), "cancelled fixture retires");
    SearchRequest request;
    check(request.getMinFom() == .35 && request.getWeight2thetaD() == .50 &&
              request.getWeightIntensity() == .50 && request.getWeightPhases() == .50 &&
              request.getDelta2theta() == .08 && !request.getCalcFom(),
          "existing boundary defaults");
    request.setCalcFom(true);
    request.setMinFom(.7);
    request.setElements("Si O");
    auto query = request.candidateQuery(CandidateQuery::Route::matching);
    check(query.decodeReflections && query.elements == "Si O", "SQL gets data projection and filters");
    request.initialize();
    check(request.getMinFom() == .35 && !request.getCalcFom() && !request.decodeReflections(),
          "request reset preserves existing scoring and projection defaults");
}
void removalAdmission(const QString &directory) {
    Workspace workspace;
    infrastructure::DatabaseCatalogSettings store;
    infrastructure::SqlReferenceRepository repository;
    DatabaseCatalog catalog(workspace.searches(), store, repository);
    const auto base = directory + "/remove-fixture";
    for (const auto *suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"}) {
        QFile file(base + suffix);
        check(file.open(QIODevice::WriteOnly), "owned database removal fixture");
        file.write("fixture");
    }
    std::vector<ReferenceDatabaseEntry> entries{{true, "remove", 1, "fixture", base.toStdString()},
                                               {false, "remaining", 2, "fixture", (directory + "/remaining").toStdString()}};
    store.save(entries);
    const auto source = workspace.session().snapshot();
    auto read = infrastructure::DatabaseLease::read(base);
    check(bool(read), "removal read admission fixture");
    auto busy = catalog.removeDraft(entries, 0, 0, true);
    check(!busy && busy.error().reason == "database_busy" && QFileInfo::exists(base + ".sq") &&
          QSettings{}.value(DB_COUNT_KEY).toInt() == 2 && workspace.session().snapshot() == source,
          "busy removal has no file, settings or product mutation");
    read.reset();
    bool observed = false;
    auto removed = catalog.removeDraft(entries, 0, 0, true, [&](const auto &result) {
        observed = true;
        check(!infrastructure::DatabaseLease::write(base) && QSettings{}.value(DB_COUNT_KEY).toInt() == 2 && result.entries.size() == 1,
              "write lease spans pre-persistence failure presentation with original settings timing");
    });
    check(observed, "single removal presentation");
    check(bool(removed) && removed.value().activeIndex == 0 && removed.value().entries.size() == 1 &&
          removed.value().entries[0].inUse && removed.value().failedFiles.empty() && QSettings{}.value(DB_COUNT_KEY).toInt() == 1,
          "draft removal reassigns active entry and persists under lease");
    for (const auto *suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"})
        check(!QFileInfo::exists(base + suffix), "all original database files removed");
    check(catalog.entries().empty() && workspace.session().snapshot() == source,
          "dialog draft removal does not publish a canonical selection");
}
} // namespace
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid())
        return 2;
    QCoreApplication::setOrganizationName("QualPF architecture tests");
    QCoreApplication::setApplicationName("workspace-context");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory.path());
    try {
        isolation();
        presenterLifetime();
        selectionAndRequest(directory.path());
        removalAdmission(directory.path());
        std::cout << "PASS: explicit workspace ownership, isolation, subscription and bounded selection\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
