#pragma once
#include "qualpf/application/peak_coordinates.h"
#include "qualpf/application/product_session.h"
#include "qualpf/application/search_engine.h"
#include "qualpf/application/kalpha2.h"
namespace qualpf::application {
enum class PreprocessingAction { subtract_background, automatic_peaks, prepare_search };
enum class PeakOperation { redetect, reselect, restore_snapshot };
// Manual values are user measurements, never estimates of undefined detection
// output. Row indices are scoped to the edit's immutable source snapshot.
struct ManualPeakChange {
    enum class Operation { add, modify, erase, clear };
    Operation operation = Operation::add;
    std::vector<std::size_t> rows;
    double two_theta = 0, intensity = 0, fwhm = 0;
    std::optional<double> integrated_intensity;
};
struct TablePeakChange {
    ManualPeakChange row;
    PeakCoordinateRepresentation representation = PeakCoordinateRepresentation::two_theta;
    double coordinate = 0;
    bool infer_width = false;
};
struct PreprocessingRequest {
    bool strip_kalpha2 = false;
    std::optional<ManualPeakChange> manual;
    std::optional<TablePeakChange> table;
    std::optional<double> inferred_graph;
    std::optional<std::vector<ParsedPeakCoordinateEntry>> imported;
    bool restart_from_original = false;
    std::optional<AngularRange> crop_range;
    std::optional<preprocessing::BackgroundOptions> background;
    std::optional<preprocessing::SmoothingOptions> smoothing;
    std::optional<preprocessing::PeakOptions> peaks;
    std::optional<PeakSelection> selection;
    std::optional<std::vector<preprocessing::BackgroundPoint>> background_points;
    std::vector<preprocessing::BackgroundFilter> background_filters;
    PeakOperation peak_operation = PeakOperation::redetect;
    bool subtract_background = false;
    bool use_background_for_peaks = false;
    // A peak root may prepare its missing background in the same transaction;
    // its final Search decision still belongs to P1/P2, not a background edit.
    bool peak_product_operation = false;
    bool restore_automatic_window = false; // Legacy sensitivity action's post-detection state.
    WavelengthAngstrom wavelength;
    AngularWidthDegrees tolerance;
};

// Private evaluation workspace: no Session, Qt, settings, SQL or publication.
// It may be evaluated synchronously or on a worker with the same frozen source.
class PreprocessingEvaluation {
  public:
    using Stripper = Result<std::vector<double>> (*)(const Pattern &);
    explicit PreprocessingEvaluation(ProductHandle source, Stripper stripper = stripKalpha2Cpp)
        : stripper_(stripper), source_(std::move(source)) {}
    Status preview(const PreprocessingRequest &, std::stop_token = {});
    Status previewRange(const PreprocessingRequest &, std::stop_token = {});
    const ProductState *preview() const { return draft_ ? &*draft_ : nullptr; }
    std::size_t candidateCount() const { return candidateCount_; }
    void clear() { draft_.reset(); }

  private:
    Status evaluate(const PreprocessingRequest &);
    Stripper stripper_;
    ProductHandle source_;
    std::optional<ProductState> draft_;
    std::size_t candidateCount_ = 0;
    std::optional<PeakSet> detected_;
    std::optional<preprocessing::PeakOptions> detectedOptions_;
    std::optional<ProductState> detectionInput_;
    bool detectionFailed_ = false;
};

// Owner-side editor admission and publication, independent of evaluation.
class ProductPreprocessingEdit {
  public:
    using Stripper = PreprocessingEvaluation::Stripper;
    explicit ProductPreprocessingEdit(ProductSearchSession &, bool interactive = false,
                                      Stripper = stripKalpha2Cpp);
    ~ProductPreprocessingEdit();
    ProductPreprocessingEdit(const ProductPreprocessingEdit &) = delete;
    ProductPreprocessingEdit &operator=(const ProductPreprocessingEdit &) = delete;
    Status preview(const PreprocessingRequest &);
    Status previewRange(const PreprocessingRequest &);
    const ProductState *preview() const { return evaluation_->preview(); }
    std::size_t candidateCount() const { return evaluation_->candidateCount(); }
    Status commit();
    void cancel();

  private:
    Status admission();
    ProductSearchSession &session_;
    ProductHandle source_;
    std::unique_ptr<PreprocessingEvaluation> evaluation_;
    bool closed_ = false, interactive_ = false, stale_ = false;
};
enum class PeakEditOrigin { detected, manual, inferred_graph, table, imported };
bool peakMatchingEquivalent(const ProductState &, const ProductState &);
bool peakEditorEquivalent(const ProductState &, const ProductState &);
Status finalizePeakEdit(ProductState &, const ProductState &, PeakEditOrigin);
Status prepareManualPeaks(ProductState &, const ManualPeakChange &, WavelengthAngstrom, AngularWidthDegrees);
Result<Pattern> preparePeakMeasurementContext(const ProductState &, WavelengthAngstrom);
Status preparePeakOperation(ProductState &, const PreprocessingRequest &);
bool rangeNumericalEquivalent(const Pattern &, const Pattern &);
void finalizeRangeState(ProductState &, const ProductState &);
bool hasActiveBackground(const Pattern &);
Result<PreprocessingRequest> automaticPreprocessingRequest(ProductHandle, PreprocessingAction);
Result<PreprocessingRequest> backgroundPointRequest(ProductHandle, bool add, double x, double y, bool redetect);
} // namespace qualpf::application
