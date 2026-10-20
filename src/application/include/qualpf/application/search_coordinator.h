#pragma once
#include "qualpf/application/search_engine.h"
#include "qualpf/application/pfsm.h"
#include "qualpf/application/project_document.h"
#include <stop_token>
#include <thread>

namespace qualpf::application {
struct ReferenceDatabase {
    std::string basePath;
    std::uint64_t selectionVersion = 0;
    std::string identity;
};
struct SearchTask {
    std::uint64_t id = 0;
    ProductHandle source;
    ReferenceDatabase database;
    ExecutionContext context;
    AlgorithmHandle algorithms;
    std::optional<WorkflowTarget> workflow;
    std::function<void(std::string)> workflowProgress;
};
class ReferenceReadGuard {
  public:
    virtual ~ReferenceReadGuard() = default;
};
class CandidateReaderFactory {
  public:
    virtual ~CandidateReaderFactory() = default;
    virtual Result<std::shared_ptr<const ReferenceReadGuard>> protect(const ReferenceDatabase &) {
        return Result<std::shared_ptr<const ReferenceReadGuard>>{std::shared_ptr<const ReferenceReadGuard>{}};
    }
    virtual Result<std::unique_ptr<CandidateReader>> open(const ReferenceDatabase &, std::uint64_t task,
                                                          const ExecutionContext &) = 0;
};
enum class SearchPublication { prepare_only, commit };

// An owner-thread actor. Workers receive SearchTask values and tokens, never
// this coordinator or its Session. Admission and publication belong here.
class SearchCoordinator {
  public:
    explicit SearchCoordinator(ProductSearchSession &session, std::uint64_t issuedTasks = 0)
        : session_(session), issuedTasks_(issuedTasks) {}
    SearchCoordinator(const SearchCoordinator &) = delete;
    SearchCoordinator &operator=(const SearchCoordinator &) = delete;
    Result<SearchTask> begin(ReferenceDatabase = {}, std::optional<WorkflowTarget> = {});
    const std::optional<WorkflowActivity> &activity() const { return activity_; }
    bool reportWorkflowProgress(std::uint64_t task, std::string detail);
    void cancel() noexcept;
    void close() noexcept;
    void selectDatabase(std::uint64_t version) noexcept { databaseVersion_ = version; }
    bool running() const noexcept { return active_.has_value(); }
    bool closing() const noexcept { return closing_; }
    bool cancelled() const noexcept { return cancelled_ || stop_.stop_requested(); }
    ProductHandle source() const { return active_ ? active_->source : ProductHandle{}; }
    const std::optional<Error> &error() const { return error_; }
    Result<PreparedSearchResult> complete(std::uint64_t task, Result<PreparedSearchResult>,
                                          SearchPublication = SearchPublication::commit);

  private:
    ProductSearchSession &session_;
    std::thread::id owner_ = std::this_thread::get_id();
    std::uint64_t issuedTasks_ = 0, databaseVersion_ = 0;
    std::optional<SearchTask> active_;
    std::optional<WorkflowActivity> activity_;
    std::stop_source stop_;
    std::optional<Error> error_;
    bool closing_ = false, cancelled_ = false;
};

// Composition owns this canonical workspace, one coordinator and its Session.
// GUI views and controllers reference it; neither owns another product state.
class Workspace {
  public:
    explicit Workspace(SearchBackend backend = SearchBackend::cpp,
                        AlgorithmHandle algorithms = defaultAlgorithms(), DefaultSettingsStore *defaults = nullptr)
        : session_(backend, std::move(algorithms), defaults), coordinator_(session_) {}
    ProductSearchSession &session() { return session_; }
    SearchCoordinator &searches() { return coordinator_; }
    Status configureAlgorithms(const AlgorithmSpec &, AlgorithmModules overrides = {});
    Result<struct WorkflowRollback> prepareRollback(std::uint64_t) const;
    Status commitRollback(const struct WorkflowRollback &);
    Status applySettings(const struct WorkflowSettingsChange &);
    Status revisePfsmRir(ProductHandle, const std::string &phaseId,
                         std::optional<PfsmRirRevision>);
    std::shared_ptr<const ProjectViewState> projectView() const { return projectView_; }
    std::uint64_t projectViewRevision() const { return viewRevision_; }
    Status updateProjectView(ProjectViewState);
    Result<ProjectSaveSnapshot> projectSnapshot() const;
    Status restoreProject(const ProjectSaveSnapshot &, ProjectDocument);
    Status markProjectSaved(const ProjectSaveSnapshot &, ProjectIdentity);
    bool projectModified() const;
    void resetProjectPresentation();

  private:
    ProductSearchSession session_;
    SearchCoordinator coordinator_;
    std::shared_ptr<const ProjectViewState> projectView_ = std::make_shared<const ProjectViewState>();
    ProjectIdentity projectIdentity_;
    std::shared_ptr<const ProjectExtensions> projectExtensions_;
    ProductHandle savedProduct_, projectSource_;
    std::uint64_t viewRevision_ = 0, savedViewRevision_ = 0;
    std::thread::id owner_ = std::this_thread::get_id();
};
} // namespace qualpf::application
