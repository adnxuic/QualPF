#include "qualpf/application/preprocessing_evaluation.h"

namespace qualpf::application {
namespace {
bool columnsEqual(const ProductPeaks &a, const ProductPeaks &b) {
    return a.valid == b.valid && a.wave == b.wave && a.tth == b.tth && a.d == b.d && a.deltaD == b.deltaD &&
           a.fwhm == b.fwhm && a.intensity == b.intensity && a.intensityOrig == b.intensityOrig;
}
MetadataObject peakConfiguration(const PreprocessingConfiguration &c) {
    const auto encoded = preprocessingValues(c);
    MetadataObject out;
    for (const auto *key : {"candidate_pool", "candidate_failure", "peaks", "selection"})
        if (encoded.contains(key))
            out[key] = encoded.at(key);
    if (out.contains("candidate_pool")) {
        auto &pool = std::get<MetadataObject>(out["candidate_pool"].data);
        auto &rows = std::get<MetadataArray>(pool["rows"].data);
        for (auto &row : rows)
            std::get<MetadataObject>(row.data).erase("id");
    }
    return out;
}
const MetadataValue &at(const MetadataObject &object, const char *key) {
    static const MetadataValue absent;
    auto found = object.find(key);
    return found == object.end() ? absent : found->second;
}
} // namespace
bool peakMatchingEquivalent(const ProductState &a, const ProductState &b) {
    return columnsEqual(a.peaks, b.peaks) && a.peakTolerance == b.peakTolerance &&
           a.automaticDelta == b.automaticDelta;
}
bool peakEditorEquivalent(const ProductState &a, const ProductState &b) {
    if (!peakMatchingEquivalent(a, b) || a.peaks.integratedIntensity != b.peaks.integratedIntensity ||
        peakConfiguration(a.preprocessing) != peakConfiguration(b.preprocessing))
        return false;
    for (const auto *key : {"peak_snapshot_origin", "candidate_pool_state", "residual_current_known"})
        if (at(a.projectMetadata, key) != at(b.projectMetadata, key))
            return false;
    return true;
}
Status finalizePeakEdit(ProductState &next, const ProductState &opening, PeakEditOrigin origin) {
    const bool manual = origin != PeakEditOrigin::detected;
    // Test raw final values before resetting residual or stamping provenance.
    if (manual && columnsEqual(next.peaks, opening.peaks) &&
        next.peaks.integratedIntensity == opening.peaks.integratedIntensity) {
        next.automaticDelta = opening.automaticDelta;
        next.peakTolerance = opening.peakTolerance;
        return validateProductState(next);
    }
    if (manual) {
        // Restore surviving rows before deriving the new width-based delta.
        // Never reconstruct removed rows from the detector pool.
        if (!columnsEqual(next.peaks, opening.peaks))
            next.peaks.intensity = next.peaks.intensityOrig;
        next.peakTolerance = opening.peakTolerance;
        next.automaticDelta = .5;
        if (next.peaks.valid) {
            auto peaks = productSearchPeaks(next.peaks);
            if (!peaks)
                return Status{peaks.error()};
            auto delta = preprocessing::automatic_tolerance(peaks.value());
            if (!delta)
                return Status{delta.error()};
            next.automaticDelta = delta.value();
        }
    }
    auto valid = validateProductState(next);
    if (!valid)
        return valid;
    const bool sameMatching = peakMatchingEquivalent(next, opening);
    // Detection's origin is assigned only if a persisted editor value changed.
    if (!manual && peakEditorEquivalent(next, opening))
        return Status{std::monostate{}};
    if (manual) {
        next.preprocessing.peak_candidates.reset();
        next.preprocessing.candidate_failure.reset();
        next.preprocessing.selection.reset();
    }
    const char *name = "detected", *pool = "complete_finite";
    switch (origin) {
    case PeakEditOrigin::detected:
        break;
    case PeakEditOrigin::manual:
        name = "manual";
        pool = "invalidated_by_manual_edit";
        break;
    case PeakEditOrigin::inferred_graph:
        name = "inferred_graph";
        pool = "invalidated_by_inferred_graph";
        break;
    case PeakEditOrigin::table:
        name = "table";
        pool = "invalidated_by_table_edit";
        break;
    case PeakEditOrigin::imported:
        name = "imported";
        pool = "unavailable_after_import";
        break;
    }
    next.projectMetadata["peak_snapshot_origin"] = name;
    next.projectMetadata["candidate_pool_state"] = pool;
    if (!sameMatching) {
        next.peaks.intensity = next.peaks.intensityOrig;
        next.candidates.clear();
        next.phases.clear();
        next.projectMetadata["residual_current_known"] = true;
    }
    return validateProductState(next);
}
} // namespace qualpf::application
