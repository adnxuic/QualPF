#pragma once

#include <array>
#include <cstddef>

namespace qualpf::application {

enum class PresentationMode { interactive_gui, automatic_gui, headless };
enum class ErrorPresentation { interactive, automatic, headless };
enum class Visibility : int { disabled = -1, hidden = 0, visible = 1 };
enum class PlotLayer : std::size_t {
    background, calculated, difference, cumulative, peaks, axis,
    background_points, intervals, canvas, legend, unindexed_peaks, smoothing, absences
};

// Application presentation state only. No pattern, accepted cards, atoms, cell,
// refinement, or reflection arrays: those are not the empty display shell.
class PresentationWorkspace {
  public:
    struct Snapshot {
        Visibility phase = Visibility::visible;
        Visibility observed = Visibility::visible;
        std::array<Visibility, 13> layers{};
        // The global display shell has no reflection-data producer. Card
        // overlays continue to be rendered from the canonical snapshot by the Qt view.
        static constexpr std::size_t reflection_count = 0;
    };

    void configure(PresentationMode mode) noexcept { mode_ = mode; configured_ = true; }
    bool configured() const noexcept { return configured_; }
    PresentationMode mode() const noexcept { return mode_; }
    bool redrawEnabled() const noexcept { return mode_ != PresentationMode::headless; }
    bool callbacksAllowed() const noexcept { return redrawEnabled(); }
    ErrorPresentation errorPresentation() const noexcept {
        if (mode_ == PresentationMode::headless) return ErrorPresentation::headless;
        return mode_ == PresentationMode::interactive_gui ? ErrorPresentation::interactive
                                                         : ErrorPresentation::automatic;
    }
    bool modalErrorsAllowed() const noexcept { return errorPresentation() == ErrorPresentation::interactive; }
    const Snapshot &snapshot() const noexcept { return state_; }
    void applyPlotAction(int action) noexcept;
    void setVisibility(PlotLayer layer, Visibility visibility) noexcept;

  private:
    PresentationMode mode_ = PresentationMode::headless;
    bool configured_ = false;
    Snapshot state_;
};

} // namespace qualpf::application
