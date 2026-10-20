#pragma once
#include "qualpf/application/workflow_settings.h"
#include "qualpf/application/algorithms.h"
namespace qualpf::application {
enum class WorkflowStep { range, smoothing, kalpha2, background, initial_peaks, fwhm, peaks, search, phases, rir };
enum class WorkflowStepStatus { not_started, running, completed, skipped, failed, cancelled, invalidated };
struct WorkflowTarget { WorkflowId workflow; WorkflowStep step; };
struct WorkflowActivity {
    WorkflowTarget target;
    WorkflowStepStatus status = WorkflowStepStatus::running;
    std::string detail;
};
const char *workflowStepName(WorkflowStep);
struct WorkflowRecord {
    std::uint64_t id = 0;
    WorkflowId workflow = WorkflowId::preprocessing;
    WorkflowStep step = WorkflowStep::background;
    WorkflowStepStatus status = WorkflowStepStatus::completed;
    bool manual = false;
    std::vector<std::uint64_t> dependencies;
    WorkflowProfile parameters;
    MetadataObject actual_parameters;
    AlgorithmSelections algorithms;
    std::shared_ptr<const Pattern> input;
    std::string detail;
    MetadataObject extensions;
};
struct WorkflowState {
    WorkflowId active = WorkflowId::preprocessing;
    std::shared_ptr<const Pattern> imported_pattern;
    std::vector<WorkflowRecord> records;
    std::uint64_t next_id = 1;
    std::optional<WorkflowActivity> last_attempt;
    // Evaluation annotation, consumed before canonical publication.
    std::optional<WorkflowStep> completed_target;
    bool configuration_transition = false;
};
} // namespace qualpf::application
