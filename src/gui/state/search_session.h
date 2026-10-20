#pragma once

#include "gui/model/cardtype.h"
#include "gui/model/experimentalpeaks.h"

#include "qualpf/application/search_coordinator.h"


#include <QObject>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>

namespace qualpf::gui {
using namespace application;

// Immutable Qt projection/intent adapter. ProductSearchSession owns the
// canonical values and every transaction barrier. No writable state escapes.
struct SessionStateView {
    ProductHandle canonical; // Derived view identity; never a second owner.
    std::optional<ProjectSettings> projectSettings;
    SearchBackend backend = SearchBackend::cpp;
    bool editing = false;
    std::uint64_t revision = 0;
    double automaticDelta = 0.5;
    std::optional<double> peakTolerance;
    MetadataValue searchOptions{MetadataObject{}};
    MetadataValue restraints{MetadataObject{}};
    Pattern pattern;
    std::optional<OriginalSignalProvenance> original_signal_provenance;
    std::shared_ptr<const PowderCifProvenance> powderCifProvenance;
    std::vector<Pattern> comparisonPatterns; // Imported overlays; first pattern alone drives search.
    ExperimentalPeaks peaks;
    QVector<CardType> candidates;
    QVector<CardType> phases;
    MetadataValue projectMetadata{MetadataObject{}};
    MetadataValue projectEnvelope{MetadataObject{}};
    PreprocessingConfiguration preprocessing;
    std::shared_ptr<const PfsmAnalysis> pfsm;
};
using SessionViewHandle = std::shared_ptr<const SessionStateView>;

class SessionView : public QObject {
    Q_OBJECT
  public:
    explicit SessionView(ProductSearchSession &);
    ProductSearchSession &canonicalSession() { return canonical_; }
    SessionViewHandle snapshot() const { return state_; }
    Status commit(SessionViewHandle source, SessionStateView next);
    void replacePeaks(ExperimentalPeaks peaks);
    void replacePattern(Pattern pattern);
    Status openPattern(SessionViewHandle source, Pattern pattern, std::vector<Pattern> comparisons = {},
                       bool synchrotron = false, std::optional<OriginalSignalProvenance> = {});
    void replaceCandidates(QVector<CardType> cards);
    void replacePhases(QVector<CardType> cards);
    void reset();
    // C++ editor owns its only draft; this barrier never stages a Legacy import.
    void beginPreprocessingEdit();
    Status commitPreprocessingEdit(SessionViewHandle source, SessionStateView next);
    void cancelPreprocessingEdit(SessionViewHandle source);
    void beginLegacyEdit(const std::string &operation);
    void endLegacyEdit(const std::string &operation, bool accept);
    // Native preview snapshots are private drafts, never authoritative inputs.
    Status stageLegacyInput(SessionViewHandle source, SessionStateView next);
  signals:
    void changed();

  private:
    ProductSearchSession &canonical_;
    ProductSearchSession::Subscription subscription_;
    SessionViewHandle state_; // Immutable Qt projection only.
};

Status validateSearchState(const SessionStateView &state);
Result<SessionStateView> preparePatternOpen(SessionViewHandle source, Pattern pattern,
                                       std::vector<Pattern> comparisons = {}, bool synchrotron = false);
Result<ExperimentalPeaks> peaksWithTolerance(const ExperimentalPeaks &, double delta);
Result<PeakSet> searchPeaks(const ExperimentalPeaks &peaks);
ReferenceCard searchCard(const CardType &card);
Result<ScoreValues> scoreCard(const SessionStateView &, const CardType &, const SearchParameters &);
Result<QVector<CardType>> rescoreCards(const SessionStateView &, const SearchParameters &);
Result<ExperimentalPeaks> residualPeaks(const SessionStateView &, const CardType &, const SearchParameters &);

} // namespace qualpf::gui

namespace qualpf::gui {
using namespace application;
ProductState productState(const SessionStateView &);
SessionStateView searchView(ProductHandle);
} // namespace qualpf::gui
