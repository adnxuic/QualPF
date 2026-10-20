#include "gui/mainwindow.h"
#include "gui/dialogs/initial_peak_options_dialog.h"
#include "gui/widgets/initial_peak_candidates_widget.h"
#include "gui/plotting/xpdviewwidget.h"
#include "tests/support/workspace_fixture.h"
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QSlider>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <atomic>
#include <cmath>
#include <iostream>
#include <semaphore>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
using qualpf::gui::SearchTaskController;
class ProductWorkflowProbe {
  public:
    static bool saved(const MainWindow &window) { return window.projectFileSaved; }
    static void refreshDisplay(MainWindow &window) { window.refreshPreprocessingDisplay(); }
};
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) throw std::runtime_error("Initial peak GUI at line " + std::to_string(at.line()));
}
struct Gate {
    std::atomic<bool> block = false, entered = false, holdQueued = false, queued = false;
    std::binary_semaphore release{0}, releaseQueued{0};
};
struct Initial final : preprocessing::IInitialPeakFinder {
    std::shared_ptr<Gate> gate;
    mutable std::atomic<int> calls = 0;
    mutable bool worker = false;
    std::thread::id owner = std::this_thread::get_id();
    mutable preprocessing::InitialPeakOptions received;
    explicit Initial(std::shared_ptr<Gate> g) : gate(std::move(g)) {}
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.initial.cpp", 5}; }
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &spectrum, WavelengthAngstrom w,
        const preprocessing::InitialPeakOptions &o, const ExecutionContext &c) const override {
        const auto &p = spectrum.pattern();
        ++calls; worker = std::this_thread::get_id() != owner; received = o;
        if (gate->block) { gate->entered = true; gate->release.acquire(); }
        return preprocessing::ClassicInitialPeakFinder{}.find(spectrum, w, o, c);
    }
};
struct Gaussian final : preprocessing::IFwhmEstimator {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.fwhm.cpp", 3}; }
    Result<preprocessing::FwhmEstimationResult> estimate(const AnalysisSpectrum &spectrum, const preprocessing::InitialPeakResult &,
        const ExecutionContext &) const override {
        ++calls;
        return Result<preprocessing::FwhmEstimationResult>{preprocessing::FwhmEstimationResult{
            {preprocessing::FwhmEstimate::State::measured, AngularWidthDegrees{double(.2f)}, 1}, {}}};
    }
};
struct Formal final : preprocessing::IExperimentalPeakExtractor {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.experimental.cpp", 3}; }
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &spectrum, const preprocessing::PeakCalibration &c,
        WavelengthAngstrom wave, AngularWidthDegrees, const ExecutionContext &) const override {
        const auto &p = spectrum.pattern();
        ++calls; preprocessing::PeakSearchResult r; r.applied = c.applied;
        r.peaks.dataset = r.candidates.dataset = p.id;
        r.peaks.effective_wavelength = r.candidates.effective_wavelength = wave;
        Peak peak; peak.id = {1}; peak.two_theta = TwoThetaDegrees{21}; peak.intensity = 20; peak.original_intensity = 20;
        peak.d_spacing = DSpacingAngstrom{wave.value / (2 * std::sin(21 * 3.14159265358979323846 / 360))};
        peak.delta_d = DSpacingAngstrom{.01}; peak.fwhm = AngularWidthDegrees{.2}; peak.integrated_intensity = 1;
        r.peaks.peaks = r.candidates.peaks = {peak}; r.automatic_tolerance = .5;
        return Result<preprocessing::PeakSearchResult>{r};
    }
};
struct Reader final : CandidateReader {
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        return Result<std::vector<ProductCard>>{std::vector<ProductCard>{}};
    }
};
struct Factory final : CandidateReaderFactory {
    Result<std::unique_ptr<CandidateReader>> open(const ReferenceDatabase &, std::uint64_t, const ExecutionContext &) override {
        return Result<std::unique_ptr<CandidateReader>>{std::make_unique<Reader>()};
    }
};
static ProductState state(AlgorithmHandle algorithms) {
    ProductState s; s.algorithms = std::move(algorithms); auto &p = s.pattern;
    p.id = {141}; p.source_id = "initial-gui-analytic-gaussian";
    p.radiation.lines = {{WavelengthAngstrom{1.54056}, 1}};
    p.processing.background_subtracted = false; p.processing.intensity_scaled = false; p.processing.kalpha2_subtracted = false;
    for (int i = 0; i <= 400; ++i) {
        const double x = 19. + .01 * i; p.two_theta.emplace_back(x);
        p.observed.push_back(10. + 100. * std::exp(-.5 * std::pow((x - 21.) / .12, 2)));
    }
    p.original_observed = p.observed;
    p.background = SampledSignal{std::vector<double>(401, 10), SampleWindow{0}};
    p.calculation_range = AngularRange{p.two_theta.front(), p.two_theta.back()};
    return s;
}
template<class T> static T *widget(QObject &parent, const char *name) {
    auto *value = parent.findChild<T *>(QString::fromLatin1(name)); check(value != nullptr); return value;
}
static void waitFor(MainWindow &window, std::function<void()> start) {
    QEventLoop loop; bool timedOut = false;
    QObject::connect(window.searchTasks(), &SearchTaskController::finished, &loop, [&](quint64, bool) {
        if (!window.searchTasks()->busy()) loop.quit();
    });
    QTimer timer; timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
    timer.start(15000); start(); if (window.searchTasks()->busy()) loop.exec();
    check(!timedOut && !window.searchTasks()->busy());
}
static void waitUntil(const std::atomic<bool> &entered) {
    QEventLoop loop; QTimer poll, timeout; bool timedOut = false;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (entered) loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
    poll.start(1); timeout.setSingleShot(true); timeout.start(10000); if (!entered) loop.exec();
    check(!timedOut && entered);
}
struct GraphSnapshot {
    QString name;
    std::vector<std::pair<double, double>> samples;
    bool operator==(const GraphSnapshot &) const = default;
};
static std::vector<GraphSnapshot> plotData(MainWindow &window) {
    std::vector<GraphSnapshot> result;
    auto *view = window.xpdViewer();
    for (int i = 0; i < view->graphCount(); ++i) {
        const auto *graph = view->graph(i);
        check(!graph->property("initialPeakCandidates").toBool() &&
            !graph->name().contains("Initial peak", Qt::CaseInsensitive));
        GraphSnapshot snapshot{graph->name(), {}};
        for (auto at = graph->data()->constBegin(); at != graph->data()->constEnd(); ++at)
            snapshot.samples.emplace_back(at->key, at->value);
        result.push_back(std::move(snapshot));
    }
    return result;
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    if (argc == 3 && std::string(argv[1]) == "--capture") {
        if (QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf") >= 0)
            app.setFont(QFont("Segoe UI", 9));
    }
    try {
        QTemporaryDir temporary; check(temporary.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        app.setOrganizationName("QualPF-tests"); app.setApplicationName("initial-peak-options");
        auto gate = std::make_shared<Gate>(); auto initial = std::make_shared<Initial>(gate);
        auto gaussian = std::make_shared<Gaussian>(); auto formal = std::make_shared<Formal>();
        AlgorithmModules modules; modules.initial_peaks = initial; modules.fwhm = gaussian; modules.experimental_peaks = formal;
        auto resolved = resolveAlgorithms(AlgorithmSpec{}, modules); check(bool(resolved));
        auto &session = AppState::workspace().session(); check(bool(session.commit(session.snapshot(), state(resolved.value()))));
        MainWindow window(AppState::context(), nullptr, std::make_shared<Factory>(), [gate](auto event, auto, auto) {
            if (event == SearchTaskController::TaskEvent::completion_queued && gate->holdQueued) {
                gate->queued = true; gate->releaseQueued.acquire();
            }
        });
        window.resize(1200, 800); window.show(); window.enableActions(MainWindow::PatternAction); check(window.refreshPatternPresentation());
        // Compare against the same direct session refresh used after publication,
        // rather than the initial binary32 compatibility frame.
        ProductWorkflowProbe::refreshDisplay(window);
        auto *options = widget<QAction>(window, "actionInitialPeakSearchOptions");
        auto *run = widget<QAction>(window, "actionInitialPeakSearch");
        auto *dialog = widget<InitialPeakOptionsDialog>(window, "initialPeakOptionsDialog");
        auto *results = widget<InitialPeakCandidatesWidget>(*dialog, "initialPeakCandidatesPage");
        auto *tabs = widget<QTabWidget>(*dialog, "initialPeakOptionsTabs");
        auto *slope = widget<QLineEdit>(*dialog, "initialPeakSlopeEdit");
        auto *automatic = widget<QCheckBox>(*dialog, "initialPeakAutomaticWindow");
        auto *half = widget<QSpinBox>(*dialog, "initialPeakHalfWindow");
        auto *degree = widget<QSpinBox>(*dialog, "initialPeakDegree");
        auto *status = widget<QLabel>(*dialog, "initialPeakStatus");
        auto *buttons = widget<QDialogButtonBox>(*dialog, "buttonBox");
        auto *table = widget<QTableView>(*results, "initialPeakCandidateTable");
        QString capture;
        if (argc == 3 && std::string(argv[1]) == "--capture") { capture = QString::fromLocal8Bit(argv[2]); check(QDir().mkpath(capture)); }
        QString unexpectedModal;
        QTimer modalWatch;
        QObject::connect(&modalWatch, &QTimer::timeout, &window, [&] {
            for (auto *w : QApplication::topLevelWidgets())
                if (auto *box = qobject_cast<QMessageBox *>(w); box && box->isVisible() &&
                    box->standardButtons() != (QMessageBox::Yes | QMessageBox::No)) {
                    unexpectedModal = box->text(); std::cerr << "Unexpected dialog: " << unexpectedModal.toStdString() << std::endl; box->reject();
                }
        });
        modalWatch.start(20);
        auto step = [](const char *name) { std::cerr << "Initial peak GUI: " << name << std::endl; };
        step("defaults and independent results");
        const auto original = session.snapshot(); const auto title = window.windowTitle();
        check(options->isEnabled() && run->isEnabled()); options->trigger();
        check(dialog->isVisible() && automatic->isChecked() && !half->isEnabled() && half->value() == 11 && degree->value() == 9);
        check(tabs->count() == 2 && tabs->currentIndex() == 0 && tabs->widget(1) == results && results->window() == dialog && !results->isWindow());
        check(table->editTriggers() == QAbstractItemView::NoEditTriggers && table->model()->rowCount() == 0);
        check(slope->text().toDouble() == .1 && session.snapshot() == original);
        if (!capture.isEmpty()) check(dialog->grab().save(capture + "/initial-peak-options.png"));
        slope->setText("-1"); buttons->button(QDialogButtonBox::Ok)->click();
        check(dialog->isVisible() && !window.searchTasks()->busy() && session.snapshot() == original);
        buttons->button(QDialogButtonBox::RestoreDefaults)->click(); dialog->reject();
        check(session.snapshot() == original && window.windowTitle() == title && ProductWorkflowProbe::saved(window) && run->isEnabled());
        const auto originalPlot = plotData(window);
        waitFor(window, [&] { run->trigger(); });
        auto found = session.snapshot(); check(found != original && found->preprocessing.initial_peak_usage);
        check(!found->preprocessing.initial_peaks && initial->worker && gaussian->calls == 0 && formal->calls == 0);
        check(found->pattern.observed == original->pattern.observed && found->peaks.tth == original->peaks.tth &&
            window.windowTitle() == title && ProductWorkflowProbe::saved(window));
        const auto &usage = *found->preprocessing.initial_peak_usage;
        check(results->isVisible() && table->model()->rowCount() == int(usage.detection.candidates.size()) && table->model()->rowCount() > 0);
        check(dialog->isVisible() && tabs->currentWidget() == results && buttons->button(QDialogButtonBox::Close)->isVisible() &&
            !buttons->button(QDialogButtonBox::Ok)->isVisible() && !run->isEnabled());
        const auto at = usage.detection.candidates.front().sample_index;
        check(table->model()->data(table->model()->index(0, 0)).toULongLong() == at + 1);
        check(std::abs(table->model()->data(table->model()->index(0, 1)).toDouble() - found->pattern.two_theta[at].value) < 1e-10);
        check(plotData(window) == originalPlot);
        if (!capture.isEmpty()) {
            // Let Qt apply the layout requests posted by the tab/button change.
            QEventLoop rendered; QTimer::singleShot(0, &rendered, &QEventLoop::quit); rendered.exec();
            const auto *close = buttons->button(QDialogButtonBox::Close);
            check(close->mapTo(dialog, close->rect().bottomRight()).y() < dialog->height());
            check(dialog->grab().save(capture + "/initial-peak-candidates.png"));
            check(window.xpdViewer()->grab().save(capture + "/initial-peak-plot.png"));
        }
        buttons->button(QDialogButtonBox::Close)->click(); check(!dialog->isVisible() && run->isEnabled());
        const auto retained = session.snapshot(); options->trigger();
        check(tabs->currentIndex() == 0 && table->model()->rowCount() == int(usage.detection.candidates.size()));
        slope->setText("0.75"); tabs->setCurrentWidget(results); buttons->button(QDialogButtonBox::Close)->click();
        check(session.snapshot() == retained && run->isEnabled());
        options->trigger(); check(slope->text().toDouble() == .1); dialog->reject();
        const int calls = initial->calls; waitFor(window, [&] { run->trigger(); });
        check(initial->calls == calls + 1 && window.windowTitle() == title && ProductWorkflowProbe::saved(window));
        check(plotData(window) == originalPlot); dialog->reject();
        step("display axes without candidate markers");
        for (int axis = 0; axis < 4; ++axis) {
            auto settings = window.xpdViewer()->pSettings(); settings.setAbscissa(static_cast<xpdutils::xAbscissaType>(axis));
            window.xpdViewer()->setPlotSettings(settings);
            ProductWorkflowProbe::refreshDisplay(window);
            const auto before = plotData(window); waitFor(window, [&] { run->trigger(); });
            check(plotData(window) == before && tabs->currentWidget() == results);
            check(std::abs(table->model()->data(table->model()->index(0, 1)).toDouble() - found->pattern.two_theta[at].value) < 1e-10);
            dialog->reject();
        }
        step("custom options");
        options->trigger(); automatic->setChecked(false); half->setValue(20); degree->setValue(7); slope->setText("0.123456789");
        check(half->isEnabled()); waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(dialog->isVisible() && tabs->currentWidget() == results && session.snapshot()->preprocessing.initial_peaks->options == preprocessing::InitialPeakOptions{.123456789f, false, 20, 7});
        check(!ProductWorkflowProbe::saved(window) && gaussian->calls == 0 && formal->calls == 0);
        tabs->setCurrentIndex(0); const auto precise = session.snapshot()->preprocessing.initial_peaks->options;
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(session.snapshot()->preprocessing.initial_peaks->options == precise);
        tabs->setCurrentIndex(0); half->setValue(2); degree->setValue(20);
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(widget<QLabel>(*results, "initialPeakCandidateSummary")->text().contains("degree: 4"));
        dialog->reject();
        auto extended = *session.snapshot();
        extended.preprocessing.initial_peaks->extensions["future"] = MetadataObject{{"wide", std::int64_t{9007199254740993LL}}, {"null", nullptr}};
        check(bool(session.commit(session.snapshot(), extended))); const auto extensions = extended.preprocessing.initial_peaks->extensions;
        options->trigger(); buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        check(automatic->isChecked() && half->value() == 11 && degree->value() == 9 && slope->text().toDouble() == .1);
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(session.snapshot()->preprocessing.initial_peaks->options == preprocessing::InitialPeakOptions{} &&
            session.snapshot()->preprocessing.initial_peaks->extensions == extensions);
        tabs->setCurrentIndex(0); slope->setText("200"); waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(session.snapshot()->preprocessing.initial_peak_usage->detection.candidates.empty() && table->model()->rowCount() == 0 && tabs->currentWidget() == results);
        plotData(window); dialog->reject();
        step("short pattern retry");
        auto shortState = state(resolved.value()); auto &p = shortState.pattern;
        p.two_theta.erase(p.two_theta.begin() + 211, p.two_theta.end()); p.two_theta.erase(p.two_theta.begin(), p.two_theta.begin() + 190);
        p.observed.erase(p.observed.begin() + 211, p.observed.end()); p.observed.erase(p.observed.begin(), p.observed.begin() + 190);
        p.original_observed = p.observed; p.background = SampledSignal{std::vector<double>(21, 10), SampleWindow{0}};
        p.calculation_range = AngularRange{p.two_theta.front(), p.two_theta.back()};
        check(bool(session.commit(session.snapshot(), shortState))); window.enableActions(MainWindow::PatternAction);
        options->trigger(); automatic->setChecked(false); half->setValue(100); auto failedSource = session.snapshot();
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(dialog->isVisible() && session.snapshot() == failedSource && status->text().contains("too few samples"));
        half->setValue(2); waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(dialog->isVisible() && tabs->currentWidget() == results); dialog->reject();
        step("main peak redetection");
        auto routed = state(resolved.value()); routed.preprocessing.initial_peaks = InitialPeakConfiguration{{.125f, false, 20, 7}, {}};
        routed.preprocessing.fwhm = FwhmConfiguration{};
        check(bool(session.commit(session.snapshot(), routed))); window.enableActions(MainWindow::PatternAction);
        waitFor(window, [&] { run->trigger(); }); const int beforeRedetection = initial->calls;
        check(session.snapshot()->preprocessing.initial_peak_usage != nullptr);
        dialog->reject();
        widget<QAction>(window, "actionPeak_Search")->trigger();
        check(initial->calls == beforeRedetection + 1 && initial->received == routed.preprocessing.initial_peaks->options &&
            gaussian->calls == 1 && formal->calls == 1);
        step("editor redetection");
        widget<QAction>(window, "actionPeak_Search_Conditions")->trigger();
        auto *peakOptions = window.findChild<PeakSearchDialog *>(); check(peakOptions && peakOptions->isVisible());
        const int beforeReselect = initial->calls;
        widget<QSlider>(*peakOptions, "intensitySlider")->setValue(25); check(initial->calls == beforeReselect);
        auto *sensitivity = widget<QSpinBox>(*peakOptions, "sensitivitySpinBox"); sensitivity->setValue(sensitivity->value() == 50 ? 51 : 50);
        check(initial->calls > beforeReselect && initial->received == routed.preprocessing.initial_peaks->options);
        static_cast<QDialog *>(peakOptions)->reject();
        step("missing peak preparation");
        auto missingPeaks = *session.snapshot(); missingPeaks.peaks.clear(); missingPeaks.preprocessing.peak_candidates.reset();
        missingPeaks.preprocessing.rejected_candidates.clear(); missingPeaks.provenance.peakSource.reset();
        check(bool(session.commit(session.snapshot(), missingPeaks))); const int beforeMatch = initial->calls;
        waitFor(window, [&] { window.runSearchMatch(); });
        check(initial->calls > beforeMatch && initial->received == routed.preprocessing.initial_peaks->options);
        check(bool(session.commit(session.snapshot(), state(resolved.value())))); window.enableActions(MainWindow::PatternAction);
        waitFor(window, [&] { run->trigger(); }); check(table->model()->rowCount() > 0); plotData(window);
        step("candidate invalidation");
        auto unrelated = *session.snapshot(); auto evidence = unrelated.preprocessing.initial_peak_usage;
        unrelated.projectMetadata["unrelated"] = true; check(bool(session.commit(session.snapshot(), unrelated)));
        check(session.snapshot()->preprocessing.initial_peak_usage == evidence && table->model()->rowCount() == int(evidence->detection.candidates.size()));
        auto edited = *session.snapshot(); edited.pattern.observed[0] += 1;
        check(bool(session.commit(session.snapshot(), edited))); check(table->model()->rowCount() == 0); plotData(window);
        dialog->reject();
        auto noWave = *session.snapshot(); noWave.pattern.radiation.lines.clear();
        check(bool(session.commit(session.snapshot(), noWave))); check(!run->isEnabled());
        noWave = *session.snapshot(); noWave.pattern.radiation = original->pattern.radiation;
        check(bool(session.commit(session.snapshot(), noWave))); check(run->isEnabled());
        step("running cancellation");
        options->trigger(); gate->block = true; gate->entered = false;
        const auto canceledSource = session.snapshot(); buttons->button(QDialogButtonBox::Ok)->click(); waitUntil(gate->entered);
        check(!buttons->button(QDialogButtonBox::Ok)->isEnabled() && !run->isEnabled());
        waitFor(window, [&] { dialog->close(); gate->release.release(); });
        check(session.snapshot() == canceledSource && !dialog->isVisible() && run->isEnabled());
        step("stale publication");
        options->trigger(); gate->entered = false; buttons->button(QDialogButtonBox::Ok)->click(); waitUntil(gate->entered);
        edited = *session.snapshot(); edited.projectMetadata["stale"] = true; check(bool(session.commit(session.snapshot(), edited)));
        const auto replacement = session.snapshot(); waitFor(window, [&] { gate->release.release(); });
        check(dialog->isVisible() && session.snapshot() == replacement); dialog->reject(); gate->block = false;
        step("queued cancellation");
        options->trigger(); gate->holdQueued = true; gate->queued = false; const auto queuedSource = session.snapshot();
        buttons->button(QDialogButtonBox::Ok)->click(); waitUntil(gate->queued);
        check(session.snapshot() == queuedSource); waitFor(window, [&] { dialog->reject(); gate->releaseQueued.release(); });
        check(session.snapshot() == queuedSource && !dialog->isVisible()); gate->holdQueued = false;
        step("confirmed close");
        options->trigger(); gate->block = true; gate->entered = false; const auto closeSource = session.snapshot();
        buttons->button(QDialogButtonBox::Ok)->click(); waitUntil(gate->entered);
        QTimer::singleShot(0, [&] {
            for (auto *w : QApplication::topLevelWidgets())
                if (auto *box = qobject_cast<QMessageBox *>(w); box && box->isVisible()) box->button(QMessageBox::Yes)->click();
        });
        window.close(); check(window.isClosing()); waitFor(window, [&] { gate->release.release(); });
        check(session.snapshot() == closeSource && !dialog->isVisible());
        check(unexpectedModal.isEmpty());
        std::cout << "PASS: initial Options tabs, read-only candidates without plot changes, routing, cancellation/stale/queued/close publication\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
