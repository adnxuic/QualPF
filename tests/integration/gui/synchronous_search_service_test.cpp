#include "gui/adapters/synchronous_search_service.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
namespace {
void check(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
class Reader : public CandidateReader {
  public:
    bool empty = false, failure = false;
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &context,
                                          SearchProgress progress) override {
        if (progress)
            progress(1, 1);
        if (context.cancellation.stop_requested())
            return Result<std::vector<ProductCard>>{Error{ErrorCode::cancelled, "search", "Search canceled"}};
        if (failure)
            return Result<std::vector<ProductCard>>{Error{ErrorCode::missing_data, "database.query", "query failed"}};
        std::vector<ProductCard> cards;
        if (!empty) {
            ProductCard card;
            card.id = "one";
            cards.push_back(std::move(card));
        }
        return Result<std::vector<ProductCard>>{std::move(cards)};
    }
};
} // namespace
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv);
    Workspace workspace;
    Reader reader;
    gui::SynchronousSearchService service(workspace.searches(), reader);
    gui::SearchRequest query;
    const auto source = workspace.session().snapshot();
    int publications = 0;
    auto subscription = workspace.session().subscribe([&](auto) { ++publications; });
    auto unchanged = [&] {
        check(!service.isSearchRunning() && workspace.session().snapshot() == source && publications == 0,
              "Synchronous prepare/error must not publish or leave an active task");
    };
    auto success = [&](bool empty) {
        reader.empty = empty;
        reader.failure = false;
        const auto cards = service.makeQuery(query);
        check(cards.size() == (empty ? 0 : 1) && service.searchError().isEmpty() && !service.isCanceled(),
              "New call must clear preceding failure/cancellation while allowing genuine empty success");
        unchanged();
    };
    success(false);
    reader.failure = true;
    check(service.makeQuery(query).isEmpty() && service.searchError() == "query failed" && !service.isCanceled(),
          "Query failure must remain distinguishable from empty success");
    unchanged();
    success(true);
    check(service.makeQuery(query, [&](int, int) { service.cancelSearch(); }).isEmpty() &&
              service.isCanceled() && service.searchError() == "Search canceled",
          "Existing cooperative callback cancellation must retain error and cancellation state");
    unchanged();
    success(false);
    check(service.makeQuery(query, [](int, int) { throw std::runtime_error("controlled callback failure"); }).isEmpty() &&
              service.searchError() == "controlled callback failure" && !service.isCanceled(),
          "Existing engine exception boundary must expose the current call error");
    unchanged();
    success(true);
    std::cout << "PASS: synchronous per-call success/empty/error/cancel/exception/retry; zero publications\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
