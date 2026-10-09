#pragma once
#include "gui/state/workspace_context.h"
#include <optional>
qualpf::SearchParameters currentSearchParameters(qualpf::gui::WorkspaceContext &context);

#include "gui/search/search_request.h"
#include "gui/model/experimentalpeaks.h"

// Peak search + FOM-based database query, shared between the GUI's
// "Search > Match" action (MainWindow::onActionSearchMatchTriggered) and the
// --search command-line mode used when a data file is given without a GUI.

// Runs a peak search on the currently loaded pattern if no peaks exist yet.
// Returns the number of peaks found (0 if none, even after searching).
// On C++ preparation failure, optionally returns the structured error for the UI.
// A successful empty result has no error. The output is reset on every call.
int ensurePeaksFound(qualpf::gui::WorkspaceContext &context, std::optional<qualpf::Error> *failure = nullptr);

// Reads the application-owned snapshot; never reimports residuals from Fortran.
// Returns the number of peaks (0 if none).
int loadExperimentalPeaks(qualpf::gui::WorkspaceContext &context);

// Builds a qualpf::gui::SearchRequest configured for a peak-based Search & Match query
// using the given experimental peaks and the persisted Search Options settings
// (min FOM, weights, delta 2theta). Composition/restraints are left to the
// caller to add afterwards.
qualpf::gui::SearchRequest buildSearchMatchQuery(qualpf::gui::WorkspaceContext &context, const ExperimentalPeaks &ep);
