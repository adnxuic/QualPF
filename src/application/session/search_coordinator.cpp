#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/workflow.h"
#include "qualpf/application/pfsm.h"
#include <exception>
#include <limits>

namespace qualpf::application {
Result<SearchTask> SearchCoordinator::begin(ReferenceDatabase database, std::optional<WorkflowTarget> workflow) {
    if (std::this_thread::get_id() != owner_)
        return Result<SearchTask>{
            Error{ErrorCode::invalid_input, "search.thread", "Admission requires the owner thread"}};
    if (closing_)
        return Result<SearchTask>{Error{ErrorCode::closed_edit, "search", "Workspace is closing", "closing"}};
    if (active_)
        return Result<SearchTask>{
            Error{ErrorCode::invalid_input, "search", "A search is already running", "busy"}};
    if (issuedTasks_ == std::numeric_limits<std::uint64_t>::max())
        return Result<SearchTask>{Error{ErrorCode::exhausted_revision, "search.task_id",
                                        "Task identity exhausted", "task_id_exhausted"}};
    SearchTask task;
    task.source = session_.snapshot();
    task.algorithms = task.source->algorithms;
    task.database = std::move(database);
    task.workflow = workflow;
    // Construct owned values before occupying the slot or consuming an ID.
    stop_ = std::stop_source{};
    task.id = issuedTasks_ + 1;
    task.context = {stop_.get_token(), task.algorithms->spec.method() == SearchMethod::pfsm ?
        NumericalPolicy::explicit_float64 : NumericalPolicy::legacy_mixed_precision, "product-search"};
    active_ = task;
    activity_ = workflow ? std::optional<WorkflowActivity>{{*workflow, WorkflowStepStatus::running, {}}} : std::nullopt;
    issuedTasks_ = task.id;
    databaseVersion_ = task.database.selectionVersion;
    error_.reset();
    cancelled_ = false;
    return Result<SearchTask>{std::move(task)};
}
Status Workspace::configureAlgorithms(const AlgorithmSpec &spec, AlgorithmModules overrides) {
    if (coordinator_.closing())
        return Status{Error{ErrorCode::closed_edit, "algorithms", "Workspace is closing", "closing"}};
    if (coordinator_.running())
        return Status{Error{ErrorCode::invalid_input, "algorithms", "Finish the running Search first", "search_in_progress"}};
    if (session_.snapshot()->editing)
        return Status{Error{ErrorCode::invalid_input, "session.edit", "Finish the preprocessing edit first"}};
    auto resolved = resolveAlgorithms(spec, std::move(overrides));
    if (!resolved) return Status{resolved.error()};
    return session_.changeAlgorithms(std::move(resolved).value());
}
Status Workspace::revisePfsmRir(ProductHandle source, const std::string &phaseId,
                               std::optional<PfsmRirRevision> revision) {
    if (coordinator_.running() || coordinator_.closing() || session_.snapshot()->editing)
        return Status{Error{ErrorCode::closed_edit, "quantification.rir",
            "Finish the active operation before editing RIR", "workspace_unavailable"}};
    if (source != session_.snapshot())
        return Status{Error{ErrorCode::stale_snapshot, "quantification.source",
            "The analysis changed while editing RIR", "stale_quantification"}};
    auto prepared = preparePfsmRirRevision(source, phaseId, std::move(revision));
    if (!prepared) return Status{prepared.error()};
    if (prepared.value().pfsm == source->pfsm) return Status{std::monostate{}};
    prepared.value().workflow.completed_target = WorkflowStep::rir;
    return session_.commit(std::move(source), std::move(prepared).value());
}
Result<WorkflowRollback> Workspace::prepareRollback(std::uint64_t id) const {
    if (coordinator_.running() || coordinator_.closing())
        return Result<WorkflowRollback>{Error{ErrorCode::closed_edit,"workflow.undo","Finish the active operation before undo","workspace_unavailable"}};
    return session_.prepareRollback(id);
}
Status Workspace::commitRollback(const WorkflowRollback &rollback) {
    if (coordinator_.running() || coordinator_.closing())
        return Status{Error{ErrorCode::closed_edit,"workflow.undo","Finish the active operation before undo","workspace_unavailable"}};
    return session_.commitRollback(rollback);
}
Status Workspace::applySettings(const WorkflowSettingsChange &change) {
    if (coordinator_.running() || coordinator_.closing() || session_.snapshot()->editing)
        return Status{Error{ErrorCode::closed_edit,"settings","Finish the active operation before changing project settings","workspace_unavailable"}};
    if (change.source!=session_.snapshot() || !change.next.projectSettings)
        return Status{Error{ErrorCode::stale_snapshot,"settings","The project changed; reopen Options"}};
    auto verified=prepareSettingsChange(change.source,*change.next.projectSettings);
    if (!verified) return Status{verified.error()};
    return session_.commit(change.source,std::move(verified).value().next);
}
bool SearchCoordinator::reportWorkflowProgress(std::uint64_t task, std::string detail) {
    if(std::this_thread::get_id()!=owner_ || closing_ || cancelled() || !active_ || active_->id!=task || !activity_) return false;
    activity_->detail=std::move(detail);
    return true;
}
void SearchCoordinator::cancel() noexcept {
    cancelled_ = true;
    stop_.request_stop();
}
void SearchCoordinator::close() noexcept {
    closing_ = true;
    cancel();
}
Result<PreparedSearchResult> SearchCoordinator::complete(std::uint64_t task,
                                                         Result<PreparedSearchResult> result,
                                                         SearchPublication publication) {
    if (std::this_thread::get_id() != owner_)
        return Result<PreparedSearchResult>{
            Error{ErrorCode::invalid_input, "search.thread", "Completion requires the owner thread"}};
    // An old delivery cannot retire, cancel or overwrite the active newer task.
    if (!active_ || active_->id != task)
        return Result<PreparedSearchResult>{
            Error{ErrorCode::stale_snapshot, "search.task_id", "Obsolete search task", "old_task"}};
    const auto input = *active_;
    try {
        if (closing_)
            error_ = Error{ErrorCode::closed_edit, "search", "Workspace is closing", "closing"};
        else if (stop_.stop_requested()) {
            cancelled_ = true;
            error_ = Error{ErrorCode::cancelled, "search", "Search canceled"};
        } else if (!result) {
            error_ = result.error();
            cancelled_ = error_->code == ErrorCode::cancelled;
        } else if (result.value().taskId != task || result.value().source != input.source ||
                   session_.snapshot() != input.source ||
                   session_.snapshot()->revision != input.source->revision)
            error_ =
                Error{ErrorCode::stale_snapshot, "search.source", "Search input changed during calculation"};
        else if (result.value().databaseVersion != input.database.selectionVersion ||
                 databaseVersion_ != input.database.selectionVersion)
            error_ = Error{ErrorCode::stale_snapshot, "search.database",
                           "Database selection changed during calculation"};
        else if (result.value().next.algorithms != input.algorithms)
            error_ = Error{ErrorCode::stale_snapshot, "search.provenance",
                           "Published algorithms disagree with the frozen task", "algorithm_provenance_mismatch"};
        else if (publication == SearchPublication::commit && result.value().changed) {
            if (input.workflow && result.value().next.projectSettings)
                result.value().next.workflow.last_attempt = WorkflowActivity{*input.workflow,WorkflowStepStatus::completed,{}};
            auto status = session_.commit(input.source, result.value().next);
            if (!status)
                error_ = status.error();
        }
    } catch (const std::exception &exception) {
        error_ =
            Error{ErrorCode::numerical_failure, "search.commit", exception.what(), "unexpected_exception"};
    } catch (...) {
        error_ = Error{ErrorCode::numerical_failure, "search.commit", "Unexpected publication failure",
                       "unexpected_exception"};
    }
    active_.reset();
    if (input.workflow) {
        activity_ = WorkflowActivity{*input.workflow,error_ ?
            (error_->code == ErrorCode::cancelled ? WorkflowStepStatus::cancelled : WorkflowStepStatus::failed) :
            WorkflowStepStatus::completed,error_ ? error_->message : std::string{}};
        if (error_ && !closing_ && session_.snapshot() == input.source && input.source->projectSettings &&
            error_->code != ErrorCode::stale_snapshot && publication == SearchPublication::commit) {
            auto next=*input.source;next.workflow.last_attempt=activity_;
            auto recorded=session_.commit(input.source,std::move(next));
            if(!recorded) error_=recorded.error();
        }
    }
    if (error_)
        return Result<PreparedSearchResult>{*error_};
    return result;
}
} // namespace qualpf::application
