#include "classic_fixture.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/presentation/quantification.h"
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
static void check(bool value, std::source_location at = std::source_location::current()) {
    if (!value)
        throw std::runtime_error("Classic algorithm contract at line " + std::to_string(at.line()));
}
int main() try {
    testing::ClassicFixture fixture;
    const auto trace = search::score_classic(fixture.input(), fixture.context);
    check(bool(trace));
    const auto &t = trace.value();
    // Independently analytic: proportional heights (100,200,100) = 10*(10,20,10).
    check(t.score.total == 1 && t.score.position == 1 && t.score.intensity == 1 && t.score.scale == 10);
    check(t.associated_sample_sum == 400 && t.total_sample_sum == 400);
    check(t.associated_card_sum == 400 && t.total_card_sum == 400);
    check(t.database_fraction == 1 && t.sample_fraction == 1 && t.sqrt_database == 1 && t.sqrt_sample == 1);
    check(t.position_difference_sum == 0 && t.expected_peaks_per_phase == 1);
    check(t.reflection_indices == std::vector<std::size_t>({0, 1, 2}));
    check(t.experimental_indices == std::vector<std::optional<std::size_t>>({0, 1, 2}));
    fixture.parameters.minimum_score = 1;
    const std::vector<search::CandidateInput> candidates{{fixture.card, {}, 3.0}};
    auto initial = search::evaluate_candidates(fixture.pattern, fixture.peaks, candidates,
        fixture.parameters, search::SearchPass::initial, fixture.context);
    auto rescore = search::evaluate_candidates(fixture.pattern, fixture.peaks, candidates,
        fixture.parameters, search::SearchPass::rescore, fixture.context);
    check(initial && initial.value().results.candidates.empty());
    check(rescore && rescore.value().results.candidates.size() == 1);
    check(rescore.value().results.candidates.front().outcome.value().scale == 3);
    auto residual = search::subtract_phase(fixture.peaks, fixture.card, 5, fixture.parameters);
    check(residual && residual.value().peaks.peaks.size() == 3);
    check(residual.value().peaks.peaks[0].intensity == 50 &&
          residual.value().peaks.peaks[1].intensity == 100 &&
          residual.value().peaks.peaks[2].intensity == 50);
    check(*residual.value().peaks.peaks[1].original_intensity == 200);

    ProductCard a,b,c; a.id = "A"; b.id = "B"; c.id = "C";
    a.fom = .5; b.fom = 1; c.fom = 1;
    const std::vector<ProductCard> unordered{a,b,c};
    auto ranked = rankCandidates(unordered, {CandidateRankingRequest::Pass::initial, 3}, fixture.context);
    check(ranked && ranked.value()[0].id == "A");
    ranked = rankCandidates(unordered, {CandidateRankingRequest::Pass::initial, 2}, fixture.context);
    check(ranked && ranked.value().size() == 2 && ranked.value()[0].id == "B" && ranked.value()[1].id == "C");
    ranked = rankCandidates(unordered, {CandidateRankingRequest::Pass::rescore, 3}, fixture.context);
    check(ranked && ranked.value()[0].id == "B" && ranked.value()[1].id == "C" && ranked.value()[2].id == "A");
    for (auto pass : {CandidateRankingRequest::Pass::metadata_browse, CandidateRankingRequest::Pass::scored_browse}) {
        ranked = rankCandidates(unordered, {pass, 0}, fixture.context);
        check(ranked && ranked.value().size() == 3 && ranked.value()[0].id == "A");
    }
    const std::vector<double> intensity{2, 10, 4};
    std::vector<QuantificationInput> phases{{true, intensity, 2, 2}, {true, intensity, 3, 1}};
    check(quantify(phases) == std::vector<double>({25, 75}));
    phases[1].rir = 0;
    check(quantify(phases).empty()); // An invalid phase invalidates the whole result.
    check(quantify({}).empty());

    preprocessing::PeakOptions options;
    Pattern flat;
    flat.id = {1};
    for (int i = 0; i < 101; ++i) {
        flat.two_theta.push_back(TwoThetaDegrees{10 + .1 * i});
        flat.observed.push_back(0);
    }
    auto noSignal = preprocessing::find_peaks(flat, options, WavelengthAngstrom{1.54}, AngularWidthDegrees{.15});
    check(!noSignal && noSignal.error().field == "peaks.normalization");
    options.search_range = -1;
    noSignal = preprocessing::find_peaks(flat, options, WavelengthAngstrom{1.54}, AngularWidthDegrees{.15});
    check(!noSignal && noSignal.error().field == "peaks.normalization");
    std::cout << "PASS: analytic FOM evidence, pass thresholds/scale, residual, all-or-empty RIR and peak failures\n";
} catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
}
