#include "gui/presentation/product_plot_refresh.h"
#include "tests/support/workspace_fixture.h"
#include "gui/plotting/qt_plot_sink.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/mainwindow.h"
#include "gui/widgets/reportwidget.h"
#include "qualpf/application/pfsm.h"
#include "tests/support/retired_presentation_probe.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QMessageBox>
#include <QAbstractButton>
#include <QTimer>
#include <QAction>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileDialog>
#include <QScrollBar>
#include <QSettings>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QLabel>
#include <QMouseEvent>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <thread>

class ProductWorkflowProbe {
public:
    static void load(MainWindow &window, const QString &path) { window.loadProject(path); }
    static void projectPath(MainWindow &window, const QString &path) { window.currentProjectFile = path; }
    static void seedPreview(MainWindow &window) {
        window.displayedPfsmPlot_ = std::make_shared<const qualpf::application::PfsmPlot>();
        window.displayedPfsmSource_ = window.context().workspace().session().snapshot();
        window.pfsmPreviewCandidate_ = "previous-project";
        window.pfsmPreviewPending_ = true;
    }
    static bool previewCleared(const MainWindow &window) {
        return !window.displayedPfsmPlot_ && window.displayedPfsmSource_.expired() &&
            window.pfsmPreviewCandidate_.isEmpty() && !window.pfsmPreviewPending_;
    }
};

