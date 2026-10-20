#pragma once
#include "qualpf/application/preprocessing_configuration.h"
#include "qualpf/application/algorithms.h"
#include "qualpf/application/product_values.h"
#include "qualpf/application/original_signal.h"
#include "qualpf/application/workflow_settings.h"
#include "qualpf/application/workflow_state.h"
#include "qualpf/search/classic.h"
#include <functional>
#include <map>
#include <memory>
#include <thread>
namespace qualpf::application {
enum class SearchBackend { cpp, legacy_fallback };
struct PowderCifProvenance;
struct PfsmAnalysis;
struct PfsmProblem;
struct ProductState;
using ProductHandle = std::shared_ptr<const ProductState>;
// Runtime metadata only; source identity is weak to avoid retaining snapshot chains.
// Stable algorithm identities denote immutable semantics; parameters are frozen values.
struct AlgorithmRunSource {
    AlgorithmHandle algorithms;
    SearchParameters parameters;
    std::weak_ptr<const ProductState> source;
    std::uint64_t sourceRevision = 0, taskId = 0, databaseVersion = 0;
    std::optional<preprocessing::BackgroundOptions> background;
    std::optional<preprocessing::PeakOptions> peaks;
    std::optional<FwhmUsage> fwhm;
};
struct CardAlgorithmSource {
    AlgorithmRunSource score, scale;
};
struct AnalysisProvenance {
    bool legacyUnverified = false; // QXP v1/v2 cannot attest algorithm provenance.
    // Retained even after phases are removed: removal never restores residual.
    std::vector<AlgorithmRunSource> residualHistory;
    std::optional<AlgorithmRunSource> backgroundSource, peakSource;
    std::vector<AlgorithmRunSource> retainedPeakSources;
    std::vector<AlgorithmRunSource> subtractedBackgroundHistory;
};
struct ProductState {
    std::string analysis_database; // Stable provider identity, independent of session selection revision.
    bool restored_project = false; // Runtime admission marker, never serialized.
    WorkflowState workflow;
    // Absent only for legacy API callers/files pending migration.
    std::optional<ProjectSettings> projectSettings;
    SearchBackend backend = SearchBackend::cpp;
    bool editing = false;
    std::uint64_t revision = 0;
    double automaticDelta = 0.5;
    std::optional<double> peakTolerance;
    MetadataObject searchOptions;
    MetadataObject restraints;
    Pattern pattern;
    std::optional<OriginalSignalProvenance> original_signal_provenance;
    std::shared_ptr<const PowderCifProvenance> powderCifProvenance;
    std::vector<Pattern> comparisonPatterns; // Imported overlays; first pattern alone drives search.
    ProductPeaks peaks;
    std::vector<ProductCard> candidates;
    std::vector<ProductCard> phases;
    MetadataObject projectMetadata;
    MetadataObject projectEnvelope; // Unknown QXP fields, with exact integer/null types.
    MetadataValue projectStateExtensions; // Unknown typed fields within the QXP v3 state tree.
    PreprocessingConfiguration preprocessing;
    AlgorithmHandle algorithms = defaultAlgorithms();
    AnalysisProvenance provenance;
    std::shared_ptr<const PfsmAnalysis> pfsm;
    std::shared_ptr<const PfsmProblem> prepared_pfsm;
};

class ProductSearchSession {
  public:
    explicit ProductSearchSession(SearchBackend backend = SearchBackend::cpp,
                                   AlgorithmHandle algorithms = defaultAlgorithms(), DefaultSettingsStore *defaults = nullptr);
    explicit ProductSearchSession(ProductState initial);
    ProductSearchSession(const ProductSearchSession &) = delete;
    ProductSearchSession &operator=(const ProductSearchSession &) = delete;
    ProductHandle snapshot() const { return state_; }
    Status commit(ProductHandle source, ProductState next);
    Status restoreProject(ProductHandle source, ProductState next);
    void replacePeaks(ProductPeaks peaks);
    void replacePattern(Pattern pattern);
    Status openPattern(ProductHandle source, Pattern pattern, std::vector<Pattern> comparisons = {},
                       bool synchrotron = false, std::optional<OriginalSignalProvenance> = {});
    void replaceCandidates(std::vector<ProductCard> cards);
    Status mergeCandidates(ProductHandle source, std::vector<ProductCard> cards);
    Status removeCandidates(ProductHandle source, std::span<const std::string> ids);
    void replacePhases(std::vector<ProductCard> cards);
    void reset();
    Result<ProjectSettings> projectDefaults() const;
    Status saveDefaultSettings(const ProjectSettings &) const;
    Status initializeProjectSettings(ProductState &) const;
    DefaultSettingsStore *defaultSettingsStore() const { return defaults_; }
    Result<struct WorkflowRollback> prepareRollback(std::uint64_t) const;
    Status commitRollback(const struct WorkflowRollback &);
    bool canUndo(std::uint64_t) const;
    // C++ editor owns its only draft; this barrier never stages a Legacy import.
    void beginPreprocessingEdit();
    Status commitPreprocessingEdit(ProductHandle source, ProductState next);
    void cancelPreprocessingEdit(ProductHandle source);
    void beginLegacyEdit(const std::string &operation);
    void endLegacyEdit(const std::string &operation, bool accept);
    // Native preview snapshots are private drafts, never authoritative inputs.
    Status stageLegacyInput(ProductHandle source, ProductState next);
    class Subscription {
        friend class ProductSearchSession;
        struct Observers;
        std::weak_ptr<Observers> observers_;
        std::uint64_t id_ = 0;
        Subscription(std::shared_ptr<Observers>, std::uint64_t);

      public:
        Subscription() = default;
        Subscription(const Subscription &) = delete;
        Subscription &operator=(const Subscription &) = delete;
        Subscription(Subscription &&) noexcept;
        Subscription &operator=(Subscription &&) noexcept;
        ~Subscription();
        void reset() noexcept;
    };
    Subscription subscribe(std::function<void(ProductHandle)> observer);

  private:
    friend class Workspace;
    Status restoreProjectWithPresentation(ProductHandle, ProductState, std::function<void()>);
    Status changeAlgorithms(AlgorithmHandle);
    ProductHandle state_;
    DefaultSettingsStore *defaults_ = nullptr; // Composition outlives this owner.
    struct History;
    std::shared_ptr<History> history_;
    std::thread::id ownerThread_ = std::this_thread::get_id();
    std::shared_ptr<Subscription::Observers> observers_;
    std::map<std::string, ProductHandle> edits_;
    std::optional<ProductState> legacyDraft_;
    ProductHandle legacyDraftSource_;
    ProductHandle editOpened_;
    ProductHandle cppEditOpened_;
};

Status validateProductState(const ProductState &);
Result<ProductState> prepareProductPatternOpen(ProductHandle, Pattern, std::vector<Pattern> = {},
                                               bool = false, std::optional<OriginalSignalProvenance> = {});
Result<PeakSet> productSearchPeaks(const ProductPeaks &);
// Product measured-table boundary, not a global restriction on optional domain fields.
Result<ProductPeaks> productMeasuredPeaks(const PeakSet &, bool requireArea = false,
    std::optional<WavelengthAngstrom> unchangedWavelength = {});
} // namespace qualpf::application
