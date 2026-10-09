#include "gui/search/search_request.h"
#include "gui/adapters/synchronous_search_service.h"

namespace qualpf::gui {
QVector<CardType> SynchronousSearchService::run(const qualpf::gui::SearchRequest &builder,
                                                application::CandidateQuery::Route route,
                                                ProgressCallback progress) {
    qualpf::SearchParameters parameters;
    parameters.delta_two_theta.value = builder.getDelta2theta();
    parameters.weight_position = builder.getWeight2thetaD();
    parameters.weight_intensity = builder.getWeightIntensity();
    parameters.weight_phases = builder.getWeightPhases();
    auto result = application::synchronousQuery(execution_, reader_,
        database_ ? database_() : application::ReferenceDatabase{}, builder.candidateQuery(route),
        parameters, builder.getMinFom(), builder.getCalcFom(), progress ? application::SearchProgress([progress](std::size_t n, std::size_t total) {
            progress(static_cast<int>(n), static_cast<int>(total));
        }) : application::SearchProgress{});
    if (!result) { error_ = QString::fromStdString(result.error().message); return {}; }
    error_.clear();
    QVector<CardType> cards;
    for (const auto &card : result.value())
        cards.append(CardType(card));
    return cards;
}
QVector<CardType> SynchronousSearchService::makeQuery(const qualpf::gui::SearchRequest &builder,
                                                      ProgressCallback progress) {
    return run(builder,
               builder.getCalcFom() ? application::CandidateQuery::Route::matching
                                    : application::CandidateQuery::Route::constrained_browse,
               std::move(progress));
}
void SynchronousSearchService::makeQueryStrongest(const qualpf::gui::SearchRequest &builder, QVector<CardType> &cards,
                                                  ProgressCallback progress) {
    auto prepared = run(builder, application::CandidateQuery::Route::stored_strongest, std::move(progress));
    if (error_.isEmpty() && !isCanceled())
        cards += prepared;
}
void SynchronousSearchService::makeQueryWithoutStrongest(const qualpf::gui::SearchRequest &builder,
                                                         QVector<CardType> &cards,
                                                         ProgressCallback progress) {
    auto prepared = run(builder, application::CandidateQuery::Route::matching, std::move(progress));
    if (error_.isEmpty() && !isCanceled())
        cards += prepared;
}
} // namespace qualpf::gui
