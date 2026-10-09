#include "gui/presentation/product_plot_refresh.h"
#include "tests/support/workspace_fixture.h"
#include "gui/plotting/qt_plot_sink.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/mainwindow.h"
#include "tests/support/retired_presentation_probe.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QSettings>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>

class ProductWorkflowProbe {
public:
    static void load(MainWindow &window, const QString &path) { window.loadProject(path); }
};

using namespace qualpf;
using namespace qualpf::application; using namespace qualpf::gui;
using namespace qualpf::application::presentation; using namespace qualpf::gui::presentation;
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    try {
        PresentationWorkspace workspace;
        workspace.configure(PresentationMode::automatic_gui);
        SessionStateView state;
        state.pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{20}, TwoThetaDegrees{30}};
        state.pattern.observed = {10, 100, 20};
        state.pattern.radiation.lines = {{WavelengthAngstrom{1.54056}, 1}};
        state.pattern.source_id = "folder/primary.dat";
        state.peaks.tth = {20}; state.peaks.intensity = {17}; state.peaks.wave = 1.54056;
        state.comparisonPatterns.push_back(state.pattern);
        state.comparisonPatterns.back().source_id = "folder/secondary.xy";
        const auto projected = project_product_refresh(state, workspace);
        check(bool(projected) && projected.value().size() == 3, "prefix frames");
        check(projected.value()[0].action == 8 && projected.value()[1].action == 5 &&
              projected.value()[1].update.commands.front().rescale == 0, "ordered style/rescale");
        check(projected.value()[0].update.snapshot.series.size() == 1 &&
              projected.value()[2].update.snapshot.series.size() == 2, "owning prefixes");
        QWidget viewOwner;
        bindWorkspaceContext(viewOwner,AppState::context());
        XpdViewWidget view(&viewOwner);
        gui::QtPlotSink sink(&view);
        for (const auto &frame : projected.value()) check(bool(sink.apply(frame.update)), "Qt frame application");
        check(view.obs.size() == 2 && view.graphCount() == 2 && view.plotWave.size() == 2, "comparison graph lifetime");
        auto session = project_session_update(make_plot_input(state, workspace, 0));
        check(bool(session) && bool(sink.apply(session.value())), "residual refresh");
        check(view.peaks.graphIndex >= 0 && view.graph(view.peaks.graphIndex)->data()->at(0)->value == 17, "current residual intensity");
        check(view.graphCount() == view.plotWave.size(), "wave graph identity");
        PlotSeries preview;
        preview.x = {10, 20}; preview.y = {1, 2}; preview.wavelength = 1.54056;
        auto curve = project_preview(preview, Abscissa::two_theta);
        PlotUpdate overlay;
        overlay.commands.push_back({CommandKind::upsert, 1, curve.value()});
        gui::QtPlotSink privateSink(&view);
        check(bool(privateSink.apply(overlay)), "private preview");
        const auto previewGraphs = view.graphCount();
        PlotUpdate bad = overlay;
        bad.commands.insert(bad.commands.begin(), {CommandKind::replace_canvas, 1, {}});
        bad.commands.back().series->y.push_back(3);
        check(!sink.apply(bad) && view.graphCount() == previewGraphs, "preflight prevents partial mutation");
        bool wrongThread = false;
        std::thread worker([&] { auto result = sink.validate(overlay); wrongThread = !result && result.error().reason == "wrong_thread"; });
        worker.join(); check(wrongThread, "GUI-thread rejection");
        for (const auto &frame : projected.value()) check(bool(sink.apply(frame.update)), "replace while private editor owns preview");
        overlay.commands.front().kind = CommandKind::erase;
        check(bool(privateSink.apply(overlay)) && view.graphCount() == 2, "destroyed preview cannot erase new graphs");
        workspace.configure(PresentationMode::headless);
        check(project_product_refresh(state, workspace).value().empty(), "headless has no frames");
        check(!legacy::LegacyPresentationMirror::process().exists(), "direct sink does not allocate Legacy shell");
        {
            MainWindow window(AppState::context());
            check(QMetaObject::invokeMethod(&window, "onActionNewTriggered", Qt::DirectConnection), "cold New action");
            check(!legacy::LegacyPresentationMirror::process().exists(), "New does not create a compatibility shell");
            check(AppState::searchSession().snapshot()->pattern.two_theta.empty(), "New resets canonical pattern");
            SessionStateView seed;
            seed.pattern = state.pattern;
            seed.pattern.id = {1};
            const auto seeded = AppState::searchSession().commit(AppState::searchSession().snapshot(), seed);
            if (!seeded) throw std::runtime_error(seeded.error().message);
            const auto populated = AppState::searchSession().snapshot();
            QTemporaryDir project;
            check(project.isValid(), "empty project directory");
            const auto emptyPath = project.filePath("empty-pattern.qxp");
            QFile emptyFile(emptyPath);
            const QByteArray emptyInput = R"({"version":2,"diffraction_data":{"filename":"empty-pattern-boundary","x":[],"y":[],"wave":[],"ratio":[]}})";
            check(emptyFile.open(QIODevice::WriteOnly) && emptyFile.write(emptyInput) == emptyInput.size(), "write independent empty QXP input");
            emptyFile.close();
            ProductWorkflowProbe::load(window, emptyPath);
            const auto loaded = AppState::searchSession().snapshot();
            check(loaded != populated && loaded->pattern.two_theta.empty() && loaded->pattern.observed.empty() && loaded->peaks.tth.empty(), "empty QXP publishes canonical empty state");
            check(loaded->pattern.source_id == "empty-pattern-boundary" && window.windowTitle().contains("empty-pattern.qxp"), "empty QXP successful project publication");
            check(!legacy::LegacyPresentationMirror::process().exists(), "empty QXP does not allocate compatibility shell");
            bool discarded = false;
            QTimer newDriver;
            QObject::connect(&newDriver, &QTimer::timeout, [&] {
                if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                    if (auto *button = box->button(QMessageBox::Discard)) {
                        discarded = true;
                        newDriver.stop();
                        button->click();
                    }
                }
            });
            newDriver.start(5);
            check(QMetaObject::invokeMethod(&window, "onActionNewTriggered", Qt::DirectConnection), "New after empty project");
            newDriver.stop();
            check(discarded, "New follows actual unsaved project Discard path");
            check(QMetaObject::invokeMethod(&window, "onActionNewTriggered", Qt::DirectConnection), "repeated New");
            check(AppState::searchSession().snapshot()->pattern.two_theta.empty() && AppState::searchSession().snapshot()->pattern.source_id.empty(), "repeated New resets canonical project");
        }
        state.pattern.observed[0] = std::numeric_limits<double>::infinity();
        workspace.configure(PresentationMode::automatic_gui);
        check(!project_product_refresh(state, workspace), "invalid input deterministic rejection");
        std::cout << "PASS: Qt sink prefixes, residuals, preview lifetime, atomic validation, GUI-thread and headless contracts\n";
        return 0;
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
