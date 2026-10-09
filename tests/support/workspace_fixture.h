#include "infrastructure/database/database_build_provider.h"
#include "infrastructure/network/update_service.h"
#include "infrastructure/platform/platform_services.h"
#include "infrastructure/settings/preference_store.h"
#include "infrastructure/settings/search_settings.h"
#include "infrastructure/settings/database_settings.h"
#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "infrastructure/settings/database_catalog_store.h"
#pragma once
#include "infrastructure/io/product_files.h"
#include "gui/state/workspace_context.h"
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include <QCoreApplication>
#include <stdexcept>
class AppState {
    struct Fixture : QObject {
        qualpf::infrastructure::QtPreferenceStore preferences;
        qualpf::infrastructure::settings::SearchSettings searchSettings;
        qualpf::infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    qualpf::application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    qualpf::infrastructure::QtUpdateServiceProvider updates;
    qualpf::infrastructure::QtPlatformServices platform;
    qualpf::infrastructure::ProductFileProviders files;
        qualpf::infrastructure::DatabaseCatalogSettings settings;
        qualpf::infrastructure::SqlReferenceRepository repository;
        qualpf::application::Workspace product;
        qualpf::application::DatabaseCatalog catalog;
        qualpf::gui::WorkspaceContext context;
        explicit Fixture(QObject *parent)
            : QObject(parent), product(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy"
                                           ? qualpf::application::SearchBackend::legacy_fallback
                                           : qualpf::application::SearchBackend::cpp),
              catalog(product.searches(), settings, repository),
              context(product, catalog, files.ports, preferences, searchSettings, platform, updates, databaseBuild, std::make_shared<qualpf::infrastructure::SqlCandidateReaderFactory>()) {}
    };

  public:
    static qualpf::gui::WorkspaceContext &context() {
        static QPointer<Fixture> fixture;
        if (!fixture) {
            if (!QCoreApplication::instance())
                throw std::logic_error("Test workspace requires a Qt application");
            fixture = new Fixture(QCoreApplication::instance());
        }
        return fixture->context;
    }
    static auto &workspace() { return context().workspace(); }
    static auto &searchSession() { return context().searchSession(); }
    static auto &presentation() { return context().presentation(); }
    static auto &searches() { return context().searches(); }
    static auto &db() { return static_cast<qualpf::infrastructure::SqlReferenceRepository &>(context().catalog().reader()); }
    static auto &peaks() { return context().peaks(); }
    static auto peakSnapshot() { return context().peakSnapshot(); }
    static void replacePeaks(ExperimentalPeaks p) { context().replacePeaks(std::move(p)); }
    static void load() { context().load(); }
    static auto &databases() { return context().databases(); }
    static auto activeDatabase() { return context().activeDatabase(); }
    static auto setDatabases(const QList<qualpf::infrastructure::DatabaseEntry> &d) {
        QList<qualpf::gui::DatabaseEntry> views;
        for (const auto &v : d) views.append({v.inUse,v.name,v.entries,v.contentType,v.path});
        return context().setDatabases(views);
    }
};
