#include "gui/search/search_task_controller.h"
#include <QPointer>
#include <QRunnable>
#include <QTimer>
#include <cfenv>
#include <exception>
#include <mutex>
#if defined(__SSE__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace qualpf::gui {
using namespace application;
namespace {
Error unexpected(const char *message) {
    return {ErrorCode::numerical_failure, "search.worker", message, "unexpected_exception"};
}
struct FloatingEnvironment {
    std::fenv_t value{};
#if defined(__SSE__) || defined(_M_X64)
    unsigned mxcsr = _mm_getcsr();
#endif
    FloatingEnvironment() { std::fegetenv(&value); }
    void apply() const {
        std::fesetenv(&value);
#if defined(__SSE__) || defined(_M_X64)
        _mm_setcsr(mxcsr);
#endif
    }
};
struct RestoreEnvironment {
    FloatingEnvironment previous;
    ~RestoreEnvironment() { previous.apply(); }
};
class UnusedReader final : public CandidateReader {
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                          SearchProgress) override {
        return Result<std::vector<ProductCard>>{
            Error{ErrorCode::missing_data, "search.reader", "No reader for this operation"}};
    }
};
} // namespace
struct SearchTaskController::Delivery {
    SearchTask task;
    std::shared_ptr<const ReferenceReadGuard> referenceGuard;
    Completion completion;
    Finalize finalize;
    SearchPublication publication = SearchPublication::commit;
    std::shared_ptr<QObject> endpoint;
    std::mutex mutex;
    std::optional<Outcome> result;
    std::size_t current = 0, total = 0;
    bool progressQueued = false;
};
SearchTaskController::SearchTaskController(SearchCoordinator &coordinator,
                                           std::shared_ptr<CandidateReaderFactory> factory, QObject *parent,
                                           Trace trace)
    : QObject(parent), coordinator_(coordinator), factory_(std::move(factory)), trace_(std::move(trace)) {
    pool_.setMaxThreadCount(1);
}
SearchTaskController::~SearchTaskController() {
    // Composition keeps this adapter alive until drained. The pool destructor
    // therefore never waits for a search while the GUI event loop is stopped.
    Q_ASSERT(!active_ && pool_.activeThreadCount() == 0);
}
std::uint64_t SearchTaskController::taskId() const noexcept { return active_ ? active_->task.id : 0; }
void SearchTaskController::cancel() { coordinator_.cancel(); }
void SearchTaskController::close() {
    coordinator_.close();
    if (!active_)
        emit drained();
}
Status SearchTaskController::start(SearchInput input, ReferenceDatabase database, Completion completion,
                                   Finalize finalize) {
    auto factory = factory_;
    std::shared_ptr<const ReferenceReadGuard> referenceGuard;
    const bool reads =
        input.operation == SearchOperation::initial || input.operation == SearchOperation::scored_browse ||
        input.operation == SearchOperation::metadata_browse || input.operation == SearchOperation::load_card;
    if (reads && factory) {
        auto protectedRead = factory->protect(database);
        if (!protectedRead)
            return Status{protectedRead.error()};
        referenceGuard = std::move(protectedRead).value();
    }
    return submit(
        std::move(database),
        [input = std::move(input), factory = std::move(factory)](const SearchTask &task,
                                                                 SearchProgress progress) mutable -> Outcome {
            if (input.source && input.source != task.source)
                return Outcome{Error{ErrorCode::stale_snapshot, "search.source", "Search input changed"}};
            input.source = task.source;
            input.taskId = task.id;
            input.databaseVersion = task.database.selectionVersion;
            if (task.context.cancellation.stop_requested())
                return Outcome{Error{ErrorCode::cancelled, "search", "Search canceled"}};
            const bool reads = input.operation == SearchOperation::initial ||
                               input.operation == SearchOperation::scored_browse ||
                               input.operation == SearchOperation::metadata_browse ||
                               input.operation == SearchOperation::load_card;
            if (!reads) {
                UnusedReader reader;
                return SearchEngine{}.run(input, reader, task.context, std::move(progress));
            }
            if (!factory)
                return Outcome{Error{ErrorCode::missing_data, "search.reader", "Missing reader factory"}};
            auto opened = factory->open(task.database, task.id, task.context);
            if (!opened)
                return Outcome{opened.error()};
            if (!opened.value())
                return Outcome{Error{ErrorCode::missing_data, "search.reader", "Missing reader"}};
            // The reader and every SQL handle are destroyed here, on this worker.
            return SearchEngine{}.run(input, *opened.value(), task.context, std::move(progress));
        },
        std::move(completion), std::move(finalize), SearchPublication::commit, std::move(referenceGuard));
}
Status SearchTaskController::submit(ReferenceDatabase database, Work work, Completion completion,
                                    Finalize finalize, SearchPublication publication,
                                    std::shared_ptr<const ReferenceReadGuard> referenceGuard) {
    if (QThread::currentThread() != thread())
        return Status{
            Error{ErrorCode::invalid_input, "search.thread", "Scheduling requires the owner thread"}};
    if (active_)
        return Status{Error{ErrorCode::invalid_input, "search", "A search is already running", "busy"}};
    if (!work)
        return Status{Error{ErrorCode::invalid_input, "search.worker", "Missing work"}};
    // Allocate the transport before occupying Application's admission slot.
    auto delivery = std::make_shared<Delivery>();
    delivery->completion = std::move(completion);
    delivery->finalize = std::move(finalize);
    delivery->publication = publication;
    delivery->referenceGuard = std::move(referenceGuard);
    delivery->endpoint =
        std::shared_ptr<QObject>(new QObject, [](QObject *object) { object->deleteLater(); });
    auto admission = coordinator_.begin(std::move(database));
    if (!admission)
        return Status{admission.error()};
    delivery->task = std::move(admission).value();
    active_ = delivery;
    const FloatingEnvironment environment;
    const QPointer<SearchTaskController> guard(this);
    const auto trace = trace_;
    try {
        pool_.start(
            QRunnable::create([delivery, work = std::move(work), environment, guard, trace]() mutable {
                RestoreEnvironment restore;
                environment.apply();
                auto progress = [delivery, guard](std::size_t current, std::size_t total) {
                    bool queue;
                    {
                        std::lock_guard lock(delivery->mutex);
                        delivery->current = current;
                        delivery->total = total;
                        queue = !delivery->progressQueued;
                        delivery->progressQueued = true;
                    }
                    if (queue)
                        QMetaObject::invokeMethod(
                            delivery->endpoint.get(),
                            [delivery, guard] {
                                if (guard)
                                    guard->deliverProgress(delivery);
                            },
                            Qt::QueuedConnection);
                };
                Outcome result{unexpected("Worker did not produce a result")};
                try {
                    if (trace) {
                        try {
                            trace(TaskEvent::worker_started, delivery->task.id, std::this_thread::get_id());
                        } catch (...) {
                        }
                    }
                    result = work(delivery->task, std::move(progress));
                } catch (const std::exception &exception) {
                    result = Outcome{unexpected(exception.what())};
                } catch (...) {
                    result = Outcome{unexpected("Unexpected worker exception")};
                }
                {
                    std::lock_guard lock(delivery->mutex);
                    delivery->result.emplace(std::move(result));
                }
                QMetaObject::invokeMethod(
                    delivery->endpoint.get(),
                    [delivery, guard] {
                        if (guard)
                            guard->deliver(delivery);
                    },
                    Qt::QueuedConnection);
                if (trace) {
                    try {
                        trace(TaskEvent::completion_queued, delivery->task.id, std::this_thread::get_id());
                    } catch (...) {
                    } // Diagnostics cannot change the prepared outcome.
                }
            }));
    } catch (const std::exception &exception) {
        active_.reset();
        auto failure = coordinator_.complete(delivery->task.id, Outcome{unexpected(exception.what())});
        return Status{failure.error()};
    } catch (...) {
        active_.reset();
        auto failure = coordinator_.complete(delivery->task.id, Outcome{unexpected("Scheduling failed")});
        return Status{failure.error()};
    }
    emit started(delivery->task.id);
    return Status{std::monostate{}};
}
void SearchTaskController::deliverProgress(const std::shared_ptr<Delivery> &delivery) {
    std::size_t current, total;
    {
        std::lock_guard lock(delivery->mutex);
        current = delivery->current;
        total = delivery->total;
        delivery->progressQueued = false;
    }
    if (active_ == delivery && !closing() && !coordinator_.cancelled())
        emit progress(delivery->task.id, current, total);
}
void SearchTaskController::deliver(const std::shared_ptr<Delivery> &delivery) {
    if (active_ != delivery)
        return;
    // Queued completion can overtake the runnable's epilogue. Keep Busy and
    // dependencies alive until the private pool has actually returned to idle.
    if (pool_.activeThreadCount() != 0) {
        QTimer::singleShot(1, this, [this, delivery] { deliver(delivery); });
        return;
    }
    Outcome result{unexpected("Missing completion")};
    {
        std::lock_guard lock(delivery->mutex);
        if (delivery->result)
            result = std::move(*delivery->result);
    }
    if (result && delivery->finalize && !closing() && !coordinator_.cancelled()) {
        try {
            auto finalized = delivery->finalize(result.value());
            if (!finalized)
                result = Outcome{finalized.error()};
        } catch (const std::exception &exception) {
            result = Outcome{unexpected(exception.what())};
        } catch (...) {
            result = Outcome{unexpected("Result preparation failed")};
        }
    }
    if (trace_) {
        try {
            trace_(TaskEvent::owner_delivery, delivery->task.id, std::this_thread::get_id());
        } catch (...) {
        }
    }
    result = coordinator_.complete(delivery->task.id, std::move(result), delivery->publication);
    active_.reset();
    const bool success = bool(result);
    const QPointer<SearchTaskController> guard(this);
    if (!closing() && delivery->completion) {
        try {
            delivery->completion(std::move(result));
        } catch (const std::exception &exception) {
            qWarning("Search presentation failed: %s", exception.what());
        } catch (...) {
            qWarning("Search presentation failed");
        }
    }
    if (!guard)
        return;
    emit finished(delivery->task.id, success);
    if (guard && closing())
        emit drained();
}
} // namespace qualpf::gui
