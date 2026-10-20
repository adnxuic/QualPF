#include "gui/state/search_session.h"
#include "gui/adapters/experimental_peaks.h"
#include "qualpf/application/search_engine.h"


#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace qualpf::gui {
using namespace application;
SessionView::SessionView(ProductSearchSession &session) : canonical_(session) {
    state_ = std::make_shared<const SessionStateView>(searchView(canonical_.snapshot()));
    subscription_ = canonical_.subscribe([this](ProductHandle source) {
        state_ = std::make_shared<const SessionStateView>(searchView(std::move(source)));
        emit changed();
    });
}
Status validateSearchState(const SessionStateView &state) { return validateProductState(productState(state)); }

Result<ExperimentalPeaks> peaksWithTolerance(const ExperimentalPeaks &peaks, double delta) {
    auto result=productPeaksWithTolerance(peaks.productValue(),delta);
    if (!result) return Result<ExperimentalPeaks>{result.error()};
    return Result<ExperimentalPeaks>{ExperimentalPeaks::fromProductValue(result.value())};
}

ProductState productState(const SessionStateView &view) {
    ProductState out;
    out.projectSettings = view.projectSettings;
    out.backend = view.backend;
    if (view.canonical) {
        out.workflow = view.canonical->workflow;
        out.prepared_pfsm = view.canonical->prepared_pfsm;
        out.analysis_database = view.canonical->analysis_database;
        out.restored_project = view.canonical->restored_project;
        out.projectStateExtensions = view.canonical->projectStateExtensions;
        out.algorithms = view.canonical->algorithms;
        out.provenance = view.canonical->provenance;
    }
    out.editing = view.editing;
    out.revision = view.revision;
    out.automaticDelta = view.automaticDelta;
    out.peakTolerance = view.peakTolerance;
    out.searchOptions = view.searchOptions.toObject();
    out.restraints = view.restraints.toObject();
    out.projectMetadata = view.projectMetadata.toObject();
    out.projectEnvelope = view.projectEnvelope.toObject();
    out.pattern = view.pattern;
    out.original_signal_provenance = view.original_signal_provenance;
    out.powderCifProvenance = view.powderCifProvenance;
    out.comparisonPatterns = view.comparisonPatterns;
    out.peaks = view.peaks.productValue();
    out.preprocessing = view.preprocessing;
    out.pfsm = view.pfsm;
    for (const auto &card : view.candidates)
        out.candidates.push_back(card.productValue());
    for (const auto &card : view.phases)
        out.phases.push_back(card.productValue());
    return out;
}
SessionStateView searchView(ProductHandle source) {
    SessionStateView view;
    view.canonical = source;
    view.projectSettings = source->projectSettings;
    view.backend = source->backend;
    view.editing = source->editing;
    view.revision = source->revision;
    view.automaticDelta = source->automaticDelta;
    view.peakTolerance = source->peakTolerance;
    view.searchOptions = MetadataValue(source->searchOptions);
    view.restraints = MetadataValue(source->restraints);
    view.projectMetadata = MetadataValue(source->projectMetadata);
    view.projectEnvelope = MetadataValue(source->projectEnvelope);
    view.pattern = source->pattern;
    view.original_signal_provenance = source->original_signal_provenance;
    view.powderCifProvenance = source->powderCifProvenance;
    view.comparisonPatterns = source->comparisonPatterns;
    view.peaks = ExperimentalPeaks::fromProductValue(source->peaks);
    view.preprocessing = source->preprocessing;
    view.pfsm = source->pfsm;
    for (const auto &card : source->candidates)
        view.candidates.append(CardType(card));
    for (const auto &card : source->phases)
        view.phases.append(CardType(card));
    return view;
}
Status SessionView::commit(SessionViewHandle source, SessionStateView next) {
    if (source != state_)
        return Status{Error{ErrorCode::stale_snapshot, "session", "Search input changed"}};
    return canonical_.commit(source->canonical, productState(next));
}
void SessionView::replacePeaks(ExperimentalPeaks peaks) { canonical_.replacePeaks(peaks.productValue()); }
void SessionView::replacePattern(Pattern pattern) { canonical_.replacePattern(std::move(pattern)); }
Status SessionView::openPattern(SessionViewHandle source, Pattern pattern, std::vector<Pattern> comparisons,
                                  bool synchrotron, std::optional<OriginalSignalProvenance> original) {
    if (source != state_)
        return Status{Error{ErrorCode::stale_snapshot, "session", "Search input changed"}};
    return canonical_.openPattern(source->canonical, std::move(pattern), std::move(comparisons), synchrotron, std::move(original));
}
Result<SessionStateView> preparePatternOpen(SessionViewHandle source, Pattern pattern, std::vector<Pattern> comparisons,
                                       bool synchrotron) {
    if (!source)
        return Result<SessionStateView>{Error{ErrorCode::invalid_input, "session.edit",
                                         "Finish the preprocessing edit before opening a pattern"}};
    auto result = prepareProductPatternOpen(std::make_shared<const ProductState>(productState(*source)),
                                            std::move(pattern), std::move(comparisons), synchrotron);
    if (!result)
        return Result<SessionStateView>{result.error()};
    return Result<SessionStateView>{searchView(std::make_shared<const ProductState>(std::move(result).value()))};
}
void SessionView::replaceCandidates(QVector<CardType> cards) {
    std::vector<ProductCard> values;
    for (const auto &card : cards)
        values.push_back(card.productValue());
    canonical_.replaceCandidates(std::move(values));
}
void SessionView::replacePhases(QVector<CardType> cards) {
    std::vector<ProductCard> values;
    for (const auto &card : cards)
        values.push_back(card.productValue());
    canonical_.replacePhases(std::move(values));
}
void SessionView::reset() { canonical_.reset(); }
void SessionView::beginPreprocessingEdit() { canonical_.beginPreprocessingEdit(); }
Status SessionView::commitPreprocessingEdit(SessionViewHandle source, SessionStateView next) {
    if (source != state_)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing", "Preview source changed"}};
    return canonical_.commitPreprocessingEdit(source->canonical, productState(next));
}
void SessionView::cancelPreprocessingEdit(SessionViewHandle source) {
    canonical_.cancelPreprocessingEdit(source ? source->canonical : ProductHandle{});
}
void SessionView::beginLegacyEdit(const std::string &operation) { canonical_.beginLegacyEdit(operation); }
void SessionView::endLegacyEdit(const std::string &operation, bool accept) {
    canonical_.endLegacyEdit(operation, accept);
}
Status SessionView::stageLegacyInput(SessionViewHandle source, SessionStateView next) {
    if (source != state_)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing.preview", "Preview source changed"}};
    return canonical_.stageLegacyInput(source->canonical, productState(next));
}
Result<PeakSet> searchPeaks(const ExperimentalPeaks &peaks) { return productSearchPeaks(peaks.productValue()); }
ReferenceCard searchCard(const CardType &card) { return productSearchCard(card.productValue()); }
Result<ScoreValues> scoreCard(const SessionStateView &state, const CardType &card, const SearchParameters &parameters) {
    return SearchEngine{}.score(productState(state),card.productValue(),parameters,
                               {{},NumericalPolicy::legacy_mixed_precision,"product-search"});
}
Result<QVector<CardType>> rescoreCards(const SessionStateView &state, const SearchParameters &parameters) {
    auto result=SearchEngine{}.rescore(productState(state),parameters,
                                      {{},NumericalPolicy::legacy_mixed_precision,"product-search"});
    if (!result) return Result<QVector<CardType>>{result.error()};
    QVector<CardType> cards; for (const auto &card:result.value()) cards.append(CardType(card));
    return Result<QVector<CardType>>{std::move(cards)};
}
Result<ExperimentalPeaks> residualPeaks(const SessionStateView &state, const CardType &card, const SearchParameters &parameters) {
    auto result=SearchEngine{}.residual(productState(state),card.productValue(),parameters,
                                       {{},NumericalPolicy::legacy_mixed_precision,"product-search"});
    if (!result) return Result<ExperimentalPeaks>{result.error()};
    return Result<ExperimentalPeaks>{ExperimentalPeaks::fromProductValue(result.value())};
}
} // namespace qualpf::gui
