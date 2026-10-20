#pragma once
#include "qualpf/application/preprocessing_configuration.h"
#include "qualpf/application/pfsm_settings.h"
#include "qualpf/domain/search.h"
#include <array>
namespace qualpf::application {
enum class WorkflowId { preprocessing, csm, pfsm, peak_search };
inline constexpr std::array workflows{WorkflowId::preprocessing, WorkflowId::csm, WorkflowId::pfsm, WorkflowId::peak_search};
const char *workflowName(WorkflowId);
struct CsmSettings {
    double minimum_fom = .35, weight_position = .5, weight_intensity = .5, weight_phases = .5;
    double delta = .08;
    bool automatic_delta = true, residual = true, strongest = true, exclude_deleted = true;
    int maximum_entries = 3000;
};
// Configuration only: measurements and resolved automatic values never belong here.
struct WorkflowProfile {
    preprocessing::BackgroundOptions background;
    InitialPeakConfiguration initial_peaks;
    FwhmConfiguration fwhm;
    preprocessing::PeakOptions peaks;
    PeakSelection selection;
    preprocessing::SmoothingOptions smoothing;
    std::optional<AngularRange> range;
    CsmSettings csm;
    PfsmSettings pfsm;
    MetadataObject extensions;
};
struct ProjectSettings {
    std::array<WorkflowProfile, 4> profiles;
    MetadataObject extensions;
    ProjectSettings();
    WorkflowProfile &profile(WorkflowId id) { return profiles.at(static_cast<std::size_t>(id)); }
    const WorkflowProfile &profile(WorkflowId id) const { return profiles.at(static_cast<std::size_t>(id)); }
};
Status validateProjectSettings(const ProjectSettings &);
MetadataObject csmSettingValues(const CsmSettings &);
CsmSettings csmSettings(const MetadataObject &);
// This port stores user defaults, never live project state.
class DefaultSettingsStore {
  public:
    virtual ~DefaultSettingsStore() = default;
    virtual Result<ProjectSettings> load() const = 0;
    virtual Status save(const ProjectSettings &) = 0;
};
} // namespace qualpf::application
