#pragma once
#include "qualpf/application/state/presentation_workspace.h"
#include "gui/adapters/synchronous_search_service.h"
#include "gui/adapters/reference_database.h"
#include "qualpf/application/product_files.h"
#include "qualpf/application/platform_services.h"
#include "qualpf/application/update_service.h"
#include "qualpf/application/database_build.h"
#include <QPointer>
namespace qualpf::gui {
class WorkspaceContext {
  public:
    explicit WorkspaceContext(application::Workspace &, application::DatabaseCatalog &, application::ProductFiles &, application::PreferenceStore &, application::SearchSettingsProvider &, application::PlatformServices &, application::UpdateServiceProvider &, application::DatabaseBuildUseCases &,
                              std::shared_ptr<application::CandidateReaderFactory>);
    WorkspaceContext(const WorkspaceContext &) = delete;
    WorkspaceContext &operator=(const WorkspaceContext &) = delete;
    void load();
    Status setDatabases(const QList<DatabaseEntry> &);
    const QList<DatabaseEntry> &databases() const;
    const DatabaseEntry *activeDatabase() const;
    application::DatabaseBuildUseCases &databaseBuild() { return databaseBuild_; }
    application::UpdateServiceProvider &updates() { return updates_; }
    application::PlatformServices &platform() { return platform_; }
    application::PreferenceStore &preferences() { return preferences_; }
    application::SearchSettingsProvider &settings() { return settings_; }
    application::ProductFiles &files() { return files_; }
    application::DatabaseCatalog &catalog() { return catalog_; }
    void closeDatabases() { catalog_.close(); }
    SynchronousSearchService &searches();
    application::Workspace &workspace();
    application::ReferenceDatabase referenceDatabase();
    SessionView &searchSession();
    application::PresentationWorkspace &presentation();
    const ExperimentalPeaks &peaks();
    std::shared_ptr<const ExperimentalPeaks> peakSnapshot();
    void replacePeaks(ExperimentalPeaks);
    QString defaultDbDir();
    void setDefaultDbDir(const QString &);
    const std::shared_ptr<application::CandidateReaderFactory> &readerFactory() const { return factory_; }
    void bindPresenter(QObject *, std::function<Status()>);
    void unbindPresenter(const QObject *);
    Status refreshPresentation();

  private:
    Status openActiveDatabase();
    void scanAndRegisterDatabases(const QString &);
    void promptForDefaultDir();
    QList<DatabaseEntry> databases_;
    application::DatabaseCatalog &catalog_;
    application::ProductFiles &files_;
    application::PreferenceStore &preferences_;
    application::SearchSettingsProvider &settings_;
    application::PlatformServices &platform_;
    application::UpdateServiceProvider &updates_;
    application::DatabaseBuildUseCases &databaseBuild_;
    application::Workspace &workspace_;
    SessionView sessionView_;
    application::PresentationWorkspace presentation_;
    std::shared_ptr<application::CandidateReaderFactory> factory_;
    SynchronousSearchService searches_;
    QPointer<QObject> presenterOwner_;
    std::function<Status()> presenter_;
};
// Designer-created children inherit the binding from their QObject owner.
// There is deliberately no process-global fallback or worker access path.
void bindWorkspaceContext(QObject &, WorkspaceContext &);
WorkspaceContext &workspaceContext(const QObject *);
} // namespace qualpf::gui
