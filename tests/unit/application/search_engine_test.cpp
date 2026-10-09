#include "classic_fixture.h"
#include "qualpf/application/search_engine.h"
#include "qualpf/application/search_coordinator.h"
#include <iostream>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool value) {
    if (!value)
        throw std::runtime_error("SearchEngine contract failed");
}
struct Reader : CandidateReader {
    std::vector<ProductCard> cards;
    bool fail = false, throwing = false;
    std::stop_source *cancel = nullptr;
    Result<ProductCard> findById(std::string_view id, double, const ExecutionContext &) override {
        auto card = cards.front(); card.id = id;
        return Result<ProductCard>{std::move(card)};
    }
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                          SearchProgress) override {
        if (throwing)
            throw std::runtime_error("controlled reader exception");
        if (cancel)
            cancel->request_stop();
        if (fail)
            return Result<std::vector<ProductCard>>{
                Error{ErrorCode::missing_data, "test.reader", "controlled read failure"}};
        return Result<std::vector<ProductCard>>{cards};
    }
};
int main() {
    testing::ClassicFixture f;
    ProductState state;
    state.pattern = f.pattern;
    state.peaks.valid = true;
    state.peaks.wave = 1.54;
    state.peaks.integratedIntensity.emplace();
    for (const auto &p : f.peaks.peaks) {
        state.peaks.tth.push_back(p.two_theta.value);
        state.peaks.intensity.push_back(p.intensity);
        state.peaks.intensityOrig.push_back(*p.original_intensity);
        state.peaks.d.push_back(p.d_spacing->value);
        state.peaks.deltaD.push_back(p.delta_d->value);
        state.peaks.fwhm.push_back(p.fwhm->value);
        state.peaks.integratedIntensity->push_back(*p.integrated_intensity);
    }
    ProductCard card;
    card.id = "B";
    for (const auto &r : f.card.reflections) {
        card.tth.push_back(r.two_theta->value);
        card.intensity.push_back(r.intensity);
    }
    Reader reader;
    reader.cards = {card};
    card.id = "A";
    reader.cards.push_back(card);
    ProductSearchSession session(state);
    auto source = session.snapshot();
    SearchInput input;
    input.source = source;
    input.parameters = f.parameters;
    input.taskId = 7;
    input.databaseVersion = 9;
    SearchEngine engine;
    auto result = engine.run(input, reader, f.context);
    check(bool(result) && session.snapshot() == source && result.value().source == source);
    check(result.value().taskId == 7 && result.value().databaseVersion == 9);
    const auto &cards = result.value().next.candidates;
    check(cards.size() == 2 && cards[0].id == "B" && cards[1].id == "A");
    // Existing ClassicWorkflow fixture authority: total 1, scale 10.
    check(cards[0].fom == 1 && cards[0].scale == 10);
    input.threshold = 1;
    check(engine.run(input, reader, f.context).value().next.candidates.empty());
    input.threshold.reset();
    input.parameters.maximum_entries = 1;
    check(engine.run(input, reader, f.context).value().next.candidates.front().id == "B");
    input.parameters.maximum_entries = 0;
    check(engine.run(input, reader, f.context).value().next.candidates.empty());
    input.parameters = f.parameters;
    auto rescoring = state;
    rescoring.candidates = cards;
    rescoring.candidates[0].scale = 3;
    auto rescored = engine.rescore(rescoring, f.parameters, f.context);
    check(bool(rescored) && rescored.value().front().scale == 3);
    std::stop_source cancellation;
    auto context = f.context;
    context.cancellation = cancellation.get_token();
    reader.cancel = &cancellation;
    auto cancelled = engine.run(input, reader, context);
    check(!cancelled && cancelled.error().code == ErrorCode::cancelled);
    reader.cancel = nullptr;
    reader.fail = true;
    check(!engine.run(input, reader, f.context) && session.snapshot() == source);
    reader.fail = false;
    reader.throwing = true;
    auto exception = engine.run(input, reader, f.context);
    check(!exception && exception.error().reason == "unexpected_exception");
    reader.throwing = false;
    std::stop_source duringEvaluation;
    context.cancellation = duringEvaluation.get_token();
    auto interrupted = engine.run(input, reader, context,
        [&](auto, auto) { duringEvaluation.request_stop(); });
    check(!interrupted && interrupted.error().code == ErrorCode::cancelled && session.snapshot() == source);
    SearchCoordinator execution(session);
    auto admission = execution.begin().value();
    check(!execution.begin() && execution.running());
    input.source = admission.source;
    input.taskId = admission.id;
    input.databaseVersion = admission.database.selectionVersion;
    input.operation = SearchOperation::initial;
    auto pending = engine.run(input, reader, admission.context);
    check(bool(pending));
    // Cancellation after calculation and before delivery still blocks publication.
    execution.cancel();
    auto late = execution.complete(admission.id, std::move(pending), SearchPublication::prepare_only);
    check(!late && late.error().code == ErrorCode::cancelled && session.snapshot() == source &&
          !execution.running());
    admission = execution.begin().value();
    input.source = admission.source;
    input.taskId = admission.id;
    input.databaseVersion = admission.database.selectionVersion;
    pending = engine.run(input, reader, admission.context);
    session.replacePeaks(state.peaks);
    auto stale = execution.complete(admission.id, std::move(pending), SearchPublication::prepare_only);
    check(!stale && stale.error().code == ErrorCode::stale_snapshot);
    input.operation = SearchOperation::accept_phase;
    input.source = source;
    input.phase = cards.front();
    auto accepted = engine.run(input, reader, f.context);
    check(bool(accepted) && accepted.value().next.phases.size() == 1);
    input.source = std::make_shared<const ProductState>(accepted.value().next);
    auto duplicate = engine.run(input, reader, f.context);
    check(bool(duplicate) && !duplicate.value().changed &&
          duplicate.value().next.peaks.intensity == accepted.value().next.peaks.intensity);
    auto withPhases = state;
    withPhases.phases = {cards.front()};
    input.source = std::make_shared<const ProductState>(withPhases);
    input.operation = SearchOperation::initial;
    check(engine.run(input, reader, f.context).value().next.phases.front().id == cards.front().id);
    withPhases.phases.clear();
    withPhases.candidates = cards;
    input.source = std::make_shared<const ProductState>(withPhases);
    input.operation = SearchOperation::accept_phase;
    input.subtractResidual = false;
    accepted = engine.run(input, reader, f.context);
    check(accepted && accepted.value().next.candidates.size() == 1 &&
          accepted.value().next.candidates.front().id == "A");
    input.source = source;
    input.operation = SearchOperation::load_card;
    input.cardId = "ID";
    input.threshold = 2;
    input.parameters.maximum_entries = 0;
    auto loaded = engine.run(input, reader, f.context);
    check(loaded && loaded.value().next.candidates.size() == 1 && loaded.value().next.candidates.front().fom == 1 &&
          loaded.value().next.candidates.front().scale == 10);
    input.source = std::make_shared<const ProductState>(loaded.value().next);
    auto duplicateCard = engine.run(input, reader, f.context);
    check(duplicateCard && !duplicateCard.value().changed);
    input.operation = SearchOperation::metadata_browse;
    input.source = std::make_shared<const ProductState>();
    check(engine.run(input, reader, f.context).value().next.candidates.size() == 2);
    std::cout << "PASS: shared Core fixture, strict threshold, stable ties, cap zero, old scale, metadata "
                 "browse and cancellation/failure isolation\n";
}
