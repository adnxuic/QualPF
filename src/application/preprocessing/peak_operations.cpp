#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/peaks/measurement.h"
#include <algorithm>
#include <cmath>

namespace qualpf::application {
namespace {
Status reject(const char *field, const char *reason) {
    return Status{Error{ErrorCode::invalid_input, field, reason, reason}};
}
std::vector<peaks::NeighborPeak> neighbors(const ProductPeaks &p, std::optional<std::size_t> omit = {}) {
    std::vector<peaks::NeighborPeak> out;
    for (std::ptrdiff_t i = 0; i < p.tth.size(); ++i)
        if (!omit || std::size_t(i) != *omit)
            out.push_back({TwoThetaDegrees{p.tth[i]}, AngularWidthDegrees{p.fwhm[i]}});
    return out;
}
// Reuse G0's existing measured-value adapter, without its pre-P1/P2 state
// policy.
Status rowChange(ProductState &next, const ManualPeakChange &change, WavelengthAngstrom wave,
                 std::optional<double> originalD = {}) {
    const auto before = next;
    auto status =
        prepareManualPeaks(next, change, wave, AngularWidthDegrees{before.peakTolerance.value_or(.15)});
    if (!status)
        return status;
    next.preprocessing = before.preprocessing;
    next.projectMetadata = before.projectMetadata;
    next.candidates = before.candidates;
    next.phases = before.phases;
    next.peakTolerance = before.peakTolerance;
    next.automaticDelta = before.automaticDelta;
    if (change.operation == ManualPeakChange::Operation::add ||
        change.operation == ManualPeakChange::Operation::modify) {
        const auto at = std::find(next.peaks.tth.begin(), next.peaks.tth.end(), change.two_theta);
        const auto row = std::ptrdiff_t(at - next.peaks.tth.begin());
        if (change.operation == ManualPeakChange::Operation::modify) {
            const auto old = std::ptrdiff_t(change.rows.front());
            if (change.intensity == before.peaks.intensity[old])
                next.peaks.intensityOrig[row] = before.peaks.intensityOrig[old];
            if (change.two_theta == before.peaks.tth[old] && !originalD) {
                next.peaks.d[row] = before.peaks.d[old];
                next.peaks.deltaD[row] = before.peaks.deltaD[old];
            }
        }
        if (originalD) {
            next.peaks.d[row] = *originalD;
            auto adjusted = productPeaksWithTolerance(next.peaks, before.peakTolerance.value_or(.15));
            if (!adjusted)
                return Status{adjusted.error()};
            next.peaks.deltaD[row] = adjusted.value().deltaD[row];
        }
    }
    return Status{std::monostate{}};
}
} // namespace
Result<Pattern> preparePeakMeasurementContext(const ProductState &state, WavelengthAngstrom wave) {
    auto p = state.pattern;
    const auto fail = [](const char *reason) {
        return Result<Pattern>{Error{ErrorCode::missing_data, "peaks.background", reason, reason}};
    };
    if (!p.processing.background_subtracted)
        return fail("subtraction_state_unknown");
    if (p.background && (!p.background->window || p.background->window->first != 0 ||
                         p.background->values.size() != p.observed.size()))
        return fail("background_alignment_unknown");
    if (*p.processing.background_subtracted) {
        if (!p.background || !std::all_of(p.background->values.begin(), p.background->values.end(),
                                          [](double v) { return v == 0; }))
            return fail("background_alignment_unknown");
    } else if (!p.background) {
        auto background = preprocessing::background(
            p, state.preprocessing.background.value_or(preprocessing::BackgroundOptions{}), wave);
        if (!background)
            return Result<Pattern>{background.error()};
        p.background = SampledSignal{std::move(background).value().values, SampleWindow{0}};
        if (p.background->values.size() != p.observed.size())
            return fail("background_alignment_unknown");
    }
    return Result<Pattern>{std::move(p)};
}
Status preparePeakOperation(ProductState &next, const PreprocessingRequest &request) {
    const auto opening = next;
    auto origin = PeakEditOrigin::manual;
    const auto wave = request.wavelength;
    if (request.manual) {
        auto status = rowChange(next, *request.manual, wave);
        if (!status)
            return status;
    } else if (request.table) {
        origin = PeakEditOrigin::table;
        auto change = request.table->row;
        if (change.operation != ManualPeakChange::Operation::add &&
            change.operation != ManualPeakChange::Operation::modify)
            return reject("peaks.table", "invalid_table_operation");
        if (change.operation == ManualPeakChange::Operation::modify &&
            (change.rows.size() != 1 || change.rows[0] >= std::size_t(next.peaks.tth.size())))
            return reject("peaks.table", "invalid_row_index");
        std::optional<double> originalD;
        if (request.table->representation == PeakCoordinateRepresentation::d_spacing) {
            originalD = request.table->coordinate;
            if (!std::isfinite(*originalD) || *originalD <= 0 || !std::isfinite(wave.value) ||
                wave.value <= 0)
                return reject("peaks.coordinate", "out_of_domain_display_coordinate");
            auto angle = preprocessing::legacy_two_theta(DSpacingAngstrom{*originalD}, wave);
            if (!angle)
                return Status{angle.error()};
            change.two_theta = angle.value().value;
        } else
            change.two_theta = request.table->coordinate;
        if (!std::isfinite(change.two_theta) || change.two_theta <= 0 || change.two_theta >= 180)
            return reject("peaks.coordinate", "out_of_domain_display_coordinate");
        if (request.table->infer_width) {
            auto list =
                neighbors(next.peaks, change.rows.empty() ? std::nullopt : std::optional{change.rows[0]});
            auto width = peaks::infer_table_width(TwoThetaDegrees{change.two_theta}, list);
            if (!width)
                return Status{width.error()};
            change.fwhm = width.value().value;
        }
        auto status = rowChange(next, change, wave, originalD);
        if (!status)
            return status;
    } else {
        if (!std::isfinite(wave.value) || wave.value <= 0)
            return reject("peaks.coordinate", "invalid_wavelength");
        if (request.inferred_graph) {
            origin = PeakEditOrigin::inferred_graph;
            auto context = preparePeakMeasurementContext(next, wave);
            if (!context)
                return Status{context.error()};
            auto result = peaks::measure_graph_peak(context.value(), TwoThetaDegrees{*request.inferred_graph},
                                                    neighbors(next.peaks), wave);
            if (!result)
                return Status{result.error()};
            const auto &p = result.value().peak;
            if (p.intensity < 0)
                return reject("peaks.measurement", "unsupported_negative_peak_intensity");
            if (!next.peaks.tth.empty() &&
                next.peaks.integratedIntensity.has_value() != p.integrated_intensity.has_value())
                return reject("peaks.measurement", "incompatible_area_presence");
            ManualPeakChange change;
            change.two_theta = p.two_theta.value;
            change.intensity = p.intensity;
            change.fwhm = p.fwhm.value;
            change.integrated_intensity = p.integrated_intensity;
            // M1 consumes the prepared Pattern wavelength. Appending must retain
            // the existing snapshot's wavelength (detector DTOs use binary32).
            if (!next.peaks.tth.empty() && float(next.peaks.wave) != float(wave.value))
                return reject("peaks.coordinate", "incompatible_snapshot_wavelength");
            const auto rowWave = next.peaks.tth.empty() ? wave : WavelengthAngstrom{next.peaks.wave};
            auto status = rowChange(next, change, rowWave, p.d_spacing.value);
            if (!status)
                return status;
        } else if (request.imported) {
            origin = PeakEditOrigin::imported;
            std::vector<peaks::ImportedCoordinate> coordinates;
            for (const auto &entry : *request.imported) {
                if (!std::isfinite(entry.value) || entry.value <= 0)
                    return reject("peaks.coordinate", "out_of_domain_display_coordinate");
                if (entry.representation == PeakCoordinateRepresentation::d_spacing) {
                    auto t = preprocessing::legacy_two_theta(DSpacingAngstrom{entry.value}, wave);
                    if (!t)
                        return Status{t.error()};
                    if (!std::isfinite(t.value().value) || t.value().value <= 0 || t.value().value >= 180)
                        return reject("peaks.coordinate", "out_of_domain_display_coordinate");
                    coordinates.push_back({t.value(), DSpacingAngstrom{entry.value}});
                } else {
                    if (entry.value >= 180)
                        return reject("peaks.coordinate", "out_of_domain_display_coordinate");
                    coordinates.push_back({TwoThetaDegrees{entry.value}, {}});
                }
            }
            auto context = preparePeakMeasurementContext(next, wave);
            if (!context)
                return Status{context.error()};
            auto result = peaks::measure_imported_peaks(context.value(), coordinates, wave);
            if (!result)
                return Status{result.error()};
            ProductPeaks p;
            p.valid = true;
            p.wave = wave.value;
            p.integratedIntensity.emplace();
            for (const auto &measurement : result.value()) {
                const auto &m = measurement.peak;
                if (m.intensity < 0)
                    return reject("peaks.measurement", "unsupported_negative_peak_intensity");
                p.tth.push_back(m.two_theta.value);
                p.d.push_back(m.d_spacing.value);
                p.deltaD.push_back(0);
                p.intensity.push_back(m.intensity);
                p.intensityOrig.push_back(m.original_intensity);
                p.fwhm.push_back(m.fwhm.value);
                p.integratedIntensity->push_back(*m.integrated_intensity);
            }
            auto adjusted = productPeaksWithTolerance(p, next.peakTolerance.value_or(.15));
            if (!adjusted)
                return Status{adjusted.error()};
            next.peaks = std::move(adjusted).value();
        } else
            return reject("peaks.operation", "missing_peak_operation");
    }
    return finalizePeakEdit(next, opening, origin);
}
} // namespace qualpf::application
