#include "classic_fixture.h"
#include "gui/mainwindow.h"
#include "gui/plotting/qt_plot_sink.h"
#include "tests/support/workspace_fixture.h"
#include <QApplication>
#include <QAbstractButton>
#include <QEventLoop>
#include <QMessageBox>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
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
    bool queued = false;
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
    auto *background = window.findChild<BackgroundDialog *>();
    background->setBackground();
    background->show();
    const auto editing = AppState::workspace().session().snapshot();
    check(editing->editing, "active child draft");
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
    source = AppState::workspace().session().snapshot();
    window.runSearch(SearchOptions{});
    gate->entered.acquire();
    const auto task = window.searchTasks()->taskId();
    window.runSearch(SearchOptions{});
    background->setBackground();
    const bool busyPreserved = window.searchTasks()->taskId() == task && gate->opens == 1 &&
                               AppState::workspace().session().snapshot() == source;
    answer(QMessageBox::No);
    window.close();
    const bool refused =
        window.searchTasks()->busy() && !window.isClosing() && !AppState::workspace().searches().cancelled();
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
