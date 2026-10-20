#include "classic_fixture.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool v, std::source_location at = std::source_location::current()) {
    if (!v) throw std::runtime_error("Algorithm provenance at line " + std::to_string(at.line()));
}
static ProductState state() {
    testing::ClassicFixture f;
    ProductState s;
    s.pattern = f.pattern; s.pattern.radiation.lines = {{WavelengthAngstrom{1.54}, 1}};
    s.pattern.processing.background_subtracted = false;
    s.pattern.processing.intensity_scaled = false;
    s.pattern.original_observed = s.pattern.observed;
    s.pattern.intensity_scale = IntensityScale::counts;
    s.peaks.valid = true; s.peaks.wave = 1.54;
    for (const auto &p : f.peaks.peaks) {
        s.peaks.tth.push_back(p.two_theta.value); s.peaks.intensity.push_back(p.intensity);
        s.peaks.intensityOrig.push_back(*p.original_intensity); s.peaks.d.push_back(p.d_spacing->value);
        s.peaks.deltaD.push_back(p.delta_d->value); s.peaks.fwhm.push_back(p.fwhm->value);
    }
    return s;
}
struct Reader : CandidateReader {
    int reads = 0;
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        ++reads;
        testing::ClassicFixture f;
        ProductCard c; c.id = "A";
        for (const auto &r : f.card.reflections) { c.tth.push_back(r.two_theta->value); c.intensity.push_back(r.intensity); }
        auto b = c; b.id = "B";
        return Result<std::vector<ProductCard>>{std::vector<ProductCard>{c, b}};
    }
};
struct MatcherB : search::IClassicMatcher {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"test.provenance.matcher-b", 1}; }
    AlgorithmIdentity produces() const override { return search::classic_match_contract(); }
    Result<search::ClassicMatch> match(const search::ClassicMatchInput &i, const ExecutionContext &ctx) const override {
        ++calls;
        auto result = search::ClassicMatcher{}.match(i, ctx);
        if (result) {
            // Twice the reference evidence gives the independently known scale 10/2.
            for (auto &value : result.value().merged) value *= 2;
            for (auto &value : result.value().card_y) value *= 2;
        }
        return result;
    }
};
struct BackgroundB : preprocessing::IBackgroundEstimator {
    AlgorithmIdentity identity() const override { return {"test.provenance.background-b", 1}; }
    Result<preprocessing::BackgroundResult> estimate(const Pattern &p, const preprocessing::BackgroundOptions &o,
        WavelengthAngstrom w, std::optional<std::span<const preprocessing::BackgroundPoint>> points, const ExecutionContext &ctx) const override {
        return preprocessing::CppBackgroundEstimator{}.estimate(p, o, w, points, ctx);
    }
};
int main() try {
    testing::ClassicFixture f; Reader reader; Workspace w;
    check(bool(w.session().commit(w.session().snapshot(), state())));
    SearchInput input; input.source = w.session().snapshot(); input.parameters = f.parameters;
    input.taskId = 17; input.databaseVersion = 23;
    auto found = SearchEngine{}.run(input, reader, f.context); check(bool(found));
    auto &candidate = found.value().next.candidates[0];
    check(candidate.scale == 10 && candidate.algorithmSource &&
          candidate.algorithmSource->scale.taskId == 17 && candidate.algorithmSource->score.databaseVersion == 23 &&
          candidate.algorithmSource->score.source.lock() == input.source);
    candidate.scale = 3;
    check(bool(w.session().commit(input.source, found.value().next)));
    input.source = w.session().snapshot(); input.operation = SearchOperation::rescore;
    auto same = SearchEngine{}.run(input, reader, f.context);
    check(same && same.value().next.candidates[0].scale == 3 &&
          same.value().next.candidates[0].algorithmSource->scale.taskId == 17);
    auto b = std::make_shared<MatcherB>(); AlgorithmSelections s; s.matcher = b->identity(); AlgorithmModules m; m.matcher = b;
    auto resolved = resolveAlgorithms(AlgorithmSpec{s}, m); check(bool(resolved));
    // Constructor B/source A must fail before any module/database work, on every public operation.
    const auto beforeReads = reader.reads;
    auto mismatch = SearchEngine{resolved.value()}.run(input, reader, f.context);
    check(!mismatch && mismatch.error().reason == "algorithm_provenance_mismatch" && b->calls == 0 && reader.reads == beforeReads);
    check(!SearchEngine{resolved.value()}.score(*input.source, candidate, f.parameters, f.context));
    check(!SearchEngine{resolved.value()}.rescore(*input.source, f.parameters, f.context));
    check(!SearchEngine{resolved.value()}.residual(*input.source, candidate, f.parameters, f.context));
    // An equivalent specification is still a different, unfrozen assembly.
    const auto equivalent = resolveAlgorithms(input.source->algorithms->spec).value();
    check(equivalent->spec == input.source->algorithms->spec && equivalent != input.source->algorithms);
    check(!SearchEngine{equivalent}.run(input, reader, f.context) && reader.reads == beforeReads);
    check(!SearchEngine{equivalent}.score(*input.source, candidate, f.parameters, f.context));
    check(!SearchEngine{equivalent}.rescore(*input.source, f.parameters, f.context));
    check(!SearchEngine{equivalent}.residual(*input.source, candidate, f.parameters, f.context));
    check(bool(w.configureAlgorithms(AlgorithmSpec{s}, m)));
    input.source = w.session().snapshot();
    auto oldScale = SearchEngine{}.run(input, reader, f.context);
    check(!oldScale && oldScale.error().reason == "algorithm_provenance_mismatch" && b->calls == 0);
    input.operation = SearchOperation::accept_phase; input.phase = candidate;
    check(!SearchEngine{}.run(input, reader, f.context));
    // A fresh search uses B's fresh scale; it does not reinterpret A's display scale.
    input.operation = SearchOperation::initial;
    auto fresh = SearchEngine{}.run(input, reader, f.context);
    check(fresh && fresh.value().next.candidates[0].scale == 5);

    Workspace history; check(bool(history.session().commit(history.session().snapshot(), state())));
    input = SearchInput(); input.source = history.session().snapshot(); input.parameters = f.parameters;
    found = SearchEngine{}.run(input, reader, f.context); check(bool(found));
    check(bool(history.session().commit(input.source, found.value().next)));
    input.source = history.session().snapshot(); input.operation = SearchOperation::accept_phase;
    input.phase = input.source->candidates.front(); input.phase->scale = 3;
    auto accepted = SearchEngine{}.run(input, reader, f.context); check(bool(accepted));
    check(accepted.value().next.provenance.residualHistory.size() == 1);
    check(bool(history.session().commit(input.source, accepted.value().next)));
    const auto residual = history.session().snapshot()->peaks;
    history.session().replacePhases({}); // Does not erase provenance or restore the spectrum.
    check(bool(history.configureAlgorithms(AlgorithmSpec{s}, m)));
    const auto current = history.session().snapshot();
    input.source = current; input.phase = found.value().next.candidates[1];
    auto mixed = SearchEngine{}.run(input, reader, f.context);
    check(!mixed && mixed.error().reason == "algorithm_provenance_mismatch" && history.session().snapshot() == current && current->peaks == residual);
    input.operation = SearchOperation::initial; check(!SearchEngine{}.run(input, reader, f.context));
    // Returning to the compatible algorithm is explicit and retains historical residual.
    check(bool(history.configureAlgorithms(AlgorithmSpec{})));
    input.source = history.session().snapshot(); check(bool(SearchEngine{}.run(input, reader, f.context)));

    // Publication also checks the frozen handle, and every rejection retires Busy.
    auto task = history.searches().begin().value();
    PreparedSearchResult forged{task.source, task.id, 0, *task.source}; forged.next.algorithms = resolved.value();
    auto rejected = history.searches().complete(task.id, Result<PreparedSearchResult>{forged});
    check(!rejected && rejected.error().reason == "algorithm_provenance_mismatch" && !history.searches().running() && history.session().snapshot() == task.source);
    Workspace background; check(bool(background.session().commit(background.session().snapshot(), state())));
    auto backgroundSource = background.session().snapshot();
    PreprocessingRequest request;
    request.wavelength = WavelengthAngstrom{1.54}; request.background = preprocessing::BackgroundOptions{};
    request.background->method = preprocessing::BackgroundMethod::none;
    request.background_points = std::vector<preprocessing::BackgroundPoint>{{0,0},{60,0}};
    PreprocessingEvaluation evaluation(backgroundSource); check(bool(evaluation.preview(request)));
    check(evaluation.preview()->provenance.subtractedBackgroundHistory.empty() &&
          evaluation.preview()->provenance.backgroundSource->background->method == preprocessing::BackgroundMethod::none);
    auto historicalBackground = *evaluation.preview();
    historicalBackground.pattern = preprocessing::subtract_background(historicalBackground.pattern).value();
    historicalBackground.provenance.subtractedBackgroundHistory.push_back(*historicalBackground.provenance.backgroundSource);
    check(bool(background.session().commit(backgroundSource, std::move(historicalBackground))));
    auto bg = std::make_shared<BackgroundB>(); s = {}; m = {}; s.background = bg->identity(); m.background = bg;
    check(bool(background.configureAlgorithms(AlgorithmSpec{s}, m)));
    input.source = background.session().snapshot(); input.operation = SearchOperation::initial;
    check(!SearchEngine{}.run(input, reader, f.context));
    input.operation = SearchOperation::metadata_browse; check(bool(SearchEngine{}.run(input, reader, f.context)));
    request = PreprocessingRequest(); request.restart_from_original = true;
    PreprocessingEvaluation restart(input.source); check(bool(restart.preview(request)));
    check(restart.preview()->provenance.subtractedBackgroundHistory.empty() && restart.preview()->provenance.residualHistory.empty() &&
          restart.preview()->pattern.observed == *input.source->pattern.original_observed);
    std::cout << "PASS: frozen origins, incompatible assembly, old scale, historical residual and atomic publication\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
