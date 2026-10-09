#pragma once
#include "qualpf/application/preprocessing_configuration.h"
#include "qualpf/application/product_values.h"
#include "qualpf/search/classic.h"
#include <functional>
#include <map>
#include <memory>
#include <thread>
namespace qualpf::application {
enum class SearchBackend { cpp, legacy_fallback };
struct PowderCifProvenance;
struct ProductState {
    SearchBackend backend = SearchBackend::cpp;
    bool editing = false;
    std::uint64_t revision = 0;
    double automaticDelta = 0.5;
    std::optional<double> peakTolerance;
    MetadataObject searchOptions;
    MetadataObject restraints;
    Pattern pattern;
    std::shared_ptr<const PowderCifProvenance> powderCifProvenance;
    std::vector<Pattern> comparisonPatterns; // Imported overlays; first pattern alone drives search.
    ProductPeaks peaks;
    std::vector<ProductCard> candidates;
    std::vector<ProductCard> phases;
    MetadataObject projectMetadata;
    MetadataObject projectEnvelope; // Unknown QXP fields, with exact integer/null types.
    PreprocessingConfiguration preprocessing;
};
using ProductHandle = std::shared_ptr<const ProductState>;

class ProductSearchSession {
  public:
    explicit ProductSearchSession(SearchBackend backend = SearchBackend::cpp);
    explicit ProductSearchSession(ProductState initial);
    ProductSearchSession(const ProductSearchSession &) = delete;
    ProductSearchSession &operator=(const ProductSearchSession &) = delete;
    ProductHandle snapshot() const { return state_; }
    Status commit(ProductHandle source, ProductState next);
    void replacePeaks(ProductPeaks peaks);
    void replacePattern(Pattern pattern);
    Status openPattern(ProductHandle source, Pattern pattern, std::vector<Pattern> comparisons = {},
                       bool synchrotron = false);
    void replaceCandidates(std::vector<ProductCard> cards);
    void replacePhases(std::vector<ProductCard> cards);
    void reset();
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
    ProductHandle state_;
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
                                               bool = false);
Result<PeakSet> productSearchPeaks(const ProductPeaks &);
} // namespace qualpf::application
