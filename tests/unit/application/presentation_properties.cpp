#include "qualpf/application/presentation/plot_snapshot.h"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::application::presentation;
namespace {
int checks = 0;
void require(bool value, const char *reason) {
    ++checks;
    if (!value)
        throw std::runtime_error(reason);
}
PlotInput input() {
    PlotInput in;
    in.mode = PresentationMode::automatic_gui;
    PatternLayerInput p;
    p.pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{12}, TwoThetaDegrees{14}};
    p.pattern.observed = {-0., 7., -3.};
    p.pattern.radiation.lines = {{WavelengthAngstrom{1.5}, 1}};
    p.count = 3;
    p.display_name = "measured";
    in.patterns.push_back(p);
    return in;
}
} // namespace
int main() {
    try {
        auto in = input();
        const auto untouched = in.patterns[0].pattern.observed;
        auto frame = project_compatibility_frame(in);
        require(bool(frame), "observed frame");
        require(frame.value().commands.size() == 4, "rescale/replace/observed/draw order");
        require(frame.value().commands[0].kind == CommandKind::rescale &&
                    frame.value().commands[1].kind == CommandKind::replace_canvas &&
                    frame.value().commands.back().kind == CommandKind::redraw,
                "ordered commands");
        require(frame.value().snapshot.series.size() == 1, "empty-reflection shell emits only observed");
        require(std::signbit(frame.value().snapshot.series[0].y[0]), "signed zero preserved");
        require(in.patterns[0].pattern.observed == untouched, "input immutable");
        in.peaks.x = {11};
        in.peaks.current_intensity = {-2};
        in.peaks.wavelength = 1.5;
        in.presentation.layers[std::size_t(PlotLayer::peaks)] = Visibility::visible;
        frame = project_compatibility_frame(in);
        require(bool(frame), "peak sampling frame");
        require(frame.value().snapshot.series.back().y == std::vector<double>{7}, "upper nearest tie");
        auto update = project_session_update(in);
        require(bool(update), "session refresh");
        require(update.value().snapshot.series.front().y == std::vector<double>{-2},
                "negative residual remains exact");
        require(update.value().snapshot.series.front().precision == Precision::binary64,
                "Session stays binary64");
        require(update.value().snapshot.series[2].update == UpdatePolicy::erase,
                "absent background clears targeted layer");
        const auto native = frame.value();
        in.mode = PresentationMode::headless;
        require(project_compatibility_frame(in).value().commands.empty(),
                "headless emits zero native-frame commands");
        require(project_session_update(in).value().commands.empty(), "headless emits zero session commands");
        in.mode = PresentationMode::automatic_gui;
        in.presentation.observed = Visibility::hidden;
        in.patterns[0].observed_visibility = Visibility::hidden;
        frame = project_compatibility_frame(in);
        require(frame.value().snapshot.series.size() == 1, "hidden observed leaves peak sampling");
        RecordingPlotSink sink;
        PlotDispatcher{}.dispatch(native, sink);
        require(sink.commands.size() == native.commands.size(), "sink ordered owning copy");
        sink.commands[2].series->y[1] = 999;
        require(native.commands[2].series->y[1] == 7, "sink cannot mutate producer snapshot");
        in = input();
        in.patterns[0].pattern.observed[0] = std::numeric_limits<double>::infinity();
        require(!project_compatibility_frame(in), "nonfinite input rejected before dispatch");
        in = input();
        in.patterns[0].first = 4;
        require(!project_compatibility_frame(in), "invalid span rejected");
        in = input();
        in.presentation.layers[std::size_t(PlotLayer::cumulative)] = Visibility::visible;
        require(!project_compatibility_frame(in),
                "latent undefined cumulative requires bounded future contract");
        in = input();
        in.patterns[0].pattern.background = SampledSignal{{2, 3, 4}, SampleWindow{0}};
        in.patterns[0].background_enabled = true;
        in.patterns[0].pattern.calculated = SampledSignal{{1, 2, 3}, SampleWindow{0}};
        in.presentation.layers[std::size_t(PlotLayer::calculated)] = Visibility::visible;
        in.presentation.layers[std::size_t(PlotLayer::background)] = Visibility::visible;
        in.presentation.layers[std::size_t(PlotLayer::difference)] = Visibility::visible;
        frame = project_compatibility_frame(in);
        require(bool(frame), "calculated/background/difference explicit input");
        const auto &s = frame.value().snapshot.series;
        require(s.size() == 4 && s[1].y == std::vector<double>({3, 5, 7}) &&
                    s[3].y == std::vector<double>({-3, 2, -10}),
                "sum then subtraction contract");
        in.patterns[0].pattern.processing.background_subtracted = true;
        frame = project_compatibility_frame(in);
        require(frame.value().snapshot.series.size() == 3, "subtracted flag suppresses background only");
        in = input();
        in.patterns[0].smoothing_preview = std::vector<double>{1, 2, 3};
        in.presentation.layers[std::size_t(PlotLayer::smoothing)] = Visibility::visible;
        frame = project_compatibility_frame(in);
        require(frame.value().snapshot.series.back().y == std::vector<double>({1, 2, 3}),
                "preview projects supplied values without smoothing math");
        PlotSeries signal;
        signal.x = {30, 60, 90};
        signal.y = {5, 9, 2};
        signal.wavelength = 1.5;
        require(bool(project_coordinates(signal, Abscissa::d_spacing)), "d projection");
        require(signal.y == std::vector<double>({2, 9, 5}), "d reverses paired values");
        require(std::abs(signal.x[1] - 1.5) < 1e-14, "independent sin(30) identity");
        PlotSeries bad;
        bad.x = {0};
        bad.y = {1};
        bad.wavelength = 1.5;
        require(!project_coordinates(bad, Abscissa::d_spacing) && bad.x[0] == 0 && bad.y[0] == 1,
                "coordinate failure atomic");
        in = input();
        in.peaks.x = {11};
        require(!project_session_update(in), "missing residual heights rejected before dispatch");
        in = input();
        in.patterns[0].background_points_x = {11};
        require(!project_session_update(in), "missing background point heights rejected");
        in = input();
        in.patterns[0].pattern.background = SampledSignal{{2}, SampleWindow{1}};
        in.presentation.layers[std::size_t(PlotLayer::background)] = Visibility::visible;
        require(!project_compatibility_frame(in), "partial native scratch span fails explicitly");
        require(!project_coordinates(signal, static_cast<Abscissa>(99)), "invalid axis rejected");
        auto preview = project_preview(signal, Abscissa::two_theta, PreviewGeometry::peak_impulses);
        require(preview && preview.value().preview_geometry == PreviewGeometry::peak_impulses,
                "private peak/manual preview retains impulse geometry");
        require(project_preview(signal, Abscissa::two_theta).value().preview_geometry ==
                    PreviewGeometry::curve,
                "private signal preview retains curve geometry");
        CardOverlayInput card;
        card.id = "card1";
        card.two_theta = {40, 20};
        card.d_spacing = {2, 4};
        card.chemical_name = "Oxide";
        card.mineral_name = "Mineral";
        auto overlay = project_card_overlay(card, Abscissa::d_spacing);
        require(bool(overlay), "direct card projection");
        require(overlay.value().x == std::vector<double>({2, 4}) && overlay.value().y.empty(),
                "sorted card sticks use supplied d values");
        require(overlay.value().label == "Oxide [Mineral]" &&
                    overlay.value().style_identity == "accepted_card:card1",
                "card identity and style key");
        require(overlay.value().x == std::vector<double>({2, 4}), "card input not rewritten");
        std::cout << "PASS " << checks
                  << " independent ownership/dispatch/transport/coordinate/error checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
