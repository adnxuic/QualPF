#include "classic_fixture.h"
#include "gui/search/search_task_controller.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <atomic>
#include <bit>
#include <cfenv>
#include <iostream>
#include <semaphore>
#include <stdexcept>
#include <thread>

using namespace qualpf;
using namespace qualpf::application;
using qualpf::gui::SearchTaskController;
static void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
struct Gate {
    std::binary_semaphore entered{0}, release{0};
    std::thread::id opened, read, destroyed;
    bool block = true, fail = false, throwing = false;
    std::vector<ProductCard> cards;
};
class Reader final : public CandidateReader {
    std::shared_ptr<Gate> gate_;

  public:
    explicit Reader(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    ~Reader() { gate_->destroyed = std::this_thread::get_id(); }
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                          SearchProgress progress) override {
        gate_->read = std::this_thread::get_id();
        if (progress)
            for (std::size_t n = 0; n < 10000; ++n)
                progress(n, 10000);
        if (gate_->block) {
            gate_->entered.release();
            gate_->release.acquire();
        }
        if (gate_->throwing)
            throw std::runtime_error("controlled query exception");
        if (gate_->fail)
            return Result<std::vector<ProductCard>>{Error{ErrorCode::missing_data, "reader", "failed"}};
        return Result<std::vector<ProductCard>>{gate_->cards};
    }
};
class Factory final : public CandidateReaderFactory {
    std::shared_ptr<Gate> gate_;

