#pragma once

#include "qualpf/application/search_coordinator.h"
#include <QObject>
#include <QThreadPool>
#include <memory>
#include <optional>

namespace qualpf::gui {
// Qt scheduling only. Admission, cancellation and publication are Application
// rules; workers see owned input/ports and never the coordinator or Session.
class SearchTaskController final : public QObject {
    Q_OBJECT
  public:
    using Outcome = Result<application::PreparedSearchResult>;
    using Work = std::function<Outcome(const application::SearchTask &, application::SearchProgress)>;
    using Completion = std::function<void(Outcome)>;
    enum class TaskEvent { worker_started, completion_queued, owner_delivery };
    // Diagnostic observer; worker events require a thread-safe callback.
    using Trace = std::function<void(TaskEvent, std::uint64_t, std::thread::id)>;
    using Finalize = std::function<Status(application::PreparedSearchResult &)>;
    struct Timing {
        std::uint64_t task = 0;
        double queue_seconds = 0, worker_seconds = 0, delivery_seconds = 0;
        double publication_seconds = 0, presentation_seconds = 0, total_seconds = 0;
        bool success = false;
    };
    SearchTaskController(application::SearchCoordinator &,
                         std::shared_ptr<application::CandidateReaderFactory>, QObject *parent = nullptr,
                         Trace = {});
    ~SearchTaskController() override;
    Status start(application::SearchInput, application::ReferenceDatabase, Completion = {}, Finalize = {});
    Status submit(application::ReferenceDatabase, Work, Completion = {}, Finalize = {},
                  application::SearchPublication = application::SearchPublication::commit,
                  std::shared_ptr<const application::ReferenceReadGuard> = {},
                  std::optional<application::WorkflowTarget> = {});
    void cancel();
    void close();
    bool busy() const noexcept { return bool(active_); }
    bool closing() const noexcept { return coordinator_.closing(); }
    std::uint64_t taskId() const noexcept;
    // Owner-thread diagnostics only; presentation is the synchronous callback,
    // not a claim about a subsequent native paint or asynchronous plot preview.
    std::optional<Timing> lastTiming() const { return lastTiming_; }
  signals:
    void started(quint64 task);
    void progress(quint64 task, quint64 current, quint64 total);
    void workflowProgress(quint64 task);
    void finished(quint64 task, bool success);
    void timingReady(quint64 task);
    void drained();

  private:
    struct Delivery;
    void deliverProgress(const std::shared_ptr<Delivery> &);
    void deliver(const std::shared_ptr<Delivery> &);
    application::SearchCoordinator &coordinator_;
    std::shared_ptr<application::CandidateReaderFactory> factory_;
    Trace trace_;
    QThreadPool pool_;
    std::shared_ptr<Delivery> active_;
    std::optional<Timing> lastTiming_;
};
} // namespace qualpf::gui
