#include "qualpf/application/preprocessing_configuration.h"
#include "qualpf/domain/validation.h"
#include <cmath>
namespace qualpf::application {
Status validatePreprocessing(const PreprocessingConfiguration &c) {
    const auto error = Error{ErrorCode::invalid_input, "preprocessing.configuration",
                             "Invalid preprocessing configuration", "invalid_configuration"};
    const auto real = [](double v, double lo, double hi) { return std::isfinite(v) && v >= lo && v <= hi; };
    const auto integer = [](int v, int lo, int hi) { return v >= lo && v <= hi; };
    const auto range = [&](const AngularRange &r) {
        return real(r.minimum.value, 0, 180) && real(r.maximum.value, 0, 180) &&
               r.minimum.value <= r.maximum.value;
    };
    if (c.candidate_failure &&
        (*c.candidate_failure != "nonfinite_candidate_measurement" || c.peak_candidates))
        return Status{error};
    if (c.peak_candidates) {
        const auto &p = *c.peak_candidates;
        if (!p.effective_wavelength || !real(p.effective_wavelength->value, 1e-12, 8) ||
            !integer(int(p.source_precision), 0, 2) || !integer(int(p.intensity_scale), 0, 2) ||
            !validate(p).empty())
            return Status{error};
        for (std::size_t i = 0; i < p.peaks.size(); ++i) {
            const auto &row = p.peaks[i];
            if (!row.d_spacing || !row.delta_d || !row.fwhm || !row.original_intensity ||
                !std::isfinite(row.two_theta.value) || !std::isfinite(row.intensity) ||
                !std::isfinite(row.d_spacing->value) || !std::isfinite(row.delta_d->value) ||
                !std::isfinite(row.fwhm->value) || !std::isfinite(*row.original_intensity) ||
                (row.integrated_intensity && !std::isfinite(*row.integrated_intensity)) ||
                (i && row.two_theta.value < p.peaks[i - 1].two_theta.value))
                return Status{error};
        }
    }
    if (c.background_points) {
        for (std::size_t i = 0; i < c.background_points->size(); ++i) {
            const auto p = (*c.background_points)[i];
            if (!std::isfinite(p.x) || !std::isfinite(p.y) || (i && p.x < (*c.background_points)[i - 1].x))
                return Status{error};
        }
    }
    for (const auto &f : c.background_filters)
        if (!real(f.minimum, 0, 180) || !real(f.maximum, 0, 180) || f.minimum > f.maximum ||
            !integer(f.iterations, 0, 10000) || !integer(f.half_window, 1, 100000))
            return Status{error};
    if (c.selection) {
        const auto &s = *c.selection;
        if (!real(s.minimum, 0, 180) || !real(s.maximum, 0, 180) || s.minimum > s.maximum ||
            !real(s.threshold_percent, 0, 100) || !integer(s.maximum_peaks, -1, 1000000))
            return Status{error};
    }
    if (c.background) {
        const auto &o = *c.background;
        if (!integer(int(o.method), 0, 6) || !integer(o.coefficients, 0, 36) ||
            !integer(o.iterations, 0, 100000) || !integer(o.half_window, 0, 100000) ||
            (o.bruckner_range && !range(*o.bruckner_range)))
            return Status{error};
    }
    if (c.smoothing) {
        const auto &o = *c.smoothing;
        if (!integer(int(o.method), 0, 1) || !integer(o.half_window, 1, 100000) ||
            !integer(o.polynomial_degree, 0, 20) || !integer(o.derivative, 0, 0))
            return Status{error};
    }
    if (c.peaks) {
        const auto &o = *c.peaks;
        if (!real(o.threshold_percent, 0, 100) || !real(o.slope, 0, 1e6) || !real(o.search_range, -1, 180) ||
            !integer(o.half_window, 1, 100) || !integer(o.polynomial_degree, 3, 20) ||
            !integer(o.maximum_peaks, -1, 1000000) || !integer(o.minimum_peaks, -1, 1000000) ||
            (o.detection_range && !range(*o.detection_range)))
            return Status{error};
    }
    return Status{std::monostate{}};
}
} // namespace qualpf::application
