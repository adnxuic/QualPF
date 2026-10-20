#include "gui/mainwindow.h"
#include "gui/dialogs/fwhm_options_dialog.h"
#include "tests/support/workspace_fixture.h"
#include <QApplication>
#include <QAbstractButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QEventLoop>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
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
static void check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("FWHM GUI at line " + std::to_string(at.line()));
}
struct Gate { std::atomic<bool> block = false, entered = false; std::binary_semaphore release{0}; };
struct Initial final : preprocessing::IInitialPeakFinder {
    mutable std::atomic<int> calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.initial.cpp", 5}; }
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &spectrum, WavelengthAngstrom,
        const preprocessing::InitialPeakOptions &, const ExecutionContext &) const override {
        const auto &p = spectrum.pattern();
        ++calls; preprocessing::InitialPeakResult r; r.dataset = p.id; r.candidates = {{200}};
        return Result<preprocessing::InitialPeakResult>{r};
    }
};
struct Gaussian final : preprocessing::IFwhmEstimator {
    mutable std::atomic<int> calls = 0;
    std::shared_ptr<Gate> gate;
    std::thread::id owner = std::this_thread::get_id();
    mutable bool worker = false;
    explicit Gaussian(std::shared_ptr<Gate> g) : gate(std::move(g)) {}
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.fwhm.cpp", 3}; }
    Result<preprocessing::FwhmEstimationResult> estimate(const AnalysisSpectrum &spectrum, const preprocessing::InitialPeakResult &,
        const ExecutionContext &) const override {
        ++calls; worker = std::this_thread::get_id() != owner;
        if (gate->block) { gate->entered = true; gate->release.acquire(); }
        return Result<preprocessing::FwhmEstimationResult>{preprocessing::FwhmEstimationResult{
            {preprocessing::FwhmEstimate::State::measured, AngularWidthDegrees{double(.2f)}, 1}, {}}};
    }
};
struct Formal final : preprocessing::IExperimentalPeakExtractor {
    mutable int calls = 0;
    mutable preprocessing::PeakCalibration last;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.experimental.cpp", 3}; }
    Result<preprocessing::PeakSearchResult> extract(const AnalysisSpectrum &spectrum, const preprocessing::PeakCalibration &c,
        WavelengthAngstrom wave, AngularWidthDegrees, const ExecutionContext &) const override {
        const auto &p = spectrum.pattern();
        ++calls; last = c;
        preprocessing::PeakSearchResult r; r.applied = c.applied;
        r.peaks.dataset = r.candidates.dataset = p.id;
        r.peaks.effective_wavelength = r.candidates.effective_wavelength = wave;
        Peak peak; peak.id = {1}; peak.two_theta = TwoThetaDegrees{20.4}; peak.intensity = 20;
        peak.original_intensity = 20; peak.d_spacing = DSpacingAngstrom{1.54 / (2 * std::sin(20.4 * 3.14159265358979323846 / 360))};
        peak.delta_d = DSpacingAngstrom{.01}; peak.fwhm = AngularWidthDegrees{.2}; peak.integrated_intensity = 1;
        r.peaks.peaks = r.candidates.peaks = {peak}; r.automatic_tolerance = .5;
        return Result<preprocessing::PeakSearchResult>{r};
    }
};
struct EmptyReader final : CandidateReader {
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        return Result<std::vector<ProductCard>>{std::vector<ProductCard>{}};
    }
};
struct Factory final : CandidateReaderFactory {
    Result<std::unique_ptr<CandidateReader>> open(const ReferenceDatabase &, std::uint64_t, const ExecutionContext &) override {
        return Result<std::unique_ptr<CandidateReader>>{std::make_unique<EmptyReader>()};
    }
};
static ProductState state(AlgorithmHandle algorithms) {
    ProductState s; s.algorithms = std::move(algorithms); auto &p = s.pattern;
    p.id = {101}; p.source_id = "gui-triangle"; p.radiation.lines = {{WavelengthAngstrom{1.54}, 1}};
    p.processing.background_subtracted = false; p.processing.intensity_scaled = false; p.processing.kalpha2_subtracted = false;
    for (int i = 0; i <= 400; ++i) { p.two_theta.emplace_back(20 + .002 * i); p.observed.push_back(120 - .2 * std::abs(i - 200)); }
    p.original_observed = p.observed; p.background = SampledSignal{std::vector<double>(401, 100), SampleWindow{0}};
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
static void waitEntered(const std::shared_ptr<Gate> &gate) {
    QEventLoop loop; QTimer poll, timeout; bool timedOut = false;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (gate->entered) loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { timedOut = true; loop.quit(); });
    poll.start(1); timeout.setSingleShot(true); timeout.start(10000); if (!gate->entered) loop.exec();
    check(!timedOut && gate->entered);
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        QTemporaryDir temporary; check(temporary.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
        app.setOrganizationName("QualPF-tests"); app.setApplicationName("fwhm-options");
        auto gate = std::make_shared<Gate>(); auto initial = std::make_shared<Initial>();
        auto gaussian = std::make_shared<Gaussian>(gate); auto formal = std::make_shared<Formal>();
        AlgorithmModules modules; modules.initial_peaks = initial; modules.fwhm = gaussian; modules.experimental_peaks = formal;
        auto resolved = resolveAlgorithms(AlgorithmSpec{}, modules); check(bool(resolved));
        auto &session = AppState::workspace().session(); check(bool(session.commit(session.snapshot(), state(resolved.value()))));
        MainWindow window(AppState::context(), nullptr, std::make_shared<Factory>()); window.show();
        window.enableActions(MainWindow::PatternAction);
        auto *options = widget<QAction>(window, "actionFwhmEstimationOptions");
        auto *estimate = widget<QAction>(window, "actionFwhmEstimation");
        auto *dialog = widget<FwhmOptionsDialog>(window, "fwhmOptionsDialog");
        auto *algorithm = widget<QComboBox>(*dialog, "fwhmAlgorithmComboBox");
        auto *fixed = widget<QCheckBox>(*dialog, "useFixedFwhmCheckBox");
        auto *width = widget<QLineEdit>(*dialog, "defaultFwhmEdit");
        auto *slider = widget<QSlider>(*dialog, "fwhmSamplingSlider");
        auto *spin = widget<QSpinBox>(*dialog, "fwhmSamplingSpinBox");
        auto *status = widget<QLabel>(*dialog, "fwhmStatus");
        auto *buttons = widget<QDialogButtonBox>(*dialog, "buttonBox");
        QString capture;
        if (argc == 3 && std::string(argv[1]) == "--capture") { capture = QString::fromLocal8Bit(argv[2]); check(QDir().mkpath(capture)); }
        const auto before = session.snapshot();
        check(options->isEnabled() && estimate->isEnabled());
        const auto menuActions = widget<QMenu>(window, "menuPattern")->actions();
        const int position = menuActions.indexOf(estimate);
        check(position > 0 && menuActions[position - 1]->isSeparator() && menuActions[position + 1] == options && menuActions[position + 2]->isSeparator());
        if (!capture.isEmpty()) { auto *menu = widget<QMenu>(window, "menuPattern"); menu->popup(window.pos()); check(menu->grab().save(capture + "/preprocessing-menu.png")); menu->hide(); }
        options->trigger();
        check(dialog->isVisible() && algorithm->currentIndex() == 0 && !fixed->isChecked() && width->isReadOnly());
        check(width->text().toDouble() == .1 && !widget<QWidget>(*dialog, "fwhmSamplingPanel")->isVisible());
        check(status->text().contains("not yet estimated") && session.snapshot() == before);
        check(widget<QFrame>(*dialog, "fwhmAlgorithmSeparator")->frameShape() == QFrame::HLine);
        check(widget<QFrame>(*dialog, "fwhmButtonSeparator")->frameShape() == QFrame::HLine);
        if (!capture.isEmpty()) check(dialog->grab().save(capture + "/fwhm-gaussian.png"));
        algorithm->setCurrentIndex(1); check(widget<QWidget>(*dialog, "fwhmSamplingPanel")->isVisible());
        check(spin->minimum() == 2 && spin->maximum() == 8 && spin->value() == 6);
        slider->setValue(2); check(spin->value() == 2); spin->setValue(8); check(slider->value() == 8); spin->setValue(6);
        if (!capture.isEmpty()) check(dialog->grab().save(capture + "/fwhm-geometric.png"));
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        auto measured = session.snapshot(); check(measured != before && !dialog->isVisible());
        check(measured->preprocessing.fwhm->method == FwhmMethod::geometric && measured->preprocessing.fwhm_usage->current);
        check(measured->preprocessing.fwhm_usage->manual && measured->preprocessing.fwhm_usage->source_pattern);
        // Analytic triangle: apex 20 at 20.4, half-height crossings 20.3 and 20.5.
        check(std::abs(measured->preprocessing.fwhm->default_fwhm.value - .2) < 1e-12);
        check(measured->pattern.observed == before->pattern.observed && !measured->peaks.valid);
        check(initial->calls == 0 && gaussian->calls == 0 && estimate->isEnabled());
        waitFor(window, [&] { estimate->trigger(); }); check(session.snapshot() != measured && initial->calls == 0);
        measured = session.snapshot();
        widget<QAction>(window, "actionPeak_Search")->trigger();
        check(formal->calls == 1 && session.snapshot()->preprocessing.fwhm_usage->source_revision == measured->revision);
        check(formal->last.source == preprocessing::PeakCalibration::Source::geometric_fwhm);
        check(std::abs(session.snapshot()->preprocessing.fwhm->default_fwhm.value - .2) < 1e-12);
        options->trigger(); fixed->setChecked(true);
        check(!width->isReadOnly() && !algorithm->isEnabled() && !slider->isEnabled());
        width->setText("0"); measured = session.snapshot(); buttons->button(QDialogButtonBox::Ok)->click();
        check(!window.searchTasks()->busy() && dialog->isVisible() && session.snapshot() == measured);
        constexpr double manualWidth = .12345678912345;
        width->setText("0.12345678912345");
        check(status->text().contains("estimation is skipped"));
        if (!capture.isEmpty()) check(dialog->grab().save(capture + "/fwhm-fixed.png"));
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(session.snapshot()->preprocessing.fwhm->default_fwhm.value == manualWidth && !estimate->isEnabled());
        check(gaussian->calls == 0 && initial->calls == 0);
        check(formal->calls == 1 && session.snapshot()->peaks.valid);
        check(session.snapshot()->preprocessing.peak_fwhm_usage->origin == FwhmOrigin::geometric);
        options->trigger(); waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(session.snapshot()->preprocessing.fwhm->default_fwhm.value == manualWidth); // Formatting must not round it back.
        widget<QAction>(window, "actionPeak_Search")->trigger();
        check(formal->calls == 2 && formal->last.applied.search_range == .9f * float(manualWidth));
        check(initial->calls == 0 && gaussian->calls == 0 && session.snapshot()->peaks.valid);
        widget<QAction>(window, "actionPeak_Search_Conditions")->trigger();
        auto *peakOptions = window.findChild<PeakSearchDialog *>(); check(peakOptions && peakOptions->isVisible());
        const int formalCalls = formal->calls;
        widget<QSlider>(*peakOptions, "intensitySlider")->setValue(25);
        check(formal->calls == formalCalls && gaussian->calls == 0);
        auto *sensitivity = widget<QSpinBox>(*peakOptions, "sensitivitySpinBox");
        sensitivity->setValue(sensitivity->value() == 50 ? 51 : 50);
        check(formal->calls == formalCalls + 1 && initial->calls == 0 && gaussian->calls == 0);
        static_cast<QDialog *>(peakOptions)->reject();
        auto noPeaks = *session.snapshot(); noPeaks.peaks.clear(); noPeaks.preprocessing.peak_candidates.reset();
        noPeaks.preprocessing.rejected_candidates.clear(); noPeaks.provenance.peakSource.reset();
        check(bool(session.commit(session.snapshot(), std::move(noPeaks))));
        const int beforeMatch = formal->calls;
        waitFor(window, [&] { window.runSearchMatch(); });
        check(formal->calls == beforeMatch + 1 && initial->calls == 0 && gaussian->calls == 0 && session.snapshot()->peaks.valid);
        auto flat = state(resolved.value()); flat.pattern.observed.assign(401, 100);
        flat.preprocessing.fwhm = FwhmConfiguration{}; flat.preprocessing.fwhm->method = FwhmMethod::geometric;
        check(bool(session.commit(session.snapshot(), flat))); window.enableActions(MainWindow::PatternAction);
        options->trigger(); const auto failedSource = session.snapshot();
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(dialog->isVisible() && session.snapshot() == failedSource && status->text().contains("No eligible peak"));
        check(!options->isEnabled() && !estimate->isEnabled());
        fixed->setChecked(true); width->setText("0.15");
        waitFor(window, [&] { buttons->button(QDialogButtonBox::Ok)->click(); });
        check(!dialog->isVisible() && session.snapshot()->preprocessing.fwhm->default_fwhm.value == .15);
        options->trigger(); const auto defaultsSource = session.snapshot();
        buttons->button(QDialogButtonBox::RestoreDefaults)->click();
        check(algorithm->currentIndex() == 0 && !fixed->isChecked() && width->isReadOnly() && width->text().toDouble() == .1 && spin->value() == 6);
        dialog->reject(); check(session.snapshot() == defaultsSource);
        check(bool(session.commit(session.snapshot(), state(resolved.value())))); window.enableActions(MainWindow::PatternAction);
        options->trigger(); gate->block = true; gate->entered = false;
        const auto canceledSource = session.snapshot(); buttons->button(QDialogButtonBox::Ok)->click(); waitEntered(gate);
        check(window.searchTasks()->busy() && !buttons->button(QDialogButtonBox::Ok)->isEnabled());
        waitFor(window, [&] { dialog->reject(); gate->release.release(); });
        check(session.snapshot() == canceledSource && !dialog->isVisible() && estimate->isEnabled());
        options->trigger(); gate->entered = false;
        buttons->button(QDialogButtonBox::Ok)->click(); waitEntered(gate);
        auto replacement = *session.snapshot(); replacement.projectMetadata["fwhm_stale_probe"] = true;
        check(bool(session.commit(session.snapshot(), std::move(replacement))));
        const auto newSource = session.snapshot();
        waitFor(window, [&] { gate->release.release(); });
        check(session.snapshot() == newSource && dialog->isVisible() && !status->text().isEmpty());
        check(!newSource->preprocessing.fwhm_usage);
        dialog->reject();
        gate->block = false;
        waitFor(window, [&] { estimate->trigger(); });
        check(gaussian->worker && session.snapshot()->preprocessing.fwhm_usage->origin == FwhmOrigin::gaussian);
        check(session.snapshot()->preprocessing.fwhm->default_fwhm.value == double(.2f));
        options->trigger(); gate->block = true; gate->entered = false;
        const auto closeSource = session.snapshot(); buttons->button(QDialogButtonBox::Ok)->click(); waitEntered(gate);
        QTimer::singleShot(0, [&] {
            for (auto *widget : QApplication::topLevelWidgets())
                if (auto *box = qobject_cast<QMessageBox *>(widget); box && box->isVisible()) box->button(QMessageBox::Yes)->click();
        });
        window.close(); check(window.isClosing());
        waitFor(window, [&] { gate->release.release(); });
        check(session.snapshot() == closeSource && !dialog->isVisible());
        std::cout << "FWHM GUI controls, menu groups, worker execution/cancellation/close and all redetection routes passed\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
