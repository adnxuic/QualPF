#pragma once
#include "tests/support/workspace_fixture.h"
#include "qualpf/application/startup/application_initialization_service.h"
#include "qualpf/application/startup/startup_options.h"
#include "gui/model/preprocessing_options.h"

namespace qualpf::legacy {
// Adapt only the old probe setup. Workflow assertions and inputs stay intact;
// the actual executable independently exercises main's production startup.
inline void initialize_product_presentation_for_probe(qualpf::application::StartupOptions *options, const char *, int, int *error) {
    using application::PresentationMode;
    application::ApplicationInitializationService startup;
    startup.initialize(AppState::presentation(), options->nogui ? PresentationMode::headless :
        options->autom ? PresentationMode::automatic_gui : PresentationMode::interactive_gui);
    *error = 0;
}
}
