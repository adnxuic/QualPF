#include "classic_fixture.h"
#include "gui/adapters/synchronous_search_service.h"
#include <QCoreApplication>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v) { if (!v) throw std::runtime_error("CLI selection contract"); }
struct Reader : CandidateReader {
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        testing::ClassicFixture f; std::vector<ProductCard> cards;
        for (auto [id, factor] : {std::pair{"A",10.}, {"B",5.}, {"C",2.5}}) {
            ProductCard c; c.id = id;
            for (const auto &r : f.card.reflections) { c.tth.push_back(r.two_theta->value); c.intensity.push_back(r.intensity * factor); }
            cards.push_back(c);
        }
        return Result<std::vector<ProductCard>>{cards};
    }
};
struct Matcher : search::IClassicMatcher {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"test.cli.matcher", 1}; }
    AlgorithmIdentity produces() const override { return search::classic_match_contract(); }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &input, const ExecutionContext &context) const override {
        ++calls;
        auto result = search::ClassicMatcher{}.match(input, context);
        if (result) result.value().position_difference_sum = input.candidate.card_id == "A" ? .375f :
            input.candidate.card_id == "B" ? .1875f : 0.f;
        return result;
    }
};
int main(int argc, char **argv) try {
    QCoreApplication app(argc, argv); testing::ClassicFixture f; Reader reader;
    Workspace workspace; ProductState s; s.pattern = f.pattern;
    auto p = f.peaks; p.effective_wavelength = WavelengthAngstrom{1.54}; s.peaks = productMeasuredPeaks(p).value();
    check(bool(workspace.session().commit(workspace.session().snapshot(), s)));
    gui::SynchronousSearchService service(workspace.searches(), reader);
    gui::SearchRequest q; q.setCalcFom(true); q.setMinFom(.25); q.setDelta2theta(.25);
    for (std::size_t cap : {4,3,2,0}) {
        auto cards = service.makeMatch(q, cap, false);
        check(service.searchError().isEmpty() && cards.size() == std::min(std::size_t(3), cap));
        for (int i = 0; i < cards.size(); ++i) check(cards[i].getId() == QString(QChar('A'+i)));
    }
    auto matcher = std::make_shared<Matcher>(); AlgorithmSelections selected; selected.matcher = matcher->identity();
    AlgorithmModules modules; modules.matcher = matcher;
    check(bool(workspace.configureAlgorithms(AlgorithmSpec{selected}, modules)));
    const auto source = workspace.session().snapshot();
    auto cards = service.makeMatch(q, 2, false);
    check(cards.size() == 2 && cards[0].getId() == "C" && cards[1].getId() == "B" && matcher->calls == 3);
    const auto c = cards[0].productValue();
    check(c.algorithmSource && c.algorithmSource->score.source.lock() == source &&
          c.algorithmSource->score.algorithms == source->algorithms);
    cards = service.makeMatch(q, 1, false);
    check(cards.size() == 1 && cards[0].getId() == "C" && !workspace.searches().running() && workspace.session().snapshot() == source);
    cards = service.makeMatch(q, 3, false);
    check(cards.size() == 3 && cards[0].getId() == "A" && cards[2].getId() == "C");
    q.setMinFom(1);
    check(service.makeMatch(q, 1, false).empty() && service.searchError().isEmpty() &&
          !workspace.searches().running() && workspace.session().snapshot() == source);
    std::cout << "PASS: CLI adapter uses fixed threshold/order/cap with selected matching evidence and preserves frozen card origins\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
