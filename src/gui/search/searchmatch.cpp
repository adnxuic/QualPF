#include "gui/search/search_request.h"
#include "gui/search/searchmatch.h"
#include "gui/state/workspace_context.h"
#include "gui/state/preprocessing_product.h"
#include "qualpf/application/search_preparation.h"



int ensurePeaksFound(qualpf::gui::WorkspaceContext &context, std::optional<qualpf::Error> *failure) {
    if (failure)
        failure->reset();
    auto &session = context.workspace().session();
    const auto source = session.snapshot();
    auto prepared = qualpf::application::prepareSearch(source, source->algorithms->spec.method());
    if (!prepared) {
        if (failure) *failure = prepared.error();
        qWarning("Search preparation rejected: %s (%s)", prepared.error().message.c_str(), prepared.error().reason.c_str());
        return 0;
    }
    if (prepared.value().state == qualpf::application::SearchPreparation::State::needs_peak_preparation) {
        qualpf::application::ProductPreprocessingEdit edit(session);
        auto status = edit.preview(*prepared.value().peaks);
        if (status) status = edit.commit();
        if (!status) {
            if (failure) *failure = status.error();
            qWarning("C++ preprocessing rejected: %s (%s)", status.error().message.c_str(), status.error().reason.c_str());
            return 0;
        }
    }
    return session.snapshot()->peaks.tth.size();
}

int loadExperimentalPeaks(qualpf::gui::WorkspaceContext &context) { return context.peaks().tth.size(); }

qualpf::SearchParameters currentSearchParameters(qualpf::gui::WorkspaceContext &context) {
    return context.settings().load(
        context.workspace().session().snapshot()).value().parameters;
}

qualpf::gui::SearchRequest buildSearchMatchQuery(qualpf::gui::WorkspaceContext &context, const ExperimentalPeaks &ep) {
    auto source = context.workspace().session().snapshot();
    auto settings = context.settings().load(source).value();
    auto input = qualpf::application::matchingInput(source, settings).value();
    // The compatibility entry may explicitly supply a different owned peak set.
    input.query.d = std::vector<double>(ep.d.begin(), ep.d.end());
    input.query.deltaD = std::vector<double>(ep.deltaD.begin(), ep.deltaD.end());
    input.query.wavelength = ep.wave;
    input.query.print = true;
    qualpf::gui::SearchRequest builder(input.query);
    builder.setCalcFom(true);
    builder.setMinFom(settings.parameters.minimum_score);
    builder.setWeight2thetaD(settings.parameters.weight_position);
    builder.setWeightIntensity(settings.parameters.weight_intensity);
    builder.setWeightPhases(settings.parameters.weight_phases);
    builder.setDelta2theta(settings.parameters.delta_two_theta.value);
    if (source->searchOptions.empty()) {
        auto next = *source;
        next.searchOptions = settings.persistedOptions;
        auto status = context.workspace().session().commit(source, std::move(next));
        if (!status) qWarning("Search settings snapshot failed");
    }
    return builder;
}
