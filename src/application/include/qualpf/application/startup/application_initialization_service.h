#pragma once

#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/state/presentation_workspace.h"
#include "qualpf/crystallography/chemistry.h"
#include "qualpf/crystallography/symmetry.h"
#include <memory>

namespace qualpf::application {

// Application startup. Registries already have
// immutable process-lifetime storage. This service does not reload/copy them,
// reset an existing session, or initialize any native compatibility state.
class ApplicationInitializationService {
  public:
    void initialize(PresentationWorkspace &presentation, PresentationMode mode) const {
        presentation.configure(mode);
    }
    [[nodiscard]] std::unique_ptr<Workspace> createWorkspace() const {
        return std::make_unique<Workspace>();
    }
    [[nodiscard]] const crystallography::SpaceGroupRegistry &symmetry() const { return symmetry_; }
    [[nodiscard]] const crystallography::ScatteringRegistry &scattering() const { return scattering_; }
    [[nodiscard]] const crystallography::ChemistryRegistry &chemistry() const { return chemistry_; }

  private:
    crystallography::SpaceGroupRegistry symmetry_;
    crystallography::ScatteringRegistry scattering_;
    crystallography::ChemistryRegistry chemistry_;
};

} // namespace qualpf::application
