#include "classic_fixture.h"
#include "gui/mainwindow.h"
#include "gui/dialogs/initial_peak_options_dialog.h"
#include "gui/dialogs/pfsm_options_dialog.h"
#include "gui/plotting/qt_plot_sink.h"
#include "tests/support/workspace_fixture.h"
#include <QApplication>
#include <QAbstractButton>
#include <QDir>
#include <QEventLoop>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <semaphore>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
using qualpf::gui::SearchTaskController;
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct Gate {
    std::binary_semaphore entered{0}, release{0};
    bool queued = false, empty = false;
    std::atomic<int> opens = 0;
};
class Reader final : public CandidateReader {
    std::shared_ptr<Gate> gate_;

  public:
    explicit Reader(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                          SearchProgress) override {
        if (!gate_->queued) {
            gate_->entered.release();
            gate_->release.acquire();
        }
        if (gate_->empty)
            return Result<std::vector<ProductCard>>{std::vector<ProductCard>{}};
        ProductCard card;
        card.id = "42";
        return Result<std::vector<ProductCard>>{std::vector<ProductCard>{std::move(card)}};
    }
};
class Factory final : public CandidateReaderFactory {
    std::shared_ptr<Gate> gate_;

  public:
    explicit Factory(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    Result<std::unique_ptr<CandidateReader>> open(const ReferenceDatabase &, std::uint64_t,
                                                  const ExecutionContext &) override {
        ++gate_->opens;
        return Result<std::unique_ptr<CandidateReader>>{std::make_unique<Reader>(gate_)};
    }
};
static void answer(QMessageBox::StandardButton button) {
    QTimer::singleShot(0, [button] {
        for (auto *widget : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(widget); box && box->isVisible())
                box->button(button)->click();
    });
}
static QAction *action(MainWindow &window, const char *name) {
    auto *result = window.findChild<QAction *>(QString::fromLatin1(name));
    check(result, "menu action exists");
    return result;
}
static void initialCommandsFollowAdmission(MainWindow &window) {
    const auto source = AppState::workspace().session().snapshot();
    const bool available = action(window, "actionInitialPeakSearchOptions")->isEnabled() &&
        !window.searchTasks()->busy() && !source->editing && !source->pattern.observed.empty() &&
        !source->pattern.radiation.lines.empty();
    check(action(window, "actionInitialPeakSearch")->isEnabled() == available, "initial peak execution admission");
    check(action(window, "actionPFSM")->isEnabled() == action(window, "actionSearch_Match")->isEnabled(), "CSM/PFSM execution admission");
}
static void peakCommandsEnabled(MainWindow &window, bool enabled) {
    for (const auto *name : {"actionPeak_Search", "actionPeak_Search_Conditions", "actionAdd_Peak",
                            "actionAddInferredPeak", "actionDelete_Peak", "actionDeleteAllPeaks",
                            "actionLoad_Peaks", "actionSave_Peaks", "actionFwhmEstimation", "actionFwhmEstimationOptions",
                            "actionInitialPeakSearchOptions"})
        check(action(window, name)->isEnabled() == enabled, "independent peak menu admission");
    initialCommandsFollowAdmission(window); // Unlike options, execution also requires a wavelength.
}
static void editableOptionsCancel(MainWindow &window) {
    const auto source = AppState::workspace().session().snapshot();
    const auto title = window.windowTitle();
    auto *options = action(window, "actionInitialPeakSearchOptions");
    check(options->isEnabled(), "idle initial options entry is available");
    options->trigger();
    auto *initial = window.findChild<InitialPeakOptionsDialog *>();
    check(initial && initial->isVisible(), "initial options are editable");
    initial->reject();
    check(AppState::workspace().session().snapshot() == source && window.windowTitle() == title,
          "canceled initial options preserve canonical state and dirty status");
    action(window, "actionPFSMOptions")->trigger();
    auto *pfsm = window.findChild<PfsmOptionsDialog *>();
    check(pfsm && pfsm->isVisible(), "PFSM options are editable");
    pfsm->reject();
    check(AppState::workspace().session().snapshot() == source && window.windowTitle() == title,
          "canceled PFSM options preserve canonical state");
}
static void captureMenus(MainWindow &window, const QString &directory) {
    check(QDir().mkpath(directory), "menu capture directory");
    window.resize(1100, 800);
    auto *menuBar = window.menuBar();
    const int barWidth = menuBar->actionGeometry(menuBar->actions().last()).right() + 8;
    const auto bar = menuBar->grab(QRect(0, 0, barWidth, menuBar->height()));
    QList<QPixmap> menus;
    int width = 0, height = 0;
    for (const auto *name : {"menuPattern", "menuPeakSearch", "menuDatabase"}) {
        auto *menu = window.findChild<QMenu *>(QString::fromLatin1(name));
        check(menu, "menu capture owner");
        menu->popup(window.menuBar()->mapToGlobal(window.menuBar()->actionGeometry(menu->menuAction()).bottomLeft()));
        auto pixels = menu->grab();
        menu->hide();
        check(pixels.save(directory + '/' + QString::fromLatin1(name) + ".png"), "native menu capture");
        width += pixels.width() + 16;
        height = std::max(height, pixels.height());
        menus.append(std::move(pixels));
    }
    QPixmap overview(std::max(width, bar.width()), bar.height() + height + 16);
    overview.fill(window.palette().color(QPalette::Window));
    QPainter painter(&overview);
    painter.drawPixmap(0, 0, bar);
    int x = 0;
    for (const auto &menu : menus) {
        painter.drawPixmap(x, bar.height() + 16, menu);
        x += menu.width() + 16;
    }
    painter.end();
    check(overview.save(directory + "/menus.png"), "menu overview capture");
}
static void completionRestoresMenus(MainWindow &window, const std::shared_ptr<Gate> &gate) {
    const bool queuedClose = gate->queued;
    gate->queued = false;
    gate->empty = true;
    for (bool cancel : {false, true}) {
        const auto source = AppState::workspace().session().snapshot();
        QEventLoop loop;
        bool success = false;
        QObject::connect(window.searchTasks(), &SearchTaskController::finished, &loop,
                         [&](quint64, bool result) { success = result; loop.quit(); });
        window.runSearch(SearchOptions{});
        gate->entered.acquire();
        peakCommandsEnabled(window, false);
        if (cancel) window.searchTasks()->cancel();
        gate->release.release();
        loop.exec();
        check(success == !cancel && !window.searchTasks()->busy(), "completed/cancelled worker retires");
        if (cancel)
            check(AppState::workspace().session().snapshot() == source, "cancelled query cannot publish");
        peakCommandsEnabled(window, true);
        initialCommandsFollowAdmission(window);
    }
    gate->queued = queuedClose;
    gate->empty = false;
}
int main(int argc, char **argv) try {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    check(directory.isValid(), "settings fixture");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    app.setOrganizationName("QualPF-tests");
    app.setApplicationName("window-lifecycle");
    testing::ClassicFixture fixture;
    auto source = AppState::workspace().session().snapshot();
    auto next = *source;
    next.pattern = fixture.pattern;
    check(bool(AppState::workspace().session().commit(source, std::move(next))), "initial pattern");
    auto gate = std::make_shared<Gate>();
    gate->queued = argc > 1 && std::string(argv[1]) == "queued";
    MainWindow window(AppState::context(), nullptr, std::make_shared<Factory>(gate), [gate](auto event, auto, auto) {
        if (gate->queued && event == SearchTaskController::TaskEvent::completion_queued) {
            gate->entered.release();
            gate->release.acquire();
        }
    });
    window.show();
    peakCommandsEnabled(window, false);
    initialCommandsFollowAdmission(window);
    window.enableActions(MainWindow::PatternAction);
    peakCommandsEnabled(window, true);
    window.saveEnabledActions();
    window.enableMenu(window.findChild<QMenu *>("menuDatabase"), true);
    initialCommandsFollowAdmission(window);
    window.restoreEnabledActions();
    editableOptionsCancel(window);
    window.saveEnabledActions();
    window.enableActions(MainWindow::RunAction);
    peakCommandsEnabled(window, false);
    window.restoreEnabledActions();
    peakCommandsEnabled(window, true);
    initialCommandsFollowAdmission(window);
    if (argc > 2 && std::string(argv[1]) == "capture") {
        captureMenus(window, QString::fromLocal8Bit(argv[2]));
        std::cout << "PASS: real Qt menu captures and read-only Options\n";
        return 0;
    }
    completionRestoresMenus(window, gate);
    auto *background = window.findChild<BackgroundDialog *>();
    background->setBackground();
    background->show();
    const auto editing = AppState::workspace().session().snapshot();
    check(editing->editing, "active child draft");
    peakCommandsEnabled(window, false);
    check(!action(window, "actionPFSMOptions")->isEnabled(), "editing blocks reserved Options");
    answer(QMessageBox::No);
    window.close();
    check(!window.isClosing() && background->isVisible() &&
              AppState::workspace().session().snapshot() == editing,
          "refused close changed active child");
    if (argc > 1 && std::string(argv[1]) == "child") {
        answer(QMessageBox::Yes); window.close();
        const auto closed = AppState::workspace().session().snapshot();
        check(window.isClosing() && !window.isVisible() && !closed->editing &&
            closed->pattern.observed == editing->pattern.observed && closed->pattern.two_theta == editing->pattern.two_theta,
            "confirmed close did not discard private child draft");
        std::cout << "PASS: confirmed idle close discards active child without numerical publication\n";
        return 0;
    }
    static_cast<QDialog *>(background)->reject();
    peakCommandsEnabled(window, true);
    initialCommandsFollowAdmission(window);
    source = AppState::workspace().session().snapshot();
    const auto previousOpens = gate->opens.load();
    window.runSearch(SearchOptions{});
    gate->entered.acquire();
    const auto task = window.searchTasks()->taskId();
    window.restoreEnabledActions();
    peakCommandsEnabled(window, false);
    initialCommandsFollowAdmission(window);
    window.runSearch(SearchOptions{});
    background->setBackground();
    const bool busyPreserved = window.searchTasks()->taskId() == task && gate->opens == previousOpens + 1 &&
                               AppState::workspace().session().snapshot() == source;
    answer(QMessageBox::No);
    window.close();
    const bool refused =
        window.searchTasks()->busy() && !window.isClosing() && !AppState::workspace().searches().cancelled();
    peakCommandsEnabled(window, false);
    answer(QMessageBox::Yes);
    window.close();
    const bool closing = window.isClosing() && window.searchTasks()->busy() &&
                         AppState::workspace().searches().closing() && !background->isVisible();
    window.runSearch(SearchOptions{});
    window.restoreEnabledActions();
    bool disabled = true;
    for (auto *action : window.findChildren<QAction *>())
        disabled &= !action->isEnabled();
    const auto plotClosed = qualpf::gui::QtPlotSink(window.xpdViewer()).apply({});
    QEventLoop loop;
    QObject::connect(window.searchTasks(), &SearchTaskController::drained, &loop,
                     [&] { QTimer::singleShot(0, &loop, &QEventLoop::quit); });
    gate->release.release();
    loop.exec();
    check(busyPreserved && refused && closing && disabled, "admission/close/action barrier");
    check(!window.searchTasks()->busy() && !window.isVisible() &&
              AppState::workspace().session().snapshot() == source,
          "late publication or unclosed window");
    check(!plotClosed && plotClosed.error().reason == "workspace_closing", "late plot guard");
    std::cout << "PASS: active-child refused close, Busy, blocked/queued worker confirmed close, event-loop "
                 "drain, no late commit/plot/actions\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
