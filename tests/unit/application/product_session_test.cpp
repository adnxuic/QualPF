#include "qualpf/application/product_session.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool value) {
    if (!value)
        throw std::runtime_error("Product Session contract failed");
}
int main() {
    ProductSearchSession session, other;
    auto opening = session.snapshot();
    int calls = 0;
    auto subscription = session.subscribe([&](ProductHandle state) {
        check(state == session.snapshot());
        ++calls;
    });
    auto next = *opening;
    next.projectMetadata["wide"] = std::int64_t(9007199254740993LL);
    check(bool(session.commit(opening, next)));
    check(calls == 1 && session.snapshot()->revision == 1 && opening->revision == 0);
    auto current = session.snapshot();
    check(!session.commit(opening, next) && session.snapshot() == current);
    check(!session.commit(other.snapshot(), next) && session.snapshot() == current);
    next = *current;
    next.automaticDelta = std::numeric_limits<double>::quiet_NaN();
    check(!session.commit(current, next) && session.snapshot() == current && calls == 1);
    next = *current;
    bool threadRejected = false;
    std::thread worker([&] {
        auto status = session.commit(current, next);
        threadRejected = !status && status.error().field == "session.thread";
    });
    worker.join();
    check(threadRejected && session.snapshot() == current);
    session.beginPreprocessingEdit();
    auto edit = session.snapshot();
    check(edit->editing && calls == 2);
    next = *edit;
    next.projectMetadata["draft"] = true;
    check(bool(session.commitPreprocessingEdit(edit, next)));
    check(!session.snapshot()->editing && calls == 3);
    session.beginPreprocessingEdit();
    edit = session.snapshot();
    next = *edit;
    next.projectMetadata["newer"] = true;
    check(bool(session.commit(edit, next)));
    auto newer = session.snapshot();
    check(!session.commitPreprocessingEdit(edit, *edit) && session.snapshot() == newer);
    session.cancelPreprocessingEdit(edit);
    check(!session.snapshot()->editing && session.snapshot()->projectMetadata.contains("newer"));
    subscription.reset();
    const int unsubscribed = calls;
    session.reset();
    check(calls == unsubscribed);
    ProductState exhausted;
    exhausted.revision = std::numeric_limits<std::uint64_t>::max();
    ProductSearchSession maximum(exhausted);
    auto maxSource = maximum.snapshot();
    auto rejected = maximum.commit(maxSource, *maxSource);
    check(!rejected && rejected.error().code == ErrorCode::exhausted_revision &&
          maximum.snapshot() == maxSource);
    ProductSearchSession::Subscription survivor;
    {
        ProductSearchSession shortLived;
        survivor = shortLived.subscribe([](auto) {});
    }
    survivor.reset();
    std::cout << "PASS: canonical ownership, atomic failure/stale/thread rejection, edit barriers, revision "
                 "exhaustion and subscription lifetime\n";
}
