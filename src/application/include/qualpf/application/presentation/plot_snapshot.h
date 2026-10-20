#pragma once

#include "qualpf/application/state/presentation_workspace.h"
#include "qualpf/domain/pattern.h"
#include "qualpf/domain/result.h"
#include <optional>
#include <string>
#include <vector>

namespace qualpf::application { struct PfsmPlot; struct ProductState; }

namespace qualpf::application::presentation {

// Semantic identities match the established view layers. No pens/widgets/ABI
// pointers live here; the view resolves style keys using its existing settings.
enum class SeriesKind {
    observed,
    calculated,
    background,
    background_points,
    difference,
    cumulative,
    reflections,
    peaks,
    smoothing,
    unindexed_peaks,
    systematic_absences,
    selected_reflections,
    selected_peaks,
    intervals,
    profile_curves,
    accepted_card,
    search_residual,
    preprocessing_preview
};
enum class Precision { binary32_transport, binary64 };
enum class Abscissa { two_theta, d_spacing, inverse_d, inverse_d_squared };
enum class UpdatePolicy { replace_canvas, upsert, erase };
enum class PreviewGeometry { curve, peak_impulses };

struct PlotSeries {
    std::string identity;
    SeriesKind kind = SeriesKind::observed;
    std::string style_identity;
    std::string label;
    std::vector<double> x, y;
    double wavelength = 0;
    Visibility visibility = Visibility::visible;
    Precision precision = Precision::binary64;
    UpdatePolicy update = UpdatePolicy::upsert;
    // Private preview geometry is fixed by the existing editor request; ordinary
    // series continue to resolve their shape through the view's style identity.
    PreviewGeometry preview_geometry = PreviewGeometry::curve;
};
struct PlotSnapshot {
    std::vector<PlotSeries> series;
    // Accepted cards survive a native canvas replacement in the existing view.
    std::vector<PlotSeries> overlays;
    Abscissa abscissa = Abscissa::two_theta;
    bool reverse_axis = false;
};
enum class CommandKind { rescale, replace_canvas, upsert, erase, redraw };
struct PlotCommand {
    CommandKind kind = CommandKind::redraw;
    int rescale = 1;
    std::optional<PlotSeries> series;
};
struct PlotUpdate {
    PlotSnapshot snapshot;
    std::vector<PlotCommand> commands;
};

// All buffers are owning C++ values. Normal producers copy their Pattern and
// ProductSearchSession values. Retained editors may supply an explicit private preview;
// it is never imported into the PresentationWorkspace or ProductSearchSession here.
struct PatternLayerInput {
    Pattern pattern;
    std::string display_name;
    Visibility observed_visibility = Visibility::visible;
    bool background_enabled = false;
    std::size_t first = 0, count = 0; // Calculation span, zero based.
    std::optional<std::vector<double>> smoothing_preview;
    std::vector<double> background_points_x, background_points_y;
};
struct PeakLayerInput {
    std::vector<double> x, current_intensity;
    std::vector<int> unindexed;
    double wavelength = 0;
};
struct CardOverlayInput {
    std::string id, chemical_name, mineral_name;
    std::vector<double> two_theta, d_spacing;
    double display_wavelength = 0;
    Visibility visibility = Visibility::visible;
};
struct PlotInput {
    std::vector<PatternLayerInput> patterns;
    PeakLayerInput peaks;
    std::vector<CardOverlayInput> cards;
    PresentationWorkspace::Snapshot presentation;
    PresentationMode mode = PresentationMode::headless;
};
struct PlotRequest {
    int rescale = 1;
    Abscissa abscissa = Abscissa::two_theta;
};

// Frozen native empty-shell frame (binary32 narrowing, full observed spans,
// calculated/background span, nearest observed samples for native peak markers).
Result<PlotUpdate> project_compatibility_frame(const PlotInput &, PlotRequest = {});
// Existing direct C++ refresh: current residual intensities, then observed,
// aligned background and points. No native peak-intensity sampling or clipping.
Result<PlotUpdate> project_session_update(const PlotInput &, PlotRequest = {});
bool can_subtract_background(const Pattern &) noexcept;
bool can_subtract_background(const ProductState &) noexcept;
Result<PlotUpdate> project_pfsm_update(const PfsmPlot &, const PresentationWorkspace::Snapshot &, PlotRequest = {});
Result<PlotSeries> project_card_overlay(const CardOverlayInput &, Abscissa);
Result<PlotSeries> project_preview(PlotSeries, Abscissa, PreviewGeometry = PreviewGeometry::curve);
Status project_coordinates(PlotSeries &, Abscissa);

// Synchronous value dispatch; a producer completes projection/validation before
// the first sink call. The sink may retain copies. No GUI event processing,
// native call, session commit or worker-to-GUI dispatch is hidden here.
class PlotSink {
  public:
    virtual ~PlotSink() = default;
    virtual void accept(const PlotCommand &) = 0;
};
class PlotDispatcher {
  public:
    void dispatch(const PlotUpdate &update, PlotSink &sink) const {
        for (const auto &command : update.commands)
            sink.accept(command);
    }
};
class RecordingPlotSink final : public PlotSink {
  public:
    void accept(const PlotCommand &command) override { commands.push_back(command); }
    std::vector<PlotCommand> commands;
};

} // namespace qualpf::application::presentation
