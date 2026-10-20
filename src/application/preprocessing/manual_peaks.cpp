#include "qualpf/application/preprocessing_evaluation.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <set>

namespace qualpf::application {
Status prepareManualPeaks(ProductState &next, const ManualPeakChange &change, WavelengthAngstrom wave,
                          AngularWidthDegrees tolerance) {
    auto &p = next.peaks;
    const auto fail = [](const char *message) {
        return Status{Error{ErrorCode::invalid_input, "peaks.manual", message, "invalid_manual_peak"}};
    };
    using Op = ManualPeakChange::Operation;
    std::set<std::size_t> rows(change.rows.begin(), change.rows.end());
    if (rows.size() != change.rows.size() || (!rows.empty() && *rows.rbegin() >= std::size_t(p.tth.size())))
        return fail("Invalid or duplicate snapshot row");
    if ((change.operation == Op::modify && rows.size() != 1) ||
        (change.operation == Op::erase && rows.empty()) ||
        ((change.operation == Op::add || change.operation == Op::clear) && !rows.empty()))
        return fail("Invalid rows for manual operation");
    if (change.operation == Op::add || change.operation == Op::modify) {
        if (!std::isfinite(change.two_theta) || change.two_theta <= 0 || change.two_theta >= 180 ||
            !std::isfinite(change.intensity) || change.intensity < 0 || !std::isfinite(change.fwhm) ||
            change.fwhm <= 0 || (change.integrated_intensity && !std::isfinite(*change.integrated_intensity)))
            return fail("Enter finite position, nonnegative intensity and positive measured FWHM");
        if (p.integratedIntensity && !change.integrated_intensity)
            return fail("This snapshot requires an explicit integrated intensity");
        if (!p.tth.empty() && !p.integratedIntensity && change.integrated_intensity)
            return fail(
                "Historical snapshot has no area column; mixed known/unknown areas are not supported");
        if (!p.tth.empty() && wave.value != p.wave)
            return fail("Manual editing cannot change the snapshot wavelength");
        auto d = preprocessing::legacy_d(TwoThetaDegrees{change.two_theta}, wave);
        if (!d)
            return Status{d.error()};
        const auto row = change.operation == Op::add ? p.tth.size() : std::ptrdiff_t(*rows.begin());
        for (std::ptrdiff_t i = 0; i < p.tth.size(); ++i)
            if (i != row && p.tth[i] == change.two_theta)
                return fail("A peak already exists at this position");
        if (change.operation == Op::add) {
            if (p.tth.empty() && change.integrated_intensity)
                p.integratedIntensity.emplace();
            p.tth.push_back(0);
            p.d.push_back(0);
            p.deltaD.push_back(0);
            p.intensity.push_back(0);
            p.intensityOrig.push_back(0);
            p.fwhm.push_back(0);
            if (p.integratedIntensity)
                p.integratedIntensity->push_back(0);
        }
        p.tth[row] = change.two_theta;
        p.d[row] = d.value().value;
        p.intensity[row] = p.intensityOrig[row] = change.intensity;
        p.fwhm[row] = change.fwhm;
        if (p.integratedIntensity)
            (*p.integratedIntensity)[row] = *change.integrated_intensity;
        p.wave = wave.value;
        p.valid = true;
        auto adjusted = productPeaksWithTolerance(p, tolerance.value);
        if (!adjusted)
            return Status{adjusted.error()};
        // A manual edit must not renormalize or re-export untouched rows.
        p.deltaD[row] = adjusted.value().deltaD[row];
    }
    std::vector<std::ptrdiff_t> order;
    for (std::ptrdiff_t i = 0; i < p.tth.size(); ++i)
        if (change.operation != Op::clear &&
            !(change.operation == Op::erase && rows.contains(std::size_t(i))))
            order.push_back(i);
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return p.tth[a] < p.tth[b]; });
    auto reorder = [&](std::vector<double> &column) {
        std::vector<double> values;
        values.reserve(order.size());
        for (auto row : order)
            values.push_back(column[row]);
        column = std::move(values);
    };
    reorder(p.tth);
    reorder(p.d);
    reorder(p.deltaD);
    reorder(p.intensity);
    reorder(p.intensityOrig);
    reorder(p.fwhm);
    if (p.integratedIntensity)
        reorder(*p.integratedIntensity);
    if (p.tth.empty())
        p.clear();
    next.automaticDelta = .5;
    if (p.valid) {
        auto converted = productSearchPeaks(p);
        if (!converted)
            return Status{converted.error()};
        auto automatic = preprocessing::automatic_tolerance(converted.value());
        if (!automatic)
            return Status{automatic.error()};
        next.automaticDelta = automatic.value();
    }
    next.peakTolerance = tolerance.value;
    next.preprocessing.peak_candidates.reset();
    next.preprocessing.peak_calibration.reset();
    next.preprocessing.fwhm_estimate.reset();
    next.preprocessing.rejected_candidates.clear();
    next.preprocessing.candidate_failure.reset();
    next.preprocessing.selection.reset();
    next.projectMetadata["peak_snapshot_origin"] = "manual";
    next.projectMetadata["candidate_pool_state"] = "invalidated_by_manual_edit";
    next.candidates.clear();
    next.phases.clear();
    return validateProductState(next);
}
} // namespace qualpf::application
