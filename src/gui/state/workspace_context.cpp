#include "gui/state/workspace_context.h"
#include <limits>
#include <stdexcept>


#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

namespace qualpf::gui {
WorkspaceContext::WorkspaceContext(application::Workspace &workspace, application::DatabaseCatalog &catalog, application::ProductFiles &files, application::PreferenceStore &preferences, application::SearchSettingsProvider &settings, application::PlatformServices &platform, application::UpdateServiceProvider &updates, application::DatabaseBuildUseCases &databaseBuild,
                                   std::shared_ptr<application::CandidateReaderFactory> factory)
    : catalog_(catalog), files_(files), preferences_(preferences), settings_(settings), platform_(platform), updates_(updates), databaseBuild_(databaseBuild), workspace_(workspace), sessionView_(workspace.session()), factory_(std::move(factory)),
      searches_(workspace.searches(), catalog.reader(), [this] { return referenceDatabase(); }) {
    if (!factory_) throw std::invalid_argument("Workspace requires a candidate reader factory");
}
SynchronousSearchService &WorkspaceContext::searches() { return searches_; }
application::Workspace &WorkspaceContext::workspace() { return workspace_; }
application::ReferenceDatabase WorkspaceContext::referenceDatabase() {
    return catalog_.reference();
}
SessionView &WorkspaceContext::searchSession() { return sessionView_; }
application::PresentationWorkspace &WorkspaceContext::presentation() { return presentation_; }
void WorkspaceContext::load() {
    catalog_.load(); databases_ = databaseViews(catalog_.entries());
    if (!catalog_.directoryExists(defaultDbDir().toStdString()) && databases_.isEmpty()) promptForDefaultDir();
    (void)openActiveDatabase();
}
Status WorkspaceContext::setDatabases(const QList<DatabaseEntry> &databases) {
    auto status = catalog_.select(databaseValues(databases));
    databases_ = databaseViews(catalog_.entries());
    return status;
}

const QList<DatabaseEntry> &WorkspaceContext::databases() const { return databases_; }

const DatabaseEntry *WorkspaceContext::activeDatabase() const {
    for (const DatabaseEntry &db : databases_) {
        if (db.inUse)
            return &db;
    }
    return nullptr;
}


const ExperimentalPeaks &WorkspaceContext::peaks() { return sessionView_.snapshot()->peaks; }

std::shared_ptr<const ExperimentalPeaks> WorkspaceContext::peakSnapshot() {
    auto state = sessionView_.snapshot();
    return std::shared_ptr<const ExperimentalPeaks>(state, &state->peaks);
}

void WorkspaceContext::replacePeaks(ExperimentalPeaks peaks) { sessionView_.replacePeaks(std::move(peaks)); }

QString WorkspaceContext::defaultDbDir() { return QString::fromStdString(catalog_.defaultDirectory()); }
void WorkspaceContext::setDefaultDbDir(const QString &dir) {
    catalog_.setDefaultDirectory(dir.toStdString());
}
void WorkspaceContext::scanAndRegisterDatabases(const QString &dir) {
    catalog_.discover(dir.toStdString()); databases_ = databaseViews(catalog_.entries());
}
void WorkspaceContext::promptForDefaultDir() {
    if (!presentation_.modalErrorsAllowed()) {
        qWarning("No crystallographic database found in the configured default directory");
        return;
    }
    QMessageBox msgBox;
    msgBox.setWindowTitle(QStringLiteral("No database found"));
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setText(QString("No crystallographic databases were found.\n\n"
                           "The default folder does not exist:\n  %1")
                       .arg(defaultDbDir()));
    msgBox.setInformativeText("You can choose a different default folder, or manage databases "
                              "later via Search > Manage Databases.");

    QPushButton *chooseBtn = msgBox.addButton("Choose folder...", QMessageBox::ActionRole);
    msgBox.addButton("Manage Databases later", QMessageBox::RejectRole);

    msgBox.exec();

    if (msgBox.clickedButton() == chooseBtn) {
        const QString dir = QFileDialog::getExistingDirectory(
            nullptr, QStringLiteral("Select QualXDB folder"), QDir::homePath());
        if (!dir.isEmpty()) {
            setDefaultDbDir(dir);
            scanAndRegisterDatabases(dir);
        }
    }
}

Status WorkspaceContext::openActiveDatabase() { return catalog_.openActive(); }

void WorkspaceContext::bindPresenter(QObject *owner, std::function<Status()> refresh) {
    presenterOwner_ = owner;
    presenter_ = std::move(refresh);
}
void WorkspaceContext::unbindPresenter(const QObject *owner) {
    if (presenterOwner_ == owner) {
        presenter_ = {};
        presenterOwner_.clear();
    }
}
Status WorkspaceContext::refreshPresentation() {
    if (!presentation_.redrawEnabled())
        return Status{std::monostate{}};
    if (!presenterOwner_ || !presenter_)
        return Status{Error{ErrorCode::missing_data, "plot.presenter",
                            "No GUI presentation binding is available", "missing_presenter"}};
    if (workspace_.searches().closing())
        return Status{Error{ErrorCode::cancelled, "plot.presenter", "Workspace is closing", "closing"}};
    return presenter_();
}
void bindWorkspaceContext(QObject &owner, WorkspaceContext &context) {
    owner.setProperty("_qualpfWorkspace",
                      QVariant::fromValue<qulonglong>(reinterpret_cast<quintptr>(&context)));
}
WorkspaceContext &workspaceContext(const QObject *object) {
    for (auto *owner = object; owner; owner = owner->parent()) {
        const auto binding = owner->property("_qualpfWorkspace");
        if (binding.isValid())
            return *reinterpret_cast<WorkspaceContext *>(static_cast<quintptr>(binding.toULongLong()));
    }
    throw std::logic_error("GUI object has no workspace binding");
}
} // namespace qualpf::gui