  public:
    int protectedCalls = 0;
    explicit Factory(std::shared_ptr<Gate> gate) : gate_(std::move(gate)) {}
    Result<std::shared_ptr<const ReferenceReadGuard>> protect(const ReferenceDatabase &) override {
        ++protectedCalls;
        return Result<std::shared_ptr<const ReferenceReadGuard>>{std::shared_ptr<const ReferenceReadGuard>{}};
    }
    Result<std::unique_ptr<CandidateReader>> open(const ReferenceDatabase &, std::uint64_t,
                                                  const ExecutionContext &) override {
        gate_->opened = std::this_thread::get_id();
        return Result<std::unique_ptr<CandidateReader>>{std::make_unique<Reader>(gate_)};
    }
};
class WorkerMatcher final : public search::IClassicMatcher {
  public:
    mutable std::atomic<int> calls{0};
    AlgorithmIdentity identity() const override { return {"test.worker.matcher", 1}; }
    AlgorithmIdentity produces() const override { return search::classic_match_contract(); }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &input, const ExecutionContext &context) const override {
        ++calls;
        auto result = search::ClassicMatcher{}.match(input, context);
        if (result) result.value().position_difference_sum = .375f;
        return result;
    }
};
static void waitFinished(SearchTaskController &controller) {
    if (!controller.busy())
        return;
    QEventLoop loop;
    QTimer watchdog;
    watchdog.setSingleShot(true);
    QObject::connect(&controller, &SearchTaskController::finished, &loop, &QEventLoop::quit);
    QObject::connect(&watchdog, &QTimer::timeout, &loop, &QEventLoop::quit);
    watchdog.start(10000);
    loop.exec();
    check(!controller.busy(), "controlled completion did not drain");
}
static ProductState state(const testing::ClassicFixture &f) {
    ProductState next;
    next.pattern = f.pattern;
    next.peaks.valid = true;
    next.peaks.wave = 1.54;
    next.peaks.integratedIntensity.emplace();
    for (const auto &p : f.peaks.peaks) {
        next.peaks.tth.push_back(p.two_theta.value);
        next.peaks.intensity.push_back(p.intensity);
        next.peaks.intensityOrig.push_back(*p.original_intensity);
        next.peaks.d.push_back(p.d_spacing->value);
        next.peaks.deltaD.push_back(p.delta_d->value);
        next.peaks.fwhm.push_back(p.fwhm->value);
        next.peaks.integratedIntensity->push_back(*p.integrated_intensity);
    }
    return next;
}
static ProductCard card(const testing::ClassicFixture &f) {
    ProductCard next;
    next.id = "A";
    for (const auto &r : f.card.reflections) {
        next.tth.push_back(r.two_theta->value);
        next.intensity.push_back(r.intensity);
    }
    return next;
}
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    const auto owner = std::this_thread::get_id();
    testing::ClassicFixture fixture;
    {
        auto unprepared = state(fixture); unprepared.peaks.clear();
        ProductSearchSession session(unprepared);
        SearchCoordinator coordinator(session);
        auto gate = std::make_shared<Gate>();
        auto factory = std::make_shared<Factory>(gate);
        SearchTaskController controller(coordinator, factory);
        const auto source = session.snapshot();
        SearchInput input;
        input.source = source;
        input.method = SearchMethod::pfsm;
        bool published = false;
        auto refused = controller.start(input, {}, [&](auto) { published = true; });
        check(!refused && refused.error().reason == "algorithm_provenance_mismatch" && !coordinator.running() &&
                  factory->protectedCalls == 0 && gate->opened == std::thread::id{} && !published &&
                  session.snapshot() == source,
              "Method mismatch precedes protection, worker/SQL and scientific publication");
    }
    {
        auto fom = std::make_shared<WorkerMatcher>();
        AlgorithmSelections selections;
        selections.matcher = fom->identity();
        AlgorithmModules modules;
        modules.matcher = fom;
        auto algorithms = resolveAlgorithms(AlgorithmSpec{selections}, std::move(modules));
        check(bool(algorithms), "worker algorithm assembly");
        auto initial = state(fixture);
        initial.algorithms = algorithms.value();
        ProductSearchSession session(std::move(initial));
        SearchCoordinator coordinator(session);
        auto gate = std::make_shared<Gate>();
        gate->block = false;
        gate->cards = {card(fixture)};
        SearchTaskController controller(coordinator, std::make_shared<Factory>(gate));
        SearchInput input;
        input.parameters = fixture.parameters;
        check(bool(controller.start(input, {"injected-reference", 9})), "injected worker starts");
        waitFinished(controller);
        check(fom->calls == 1 && session.snapshot()->candidates.size() == 1 &&
                  session.snapshot()->candidates[0].fomPeakPos == .5 && session.snapshot()->candidates[0].scale == 10 &&
                  session.snapshot()->candidates[0].fom < 1,
              "actual worker consumes frozen matching evidence through fixed Classical FOM");
    }
    for (int mode = 0; mode < 8; ++mode) {
        ProductSearchSession session(state(fixture));
        SearchCoordinator coordinator(session);
        auto source = session.snapshot();
        auto gate = std::make_shared<Gate>();
        gate->cards = {card(fixture)};
        SearchTaskController controller(coordinator, std::make_shared<Factory>(gate));
        SearchInput input;
        input.parameters = fixture.parameters;
        int notices = 0, progress = 0, modules = 0;
        QObject::connect(&controller, &SearchTaskController::workflowProgress, [&](auto id) {
            check(id==controller.taskId() && std::this_thread::get_id()==owner && coordinator.activity() && !coordinator.activity()->detail.empty(), "foreign or empty module progress");
            ++modules;
        });
        std::optional<SearchTaskController::Outcome> outcome;
        auto subscription = session.subscribe([&](auto) {
            check(std::this_thread::get_id() == owner, "commit escaped owner thread");
            ++notices;
        });
        QObject::connect(&controller, &SearchTaskController::progress, [&](auto id, auto, auto) {
            check(id == controller.taskId() && std::this_thread::get_id() == owner, "foreign progress");
            ++progress;
        });
        check(bool(controller.start(input, {"frozen-reference", 7},
                                    [&](auto result) {
                                        check(std::this_thread::get_id() == owner, "foreign result delivery");
                                        outcome.emplace(std::move(result));
                                    })),
              "start rejected");
        gate->entered.acquire();
        check(controller.busy() && coordinator.running(), "job left Busy while executing");
        auto busy = controller.start(input, {});
        check(!busy && busy.error().reason == "busy", "second job was queued/replaced");
        // A normal timer/input turn executes while the controlled worker is blocked.
        bool timer = false;
        QEventLoop loop;
        QTimer::singleShot(0, &loop, [&] {
            timer = true;
            loop.quit();
        });
        loop.exec();
        check(timer && controller.busy() && progress <= 1, "GUI responsiveness/coalescing failed");
        if (mode == 1 || mode == 2)
            controller.cancel();
        if (mode == 3) {
            auto next = *source;
            next.projectMetadata["edited"] = 1;
            check(bool(session.commit(source, std::move(next))), "controlled edit rejected");
        }
        if (mode == 4)
            coordinator.selectDatabase(8);
        if (mode == 5)
            gate->fail = true;
        if (mode == 6)
            gate->throwing = true;
        bool drained = false;
        QObject::connect(&controller, &SearchTaskController::drained, [&] { drained = true; });
        if (mode == 7)
            controller.close();
        gate->release.release();
        waitFinished(controller);
        check(gate->opened != owner && gate->opened == gate->read && gate->read == gate->destroyed,
              "reader open/use/remove thread ownership failed");
        check(!coordinator.running(), "admission not retired");
        const auto timing = controller.lastTiming();
        check(timing && timing->task != 0 && timing->queue_seconds >= 0 && timing->worker_seconds > 0 &&
                  timing->delivery_seconds >= 0 && timing->publication_seconds >= 0 &&
                  timing->presentation_seconds >= 0 && timing->total_seconds >= timing->worker_seconds &&
                  timing->success == (mode == 0), "worker/publication/presentation timing scope failed");
        if (mode == 0) {
            check(modules>=3,"search module stages were not delivered");
            check(outcome && bool(*outcome) && notices == 1, "successful result not atomically committed");
            check(session.snapshot()->candidates.front().fom == 1 &&
                      session.snapshot()->candidates.front().scale == 10,
                  "frozen fixture score changed");
        } else if (mode == 7) {
            check(drained && !outcome && notices == 0 && session.snapshot() == source,
                  "Closing published or failed to drain");
            check(!controller.start(input, {}), "Closing accepted new work");
        } else {
            check(outcome && !*outcome, "failed/canceled/stale job reported success");
            check(notices == (mode == 3 ? 1 : 0), "partial result notification");
            if (mode != 3)
                check(session.snapshot() == source, "partial result committed");
        }
    }
    // Cancel after completed work has queued delivery, before the owner dispatches it.
    {
        ProductSearchSession session;
        SearchCoordinator coordinator(session);
        std::binary_semaphore completed{0};
        SearchTaskController controller(coordinator, {}, nullptr, [&](auto event, auto, auto thread) {
            if (event == SearchTaskController::TaskEvent::completion_queued) {
                check(thread != owner, "completion queued by owner");
                completed.release();
            }
        });
        auto source = session.snapshot();
        bool canceled = false;
        check(bool(controller.submit(
                  {},
                  [&](const SearchTask &task, SearchProgress) {
                      PreparedSearchResult result{task.source, task.id, 0, *task.source};
                      result.next.projectMetadata["pending"] = 1;

                      return SearchTaskController::Outcome{std::move(result)};
                  },
                  [&](auto result) { canceled = !result && result.error().code == ErrorCode::cancelled; })),
              "queued cancellation start failed");
        completed.acquire();
        controller.cancel();
        waitFinished(controller);
        check(canceled && session.snapshot() == source, "queued completion ignored cancellation");
    }
    // Hold execution at worker entry, before the reader/engine is invoked.
    {
        ProductSearchSession session(state(fixture));
        SearchCoordinator coordinator(session);
        auto source = session.snapshot();
        auto gate = std::make_shared<Gate>();
        gate->block = false;
        SearchTaskController controller(coordinator, std::make_shared<Factory>(gate), nullptr,
                                        [&](auto event, auto, auto) {
                                            if (event == SearchTaskController::TaskEvent::worker_started) {
                                                gate->entered.release();
                                                gate->release.acquire();
                                            }
                                        });
        SearchInput input;
        input.parameters = fixture.parameters;
        check(bool(controller.start(input, {})), "pre-execution start failed");
        gate->entered.acquire();
        controller.cancel();
        gate->release.release();
        waitFinished(controller);
        check(session.snapshot() == source && gate->opened == std::thread::id{},
              "pre-execution cancellation read/published");
    }
    // The pooled thread inherits the caller's numerical environment each time.
    for (int rounding : {FE_TONEAREST, FE_DOWNWARD}) {
        std::fesetround(rounding);
        ProductSearchSession session(state(fixture));
        SearchCoordinator coordinator(session);
        auto gate = std::make_shared<Gate>();
        gate->block = false;
        gate->cards = {card(fixture)};
        Reader reader(gate);
        SearchInput input;
        input.source = session.snapshot();
        input.parameters = fixture.parameters;
        auto expected = SearchEngine{}.run(input, reader, fixture.context).value().next.candidates.front();
        SearchTaskController controller(coordinator, std::make_shared<Factory>(gate));
        check(bool(controller.start(input, {})), "parity start failed");
        waitFinished(controller);
        const auto actual = session.snapshot()->candidates.front();
        for (auto [a, b] : {std::pair{actual.fom, expected.fom}, std::pair{actual.scale, expected.scale},
                            std::pair{actual.fomPeakPos, expected.fomPeakPos},
                            std::pair{actual.fomIntensity, expected.fomIntensity}})
            check(std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b), "sync/worker FP drift");
        check(std::fegetround() == rounding, "worker altered owner rounding");
    }
    std::fesetround(FE_TONEAREST);
    std::cout
        << "PASS: controlled background scheduling, responsiveness, cancellation, stale, drain and parity\n";
    return 0;
} catch (const std::exception &exception) {
    std::cerr << exception.what() << '\n';
    return 1;
}
