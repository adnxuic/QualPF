#pragma once
#include "qualpf/application/product_session.h"
namespace qualpf::application {
struct WorkflowPreparation {
    ProductHandle source;
    ProductState next;
    WorkflowStep target;
    std::vector<WorkflowStep> required, reused;
};
Result<WorkflowPreparation> prepareWorkflow(ProductHandle, WorkflowId, WorkflowStep);
struct WorkflowCheckpoint {
    std::uint64_t record = 0;
    WorkflowStep step = WorkflowStep::background;
    ProductHandle before;
};
struct WorkflowRollback {
    ProductHandle source;
    std::vector<std::uint64_t> affected;
    ProductState next;
};
// Called inside the canonical transaction before publication; no live mutation.
Result<std::vector<WorkflowCheckpoint>> recordWorkflowTransition(ProductHandle, ProductState &);
Result<WorkflowRollback> prepareRollback(ProductHandle, std::span<const WorkflowCheckpoint>, std::uint64_t);
const WorkflowRecord *currentWorkflowRecord(const WorkflowState &, WorkflowStep);
std::vector<std::uint64_t> workflowDependents(const WorkflowState &, std::uint64_t);
bool reusableWorkflowStep(const ProductState &, WorkflowId, WorkflowStep);
Status validateWorkflowState(const WorkflowState &);
struct WorkflowSettingsChange {
    ProductHandle source;
    ProductState next;
    std::vector<std::uint64_t> affected;
};
Result<WorkflowSettingsChange> prepareSettingsChange(ProductHandle, const ProjectSettings &);
} // namespace qualpf::application