using namespace qualpf;
using namespace qualpf::application; using namespace qualpf::gui;
using namespace qualpf::application::presentation; using namespace qualpf::gui::presentation;
void check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
static void settle() {
    QEventLoop loop;
    QTimer::singleShot(0, &loop, &QEventLoop::quit);
    loop.exec();
}
enum class NewReply { cancel, discard, save_cancel, save_failure, save };
static void newProject(MainWindow &window, NewReply reply, const QString &savePath = {}) {
    bool prompted = false, saveDialog = false, warning = false, timedOut = false;
    QTimer driver, watchdog;
    QObject::connect(&driver, &QTimer::timeout, &window, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            if (box->button(QMessageBox::Discard)) {
                prompted = true;
                const auto button = reply == NewReply::cancel ? QMessageBox::Cancel :
                    reply == NewReply::discard ? QMessageBox::Discard : QMessageBox::Save;
                box->button(button)->click();
            } else if (auto *button = box->button(QMessageBox::Ok)) {
                warning = true;
                button->click();
            }
        } else if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            saveDialog = true;
            if (reply == NewReply::save_cancel) dialog->reject();
            else {
                dialog->selectFile(savePath);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            }
        }
    });
    watchdog.setSingleShot(true);
    QObject::connect(&watchdog, &QTimer::timeout, &window, [&] {
        timedOut = true;
        if (auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget())) dialog->reject();
    });
    driver.start(1); watchdog.start(5000);
    window.findChild<QAction *>("actionNew")->trigger();
    driver.stop(); watchdog.stop();
    check(!timedOut && prompted, "New traverses the unsaved-project prompt");
    check(saveDialog == (reply == NewReply::save_cancel || reply == NewReply::save), "New save dialog routing");
    check(warning == (reply == NewReply::save_failure), "New save failure remains visible");
}
static void emptyCanvas(MainWindow &window) {
    const auto *plot = window.xpdViewer();
    check(plot->graphCount() == 0 && plot->plottableCount() == 0 && plot->itemCount() == 0,
          "empty project has no graphs, plottables or items");
    check(plot->obs.empty() && plot->refSet.empty() && plot->plotWave.empty(), "empty project releases plot data caches");
    check(!plot->axisRect()->visible() && plot->legend->itemCount() == 0, "empty canvas hides axes and legend entries");
    for (const auto *item : {&plot->calc, &plot->back, &plot->bpoints, &plot->diff, &plot->cdiff, &plot->peaks})
        check(item->graphIndex == -1 && item->xSize() == 0, "empty project releases layer data and graph identities");
    auto *scroll = window.findChild<QScrollBar *>("horizontalScrollBar");
    check(scroll && scroll->isHidden() && scroll->value() == 0, "empty project resets the plot scrollbar");
    check(ProductWorkflowProbe::previewCleared(window), "empty project releases previous plot and candidate previews");
}
static void startupAndNewControls(bool newFirst, bool timingOnly) {
    QTemporaryDir inputs;
    check(inputs.isValid(), "startup input directory");
    const auto write = [&](const QString &name, const QByteArray &bytes) {
        const auto path = inputs.filePath(name);
        QFile file(path);
        check(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "write independent startup input");
        return path;
    };
    const auto primary = write("primary.xy", "10 10\n20 100\n30 20\n");
    const auto comparison = write("comparison.xy", "10 5\n20 40\n30 15\n");
    MainWindow window(AppState::context());
    window.resize(1000, 700); window.show(); settle();
    QElapsedTimer timer;
    if (newFirst) {
        timer.start(); window.findChild<QAction *>("actionNew")->trigger();
        std::cout << "Cold New: " << timer.elapsed() << " ms\n";
    }
    timer.start();
    check(window.loadDiffractionPatterns({primary}, false), "direct startup import without a prior New");
    std::cout << "Startup import (prior New=" << newFirst << "): " << timer.elapsed() << " ms\n";
    auto *plot = window.xpdViewer();
    check(plot->graphCount() == 1 && plot->graph(0)->data()->size() == 3 && plot->axisRect()->visible(), "first import presents the measured curve");
    const auto imported = AppState::workspace().session().snapshot();
    const auto heights = std::minmax_element(imported->pattern.observed.begin(), imported->pattern.observed.end());
    check(plot->xAxis->range().lower <= 10 && plot->xAxis->range().upper >= 30 &&
          plot->yAxis->range().lower <= *heights.first && plot->yAxis->range().upper >= *heights.second,
          "first import scales both axes to the imported data");
    auto *report = window.findChild<ReportWidget *>();
    auto *browser = report->findChild<QTextBrowser *>("textBrowser");
    const auto filename = browser->document()->find("Filename");
    check(!filename.isNull() && browser->toPlainText().contains("primary.xy"), "first import publishes report content");
    check(filename.charFormat().fontPointSize() == 9, "report body retains its nine-point formatting");
    const bool inheritedFont = filename.charFormat().font().resolve(browser->document()->defaultFont()).family() == browser->font().family();
    if (timingOnly) {
        check(inheritedFont, "report body inherits the browser font");
        return;
    }
    auto &session = AppState::workspace().session();
    const auto source = session.snapshot();
    const auto *observed = plot->graph(0);
    const auto xRange = plot->xAxis->range(), yRange = plot->yAxis->range();
    const auto title = window.windowTitle();
    int changes = 0;
    auto subscription = session.subscribe([&](ProductHandle) { ++changes; });
    for (const auto reply : {NewReply::cancel, NewReply::save_cancel, NewReply::save_failure}) {
        // An existing directory is a deterministic unwritable project target.
        if (reply == NewReply::save_failure) ProductWorkflowProbe::projectPath(window, inputs.path());
        newProject(window, reply);
        check(changes == 0 && session.snapshot() == source && plot->graph(0) == observed &&
              plot->graph(0)->data()->size() == 3 && window.windowTitle() == title &&
              plot->xAxis->range().lower == xRange.lower && plot->xAxis->range().upper == xRange.upper &&
              plot->yAxis->range().lower == yRange.lower && plot->yAxis->range().upper == yRange.upper,
              "cancelled or failed New preserves the session, graph and viewport");
    }
    ProductWorkflowProbe::projectPath(window, {});
    const auto saved = inputs.filePath("saved.qpf");
    newProject(window, NewReply::save, saved);
    check(QFile::exists(saved) && changes == 1 && session.snapshot()->pattern.observed.empty(), "save then New resets the canonical session exactly once");
    emptyCanvas(window); settle(); emptyCanvas(window);
    check(inheritedFont, "report body inherits the browser font");

    check(window.loadDiffractionPatterns({primary, comparison}, false), "import comparisons after New");
    auto layered = *session.snapshot();
    layered.pattern.background = SampledSignal{{2, 2, 2}, SampleWindow{0}};
    layered.preprocessing.background_points = std::vector<preprocessing::BackgroundPoint>{{10, 2}, {30, 2}};
    layered.peaks.valid = true; layered.peaks.wave = layered.pattern.radiation.lines.front().wavelength.value;
    layered.peaks.tth = {20}; layered.peaks.d = {4.4}; layered.peaks.deltaD = {.01};
    layered.peaks.intensity = {17}; layered.peaks.intensityOrig = {17}; layered.peaks.fwhm = {.1};
    ProductCard phase; phase.id = "accepted"; phase.tth = {20}; phase.d = {4.4}; phase.intensity = {100};
    layered.candidates = {phase}; layered.phases = {phase};
    check(bool(session.commit(session.snapshot(), std::move(layered))), "seed background, peaks and accepted card");
    check(window.refreshPatternPresentation(), "present layered comparisons");
    window.refreshPreprocessingDisplay();
    CardPeakData sticks; sticks.id = "selected"; sticks.tth = {20}; sticks.d = {4.4};
    sticks.intensity = {50}; sticks.wave = 1.54056; sticks.color = Qt::red;
    plot->setCardPeaks({sticks});
    plot->drawSelectedComparePoints({20}, {50}, {Qt::red});
    plot->setDoubleCursorPosition(12, 28);
    PlotSeries preview; preview.x = {10, 30}; preview.y = {1, 2}; preview.wavelength = 1.54056;
    QtPlotSink privateSink(plot);
    PlotUpdate privateUpdate; privateUpdate.commands.push_back({CommandKind::upsert, 1, project_preview(preview, Abscissa::two_theta).value()});
    check(bool(privateSink.apply(privateUpdate)), "seed a private preprocessing preview");
    ProductWorkflowProbe::seedPreview(window);
    auto settings = plot->pSettings();
    settings.backColor = QColor(24, 48, 72);
    settings.legendVisible = !settings.legendVisible;
    plot->setPlotSettings(settings);
    check(plot->obs.size() == 2 && plot->peaks.graphIndex >= 0 && !plot->refSet.empty() && plot->itemCount() > 0, "layered New starts with real plot content");
    changes = 0;
    newProject(window, NewReply::discard);
    check(changes == 1 && session.snapshot()->comparisonPatterns.empty() && session.snapshot()->phases.empty(), "discard then New publishes only the empty session");
    emptyCanvas(window); settle(); emptyCanvas(window);
    check(plot->pSettings().getAbscissa() == settings.getAbscissa() && plot->pSettings().backColor == settings.backColor &&
          plot->pSettings().legendVisible == settings.legendVisible, "New retains presentation settings");
    window.findChild<QAction *>("actionNew")->trigger(); emptyCanvas(window);
    check(changes == 2, "repeated New remains a single reset");
    check(window.loadDiffractionPatterns({comparison}, false), "reimport after a complete clear");
    privateUpdate.commands.front().kind = CommandKind::erase;
    check(bool(privateSink.apply(privateUpdate)) && plot->graphCount() == 1 && plot->graph(0)->data()->size() == 3,
          "old private preview cannot erase a newly imported curve");
    plot->drawPlot();
    check(plot->graphCount() == 1, "old selected-card overlays do not reappear on redraw");
    const auto emptyProject=inputs.filePath("empty.qpf");
    qualpf::application::ProductState emptyState;emptyState.pattern.source_id="empty";
    check(bool(qualpf::infrastructure::QpfProjectWriter{}.writeAtomic(emptyProject.toStdString(),{std::make_shared<const qualpf::application::ProductState>(emptyState),std::make_shared<const qualpf::application::ProjectViewState>(),{},{},0})),"write empty QPF");
    ProductWorkflowProbe::load(window, emptyProject);
    emptyCanvas(window); settle(); emptyCanvas(window);
    window.findChild<QAction *>("actionNew")->trigger();
    emptyCanvas(window);
}
static void hover(XpdViewWidget &view, double x, double y) {
    const QPointF local(view.xAxis->coordToPixel(x), view.yAxis->coordToPixel(y));
    QMouseEvent event(QEvent::MouseMove, local, view.mapToGlobal(local.toPoint()),
                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&view, &event);
}
static void hoverControls() {
    QWidget owner;
    bindWorkspaceContext(owner, AppState::context());
    XpdViewWidget view(&owner);
    QLabel cursor, sample, reflection;
    for (auto *label : {&cursor, &sample, &reflection}) label->resize(4096, 40);
    view.connectLabels(&cursor, &sample, &reflection);
    view.setGraphicArea();
    view.addProjectedPlot({10, 20, 30}, {10, 100, 20}, graphItem::Observed, 2, 1.54056, "Measured");
    view.drawPlot();
    int moves = 0;
    QObject::connect(&view, &QCustomPlot::mouseMove, &view, [&](QMouseEvent *) { ++moves; });

    // Reproduce the original PFSM state: a phase row exists without display
    // reflections. This real mouse event crashed the original release binary.
    ReflectionBar empty;
    empty.visible = true;
    empty.yPos = 10;
    view.refSet.push_back(empty);
    std::cout << "Hover regression: empty reflection row\n" << std::flush;
    hover(view, 20, 10);
    check(moves == 1 && reflection.text().isEmpty(), "empty reflections are not hover targets");

    ReflectionBar hidden;
    hidden.yPos = 10;
    hidden.ref = {{{0, 0, 1}, 20}};
    ReflectionBar visible;
    visible.visible = true;
    visible.wave = 1.54056;
    visible.yPos = 30;
    visible.ref = {{{1, 0, 0}, 10}, {{2, 0, 0}, 30}};
    view.refSet = {empty, hidden, visible};
    hover(view, 30, 10);
    check(reflection.text().contains("Refl: #2") && reflection.text().contains("Phase: 2"), "only visible nonempty bars are searched");
    hover(view, 20, 30);
    check(reflection.text().contains("Refl: #2"), "nearest-reflection ties retain the upper index");
    visible.ref = {{{1, 0, 0}, 20}};
    view.refSet = {visible};
    for (double x : {9., 20., 31.}) {
        hover(view, x, 30);
        check(reflection.text().contains("Refl: #1"), "singleton reflection endpoints");
    }
    view.refSet = {hidden};
    hover(view, 20, 10);
    check(reflection.text().isEmpty(), "hidden reflections clear old hover text");
    view.refSet.clear();

    // The shorter/singleton curve must not dereference its end iterator even
    // though the measured canvas has a wider domain.
    view.graph(0)->setData(QVector<double>{20}, QVector<double>{100});
    for (double x : {9., 20., 30., 31.}) {
        hover(view, x, 100);
        check(sample.text().contains("Count: #1"), "singleton curve endpoints");
    }
    view.graph(0)->setData(QVector<double>{10, 20, 30}, QVector<double>{10, 100, 20});
    hover(view, 30, 20);
    check(sample.text().contains("Count: #3"), "last curve point remains valid");
    view.plotWave.clear();
    hover(view, 20, 100);
    check(sample.text().isEmpty(), "missing wavelength clears old sample text");
    view.plotWave = {1.54056};

    for (auto axis : {xpdutils::TTHETA, xpdutils::DVALUE, xpdutils::ONE_OVER_DVALUE, xpdutils::ONE_OVER_DVALUE2}) {
        auto settings = view.pSettings();
        settings.setAbscissa(axis);
        view.setPlotSettings(settings);
        // Independent Bragg coordinates for 10 and 30 degrees. D-spacing is
        // descending; both directions must search the already displayed axis.
        const auto coordinate = [&](double degrees) {
            const double d = visible.wave / (2 * std::sin(degrees * std::numbers::pi / 360));
            switch (axis) {
            case xpdutils::DVALUE: return d;
            case xpdutils::ONE_OVER_DVALUE: return 1 / d;
            case xpdutils::ONE_OVER_DVALUE2: return 1 / (d * d);
            default: return degrees;
            }
        };
        visible.ref = {{{1, 0, 0}, coordinate(10)}, {{2, 0, 0}, coordinate(30)}};
        view.refSet = {visible};
        hover(view, coordinate(29), 30);
        check(reflection.text().contains("Refl: #2"), "reflection hover uses display coordinates on every axis");
        hover(view, coordinate(10), 30);
        check(reflection.text().contains("Refl: #1"), "first reflection endpoint on every axis");
    }

    view.refSet.clear();
    PlotSeries overlay;
    overlay.kind = SeriesKind::accepted_card;
    overlay.identity = "accepted";
    overlay.label = "Accepted phase";
    overlay.x = {20};
    overlay.wavelength = 1.54056;
    overlay.visibility = Visibility::visible;
    view.applyProjectedCard(overlay, Qt::red);
    check(view.refSet.size() == 1 && view.refSet.front().visible, "nonempty phase overlay is retained");
    overlay.visibility = Visibility::hidden;
    view.applyProjectedCard(overlay, Qt::red);
    check(!view.refSet.front().visible, "same-geometry visibility update is applied");
    overlay.x.clear();
    reflection.setText("stale phase");
    view.applyProjectedCard(overlay, Qt::red);
    check(view.refSet.isEmpty() && reflection.text().isEmpty(), "empty overlay removes stale phase and hover text");

    view.setProperty("workspaceClosing", true);
    cursor.setText("closing"); sample.setText("closing"); reflection.setText("closing");
    hover(view, 20, 30);
    check(cursor.text() == "closing" && sample.text() == "closing" && reflection.text() == "closing", "closing suppresses hover publication");
    view.setProperty("workspaceClosing", false);

    auto *temporaryLabel = new QLabel;
    view.connectLabels(temporaryLabel, temporaryLabel, temporaryLabel);
    delete temporaryLabel;
    hover(view, 20, 30); // Detached labels must not be accessed.
    check(moves > 10, "mouse events traverse the actual widget path");
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir settings;
    if (!settings.isValid()) { std::cerr << "A writable temporary settings directory is required\n"; return 1; }
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    try {
        const bool timingOnly = argc == 2 && (std::string(argv[1]) == "--startup-timing" || std::string(argv[1]) == "--new-import-timing");
        startupAndNewControls(argc == 2 && std::string(argv[1]) == "--new-import-timing", timingOnly);
        if (timingOnly) return 0;
        hoverControls();
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
            const auto emptyPath=project.filePath("empty-pattern.qpf");
            qualpf::application::ProductState emptyState;emptyState.pattern.source_id="empty-pattern-boundary";
            check(bool(qualpf::infrastructure::QpfProjectWriter{}.writeAtomic(emptyPath.toStdString(),{std::make_shared<const qualpf::application::ProductState>(emptyState),std::make_shared<const qualpf::application::ProjectViewState>(),{},{},0})),"write independent empty QPF state");
            ProductWorkflowProbe::load(window, emptyPath);
            const auto loaded = AppState::searchSession().snapshot();
            check(loaded != populated && loaded->pattern.two_theta.empty() && loaded->pattern.observed.empty() && loaded->peaks.tth.empty(), "empty QPF publishes canonical empty state");
            check(loaded->pattern.source_id == "empty-pattern-boundary" && window.windowTitle().contains("empty-pattern.qpf"), "empty QPF successful project publication");
            check(!legacy::LegacyPresentationMirror::process().exists(), "empty QPF does not allocate compatibility shell");
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
            check(!discarded, "New after a clean QPF open requires no unsaved-project prompt");
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
