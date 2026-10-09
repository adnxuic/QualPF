#include "qualpf/application/search_coordinator.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <thread>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool value) {
    if (!value)
        throw std::runtime_error("SearchCoordinator contract failed");
}
static PreparedSearchResult prepared(const SearchTask &task) {
    PreparedSearchResult result;
    result.source = task.source;
    result.taskId = task.id;
    result.databaseVersion = task.database.selectionVersion;
    result.next = *task.source;
    result.next.projectMetadata["result"] = true;
    result.changed = true;
    return result;
}
int main() {
    Workspace workspace;
    auto &session = workspace.session();
    auto &coordinator = workspace.searches();
    int notifications = 0;
    auto subscription = session.subscribe([&](auto) { ++notifications; });
    auto source = session.snapshot();
    auto task = coordinator.begin({"reference", 3}).value();
    check(!coordinator.begin() && coordinator.running() && !task.context.cancellation.stop_requested());
    auto result = coordinator.complete(task.id, Result<PreparedSearchResult>{prepared(task)});
    check(bool(result) && notifications == 1 && session.snapshot()->revision == source->revision + 1);
    check(!coordinator.running());
    source = session.snapshot();
    task = coordinator.begin({"reference", 3}).value();
    result = coordinator.complete(task.id, Result<PreparedSearchResult>{prepared(task)},
                                  SearchPublication::prepare_only);
    check(bool(result) && session.snapshot() == source && notifications == 1);
    task = coordinator.begin().value();
    coordinator.cancel();
    result = coordinator.complete(task.id, Result<PreparedSearchResult>{prepared(task)});
    check(!result && result.error().code == ErrorCode::cancelled && session.snapshot() == source);
    task = coordinator.begin().value();
    auto obsolete = coordinator.complete(task.id - 1, Result<PreparedSearchResult>{prepared(task)});
    check(!obsolete && obsolete.error().reason == "old_task" && coordinator.running());
    auto unrelated = *source;
    unrelated.projectMetadata["unrelated"] = 1;
    check(bool(session.commit(source, std::move(unrelated))));
    source = session.snapshot();
    result = coordinator.complete(task.id, Result<PreparedSearchResult>{prepared(task)});
    check(!result && result.error().code == ErrorCode::stale_snapshot && session.snapshot() == source);
    task = coordinator.begin({"reference", 3}).value();
    coordinator.selectDatabase(4);
    result = coordinator.complete(task.id, Result<PreparedSearchResult>{prepared(task)});
    check(!result && result.error().field == "search.database" && session.snapshot() == source);
    task = coordinator.begin().value();
    result = coordinator.complete(
        task.id, Result<PreparedSearchResult>{Error{ErrorCode::missing_data, "reader", "failed"}});
    check(!result && !coordinator.running() && session.snapshot() == source);
    task = coordinator.begin().value();
    std::thread worker([&] {
        auto denied = coordinator.complete(task.id, Result<PreparedSearchResult>{prepared(task)});
        check(!denied && denied.error().field == "search.thread");
    });
    worker.join();
    check(coordinator.running());
    coordinator.close();
    result = coordinator.complete(task.id, Result<PreparedSearchResult>{prepared(task)});
    check(!result && result.error().reason == "closing" && !coordinator.begin() &&
          session.snapshot() == source);
    ProductState exhausted;
    exhausted.revision = std::numeric_limits<std::uint64_t>::max();
    ProductSearchSession maximum(exhausted);
    SearchCoordinator limited(maximum);
    task = limited.begin().value();
    result = limited.complete(task.id, Result<PreparedSearchResult>{prepared(task)});
    check(!result && result.error().code == ErrorCode::exhausted_revision &&
          maximum.snapshot() == task.source);
    SearchCoordinator identities(maximum, std::numeric_limits<std::uint64_t>::max());
    check(!identities.begin() && !identities.running());
    std::cout << "PASS: shared workspace, atomic publication, Busy, task identity, cancellation, stale, "
                 "database epoch, owner thread, Closing and exhaustion\n";
}
