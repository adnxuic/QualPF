#include "qualpf/application/presentation/plot_snapshot.h"
#include "qualpf/application/pfsm.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include <tuple>

namespace qualpf::application::presentation {
namespace {
Error invalid(std::string reason) {
    return {ErrorCode::invalid_input, "plot.projection", "Invalid or unsupported plot input",
            std::move(reason)};
}
bool finite(const std::vector<double> &v, Precision p) {
    return std::all_of(v.begin(), v.end(), [p](double x) {
        return std::isfinite(x) && (p != Precision::binary32_transport || std::isfinite(float(x)));
    });
}
std::string style(SeriesKind kind) {
    constexpr const char *keys[]{"observed",
                                 "calculated",
                                 "background",
                                 "background_points",
                                 "difference",
                                 "cumulative",
                                 "reflections",
                                 "peaks",
                                 "smoothing",
                                 "unindexed_peaks",
                                 "systematic_absences",
                                 "selected_reflections",
                                 "selected_peaks",
                                 "intervals",
                                 "profile_curves",
                                 "accepted_card",
                                 "search_residual",
                                 "preprocessing_preview"};
    return keys[static_cast<std::size_t>(kind)];
}
std::vector<double> span(const std::vector<double> &v, std::size_t first, std::size_t count, Precision p) {
    std::vector<double> result(v.begin() + first, v.begin() + first + count);
    if (p == Precision::binary32_transport)
        for (auto &x : result)
            x = float(x);
    return result;
}
std::vector<double> coordinates(const Pattern &p) {
    std::vector<double> x;
    for (auto v : p.two_theta)
        x.push_back(v.value);
    return x;
}
PlotSeries series(SeriesKind kind, std::string id, std::vector<double> x, std::vector<double> y, double wave,
                  Precision precision) {
    PlotSeries result;
    result.identity = std::move(id);
    result.kind = kind;
    result.style_identity = style(kind);
    result.x = std::move(x);
    result.y = std::move(y);
    result.wavelength = wave;
    result.precision = precision;
    return result;
}
Status append(PlotUpdate &out, PlotSeries s, Abscissa abscissa) {
    if (s.x.size() != s.y.size())
        return Status{invalid("invalid_layer_dimensions")};
    auto status = project_coordinates(s, abscissa);
    if (!status)
        return status;
    out.snapshot.series.push_back(s);
    out.commands.push_back(
        {s.update == UpdatePolicy::erase ? CommandKind::erase : CommandKind::upsert, 1, std::move(s)});
    return Status{std::monostate{}};
}
Status valid_pattern(const PatternLayerInput &input, Precision precision) {
    const auto &p = input.pattern;
    auto x = coordinates(p);
    if (x.empty() || x.size() != p.observed.size() || !finite(x, precision) || !finite(p.observed, precision))
        return Status{invalid("invalid_pattern_dimensions_or_values")};
    if (p.radiation.lines.empty() || !std::isfinite(p.radiation.lines.front().wavelength.value) ||
        p.radiation.lines.front().wavelength.value <= 0 ||
        (precision == Precision::binary32_transport &&
         !std::isfinite(float(p.radiation.lines.front().wavelength.value))))
        return Status{invalid("invalid_wavelength")};
    if (!std::is_sorted(x.begin(), x.end()) || std::adjacent_find(x.begin(), x.end()) != x.end())
        return Status{invalid("unordered_or_duplicate_coordinates")};
    if (input.first > x.size() || input.count > x.size() - input.first)
        return Status{invalid("invalid_calculation_span")};
    return Status{std::monostate{}};
}
// clocate presentation semantics: clamp endpoints, closest sample, upper tie.
// Subtractions and comparisons are explicitly binary32, as in the native view.
std::size_t nearest(const std::vector<double> &x, float value) {
    if (value <= float(x.front()))
        return 0;
    if (value >= float(x.back()))
        return x.size() - 1;
    std::size_t low = 0, high = x.size() - 1;
    while (high - low > 1) {
        const auto mid = (high + low) / 2;
        if (value >= float(x[mid]))
            low = mid;
        else
            high = mid;
    }
    return float(value - float(x[low])) < float(float(x[high]) - value) ? low : high;
}
} // namespace

Status project_coordinates(PlotSeries &s, Abscissa axis) {
    if (axis < Abscissa::two_theta || axis > Abscissa::inverse_d_squared)
        return Status{invalid("invalid_abscissa")};
    if ((!s.y.empty() && s.x.size() != s.y.size()) || !finite(s.x, s.precision) || !finite(s.y, s.precision))
        return Status{invalid("invalid_series_dimensions_or_values")};
    if (axis == Abscissa::two_theta || s.x.empty())
        return Status{std::monostate{}};
    if (!std::isfinite(s.wavelength) || s.wavelength <= 0)
        return Status{invalid("invalid_coordinate_wavelength")};
    auto x = s.x;
    constexpr double dtor = 3.14159265358979323846 / 180;
    for (auto &value : x) {
        value = s.wavelength / (2.0 * std::sin(value * 0.5 * dtor));
        if (axis == Abscissa::inverse_d)
            value = 1 / value;
        else if (axis == Abscissa::inverse_d_squared)
            value = 1 / (value * value);
        if (!std::isfinite(value))
            return Status{invalid("nonfinite_coordinate_projection")};
    }
    if (axis == Abscissa::d_spacing) {
        std::reverse(x.begin(), x.end());
        std::reverse(s.y.begin(), s.y.end());
    }
    s.x = std::move(x);
    return Status{std::monostate{}};
}

bool can_subtract_background(const Pattern &p) noexcept {
    return !p.observed.empty() && p.processing.background_subtracted == false && p.background &&
        p.background->window && p.background->window->first == 0 &&
        p.background->values.size() == p.observed.size() && p.two_theta.size() == p.observed.size() &&
        std::all_of(p.background->values.begin(), p.background->values.end(), [](double v) { return std::isfinite(v); });
}
bool can_subtract_background(const ProductState &source) noexcept {
    if (!source.pfsm) return can_subtract_background(source.pattern);
    if (!source.pfsm->problem || !source.pfsm->problem->observations) return false;
    const auto &data = *source.pfsm->problem->observations;
    return !data.observed.empty() && data.observed.size() == data.background.size() &&
        data.observed.size() == data.coordinates.size();
}
static Result<PlotInput> display_input(const PlotInput &raw) {
    auto in = raw;
    if (!in.presentation.subtract_background || in.mode == PresentationMode::headless || in.patterns.empty())
        return Result<PlotInput>{std::move(in)};
    auto &layer = in.patterns.front(); auto &p = layer.pattern;
    if (!can_subtract_background(p)) return Result<PlotInput>{invalid("background_alignment_unknown")};
    for (std::size_t i = 0; i < p.observed.size(); ++i) p.observed[i] -= p.background->values[i];
    // Compatibility calculated arrays already contain the phase contribution.
    // Keep that convention in this temporary display buffer only.
    p.background->values.assign(p.observed.size(), 0);
    p.processing.background_subtracted = true;
    layer.background_enabled = false;
    layer.background_points_x.clear(); layer.background_points_y.clear();
    in.presentation.layers[std::size_t(PlotLayer::background)] = Visibility::hidden;
    in.presentation.layers[std::size_t(PlotLayer::background_points)] = Visibility::hidden;
    return Result<PlotInput>{std::move(in)};
}
Result<PlotUpdate> project_compatibility_frame(const PlotInput &raw, PlotRequest request) {
    auto projected = display_input(raw);
    if (!projected) return Result<PlotUpdate>{projected.error()};
    const auto &in = projected.value();
    PlotUpdate out;
    out.snapshot.abscissa = request.abscissa;
    out.snapshot.reverse_axis = request.abscissa == Abscissa::d_spacing;
    if (in.mode == PresentationMode::headless)
        return Result<PlotUpdate>{std::move(out)};
    if (in.patterns.empty())
        return Result<PlotUpdate>{invalid("missing_pattern")};
    for (const auto &p : in.patterns) {
        auto status = valid_pattern(p, Precision::binary32_transport);
        if (!status)
            return Result<PlotUpdate>{status.error()};
    }
    const auto &layers = in.presentation.layers;
    auto visible = [&](PlotLayer layer) {
        return layers[static_cast<std::size_t>(layer)] == Visibility::visible;
    };
    // Cumulative contains an unset loop index / potentially absent calculated
    // buffer in Legacy. It has no normal-workflow producer in the sealed oracle.
    if (visible(PlotLayer::cumulative))
        return Result<PlotUpdate>{invalid("legacy_cumulative_requires_separate_contract")};
    out.commands.push_back({CommandKind::rescale, request.rescale, {}});
    out.commands.push_back({CommandKind::replace_canvas, 1, {}});
    for (std::size_t i = 0; i < in.patterns.size(); ++i) {
        const auto &p = in.patterns[i];
        if (p.observed_visibility != Visibility::visible)
            continue;
        auto s = series(SeriesKind::observed, "observed:" + std::to_string(i), coordinates(p.pattern),
                        p.pattern.observed, float(p.pattern.radiation.lines.front().wavelength.value),
                        Precision::binary32_transport);
        for (auto &x : s.x)
            x = float(x);
        for (auto &y : s.y)
            y = float(y);
        s.label = p.display_name;
        auto status = append(out, std::move(s), request.abscissa);
        if (!status)
            return Result<PlotUpdate>{status.error()};
    }
    const auto &p = in.patterns.front();
    const auto &pattern = p.pattern;
    // Legacy's last dataset chooses calculation span/guards, then plots primary
    // arrays. Active calculated/difference/smoothing on multiple datasets is not
    // a defined supported producer: reject before exposing any sink event.
    const auto &last = in.patterns.back().pattern;
    const bool last_has_calculated = last.calculated && last.background;
    if (in.patterns.size() > 1 &&
        ((last_has_calculated && (visible(PlotLayer::calculated) || visible(PlotLayer::difference))) ||
         (visible(PlotLayer::smoothing) && in.patterns.front().smoothing_preview)))
        return Result<PlotUpdate>{invalid("secondary_calculated_layer_requires_separate_contract")};
    const auto first = in.patterns.back().first;
    const auto count = in.patterns.back().count;
    const auto x = coordinates(pattern);
    const double wave = float(pattern.radiation.lines.front().wavelength.value);
    if (first > x.size() || count > x.size() - first)
        return Result<PlotUpdate>{invalid("primary_calculation_span_mismatch")};
    auto full = [](const std::optional<SampledSignal> &s, std::size_t n) {
        return s && s->window && s->window->first == 0 && s->values.size() == n;
    };
    const bool has_background = full(pattern.background, x.size());
    const bool has_calculated = full(pattern.calculated, x.size());
    // Partial native scratch buffers require a separate bounded editor contract.
    // An enabled supplied layer must not disappear because its span is unsupported.
    if ((visible(PlotLayer::background) || visible(PlotLayer::calculated) ||
         visible(PlotLayer::difference)) &&
        pattern.background && !has_background)
        return Result<PlotUpdate>{invalid("partial_background_requires_separate_contract")};
    if ((visible(PlotLayer::calculated) || visible(PlotLayer::difference)) && pattern.calculated &&
        !has_calculated)
        return Result<PlotUpdate>{invalid("partial_calculated_requires_separate_contract")};
    const bool draw_calc = visible(PlotLayer::calculated) && has_background && has_calculated;
    const bool draw_diff = visible(PlotLayer::difference) && has_background && has_calculated;
    auto add_span = [&](SeriesKind kind, std::vector<double> y) {
        return append(out,
                      series(kind, style(kind), span(x, first, count, Precision::binary32_transport),
                             std::move(y), wave, Precision::binary32_transport),
                      request.abscissa);
    };
    std::vector<double> calc, diff;
    if (draw_calc || draw_diff) {
        if (!count)
            return Result<PlotUpdate>{invalid("empty_calculation_span")};
        for (std::size_t i = first; i < first + count; ++i) {
            const float c = float(pattern.calculated->values[i]), b = float(pattern.background->values[i]);
            const float cb = c + b;
            if (draw_calc)
                calc.push_back(cb);
            if (draw_diff)
                diff.push_back(draw_calc ? float(float(pattern.observed[i]) - cb)
                                         : float(float(float(pattern.observed[i]) - c) - b));
        }
    }
    if (draw_calc) {
        auto r = add_span(SeriesKind::calculated, std::move(calc));
        if (!r)
            return Result<PlotUpdate>{r.error()};
    }
    if (visible(PlotLayer::background) && has_background && p.background_enabled &&
        !pattern.processing.background_subtracted.value_or(false)) {
        auto r = add_span(SeriesKind::background,
                          span(pattern.background->values, first, count, Precision::binary32_transport));
        if (!r)
            return Result<PlotUpdate>{r.error()};
    }
    if (visible(PlotLayer::background_points) && has_background && p.background_enabled &&
        !pattern.processing.background_subtracted.value_or(false) && !p.background_points_x.empty()) {
            auto s = series(SeriesKind::background_points, "background_points", p.background_points_x,
                            p.background_points_y, wave, Precision::binary32_transport);
            for (auto &v : s.x)
                v = float(v);
            for (auto &v : s.y)
                v = float(v);
            auto r = append(out, std::move(s), request.abscissa);
            if (!r)
                return Result<PlotUpdate>{r.error()};
        }
    if (draw_diff) {
        auto r = add_span(SeriesKind::difference, std::move(diff));
        if (!r)
            return Result<PlotUpdate>{r.error()};
    }
    if (visible(PlotLayer::smoothing) && p.smoothing_preview) {
        if (p.smoothing_preview->size() != count)
            return Result<PlotUpdate>{invalid("smoothing_span_mismatch")};
        auto r = add_span(SeriesKind::smoothing,
                          span(*p.smoothing_preview, 0, count, Precision::binary32_transport));
        if (!r)
            return Result<PlotUpdate>{r.error()};
    }
    if (visible(PlotLayer::peaks) && !in.peaks.x.empty()) {
        if (!finite(in.peaks.x, Precision::binary32_transport))
            return Result<PlotUpdate>{invalid("invalid_peak_coordinates")};
        std::vector<double> px, py, ux, uy;
        for (std::size_t i = 0; i < in.peaks.x.size(); ++i) {
            const float value = float(in.peaks.x[i]), height = float(pattern.observed[nearest(x, value)]);
            px.push_back(value);
            py.push_back(height);
            if (visible(PlotLayer::unindexed_peaks) && i < in.peaks.unindexed.size() &&
                in.peaks.unindexed[i] > 0) {
                ux.push_back(value);
                uy.push_back(height);
            }
        }
        auto r = append(out,
                        series(SeriesKind::peaks, "native_sampled_peaks", std::move(px), std::move(py), wave,
                               Precision::binary32_transport),
                        request.abscissa);
        if (!r)
            return Result<PlotUpdate>{r.error()};
        if (!ux.empty()) {
            r = append(out,
                       series(SeriesKind::unindexed_peaks, "unindexed_peaks", std::move(ux), std::move(uy),
                              wave, Precision::binary32_transport),
                       request.abscissa);
            if (!r)
                return Result<PlotUpdate>{r.error()};
        }
    }
    for (const auto &card : in.cards) {
        auto s = project_card_overlay(card, request.abscissa);
        if (!s)
            return Result<PlotUpdate>{s.error()};
        out.snapshot.overlays.push_back(std::move(s).value());
    }
    out.commands.push_back({CommandKind::redraw, 1, {}});
    return Result<PlotUpdate>{std::move(out)};
}

Result<PlotUpdate> project_session_update(const PlotInput &raw, PlotRequest request) {
    auto projected = display_input(raw);
    if (!projected) return Result<PlotUpdate>{projected.error()};
    const auto &in = projected.value();
    PlotUpdate out;
    out.snapshot.abscissa = request.abscissa;
    out.snapshot.reverse_axis = request.abscissa == Abscissa::d_spacing;
    if (in.mode == PresentationMode::headless)
        return Result<PlotUpdate>{std::move(out)};
    auto peaks = series(SeriesKind::peaks, "search.residual_peaks", in.peaks.x, in.peaks.current_intensity,
                        in.peaks.wavelength, Precision::binary64);
    peaks.style_identity = "peaks";
    if (peaks.x.empty())
        peaks.update = UpdatePolicy::erase;
    auto status = append(out, std::move(peaks), request.abscissa);
    if (!status)
        return Result<PlotUpdate>{status.error()};
    if (in.patterns.empty() || in.patterns.front().pattern.two_theta.empty())
        return Result<PlotUpdate>{std::move(out)};
    const auto &p = in.patterns.front();
    status = valid_pattern(p, Precision::binary64);
    if (!status)
        return Result<PlotUpdate>{status.error()};
    const auto x = coordinates(p.pattern);
    const auto wave = p.pattern.radiation.lines.front().wavelength.value;
    auto observed =
        series(SeriesKind::observed, "observed:0", x, p.pattern.observed, wave, Precision::binary64);
    observed.label = p.display_name;
    status = append(out, std::move(observed), request.abscissa);
    if (!status)
        return Result<PlotUpdate>{status.error()};
    std::vector<double> bx, by;
    if (p.pattern.background && p.pattern.background->window) {
        const auto first = p.pattern.background->window->first;
        by = p.pattern.background->values;
        if (first > x.size() || by.size() > x.size() - first)
            return Result<PlotUpdate>{invalid("background_window_mismatch")};
        bx = span(x, first, by.size(), Precision::binary64);
    }
    auto background =
        series(SeriesKind::background, "background", std::move(bx), std::move(by), wave, Precision::binary64);
    if (background.x.empty() || in.presentation.layers[std::size_t(PlotLayer::background)] != Visibility::visible)
        background.update = UpdatePolicy::erase;
    status = append(out, std::move(background), request.abscissa);
    if (!status)
        return Result<PlotUpdate>{status.error()};
    auto points = series(SeriesKind::background_points, "background_points", p.background_points_x,
                         p.background_points_y, wave, Precision::binary64);
    if (points.x.empty() || in.presentation.layers[std::size_t(PlotLayer::background_points)] != Visibility::visible)
        points.update = UpdatePolicy::erase;
    status = append(out, std::move(points), request.abscissa);
    if (!status)
        return Result<PlotUpdate>{status.error()};
    for (const auto &card : in.cards) {
        auto s = project_card_overlay(card, request.abscissa);
        if (!s)
            return Result<PlotUpdate>{s.error()};
        out.snapshot.overlays.push_back(std::move(s).value());
    }
    return Result<PlotUpdate>{std::move(out)};
}

Result<PlotUpdate> project_pfsm_update(const PfsmPlot &plot, const PresentationWorkspace::Snapshot &view, PlotRequest request) {
    const auto &p = plot.pattern;
    if (!can_subtract_background(p) || !p.calculated || !p.calculated->window ||
        p.calculated->window->first != 0 || p.calculated->values.size() != p.observed.size() ||
        plot.residual.size() != p.observed.size() || p.radiation.lines.empty())
        return Result<PlotUpdate>{invalid("invalid_pfsm_plot")};
    PlotUpdate out; out.snapshot.abscissa = request.abscissa;
    out.snapshot.reverse_axis = request.abscissa == Abscissa::d_spacing;
    out.commands = {{CommandKind::rescale, request.rescale, {}}, {CommandKind::replace_canvas, 1, {}}};
    auto observed = p.observed; auto calculated = p.calculated->values;
    if (view.subtract_background)
        for (std::size_t i = 0; i < observed.size(); ++i) {
            observed[i] -= p.background->values[i]; calculated[i] -= p.background->values[i];
        }
    const auto x = coordinates(p); const auto wave = p.radiation.lines.front().wavelength.value;
    for (auto [kind, id, values] : std::vector<std::tuple<SeriesKind, std::string, std::vector<double>>>{
            {SeriesKind::observed, "observed", std::move(observed)},
            {SeriesKind::calculated, "calculated", std::move(calculated)},
            {SeriesKind::difference, "difference", plot.residual}}) {
        auto item = series(kind, id, x, std::move(values), wave, Precision::binary64);
        item.label = kind == SeriesKind::observed ? "Observed" : kind == SeriesKind::difference ? "Residual" :
            plot.candidate_id.empty() ? "Calculated · selected phases" : "Calculated · joint trial " + plot.candidate_id;
        auto status = append(out, std::move(item), request.abscissa);
        if (!status) return Result<PlotUpdate>{status.error()};
    }
    if (!view.subtract_background && !plot.background_points.empty()) {
        std::vector<double> bx, by;
        for (auto point : plot.background_points) { bx.push_back(point.x); by.push_back(point.y); }
        auto status = append(out, series(SeriesKind::background_points, "background_points", std::move(bx), std::move(by), wave, Precision::binary64), request.abscissa);
        if (!status) return Result<PlotUpdate>{status.error()};
    }
    out.commands.push_back({CommandKind::redraw, 1, {}});
    return Result<PlotUpdate>{std::move(out)};
}

Result<PlotSeries> project_card_overlay(const CardOverlayInput &card, Abscissa axis) {
    if (axis < Abscissa::two_theta || axis > Abscissa::inverse_d_squared)
        return Result<PlotSeries>{invalid("invalid_abscissa")};
    auto s = series(SeriesKind::accepted_card, card.id, card.two_theta, {}, card.display_wavelength,
                    Precision::binary64);
    s.style_identity = "accepted_card:" + card.id;
    s.visibility = card.visibility;
    s.label = card.chemical_name;
    if (!card.mineral_name.empty())
        s.label += " [" + card.mineral_name + "]";
    if (s.label.empty())
        s.label = "Reflections";
    for (std::size_t i = 0; i < s.x.size(); ++i) {
        if (axis != Abscissa::two_theta && i < card.d_spacing.size())
            s.x[i] = card.d_spacing[i];
        if (axis == Abscissa::inverse_d)
            s.x[i] = 1 / s.x[i];
        else if (axis == Abscissa::inverse_d_squared)
            s.x[i] = 1 / (s.x[i] * s.x[i]);
    }
    if (!finite(s.x, Precision::binary64))
        return Result<PlotSeries>{invalid("invalid_card_coordinates")};
    std::sort(s.x.begin(), s.x.end());
    return Result<PlotSeries>{std::move(s)};
}
Result<PlotSeries> project_preview(PlotSeries s, Abscissa axis, PreviewGeometry geometry) {
    if (s.x.size() != s.y.size())
        return Result<PlotSeries>{invalid("invalid_preview_dimensions")};
    s.kind = SeriesKind::preprocessing_preview;
    s.style_identity = "preprocessing_preview";
    s.identity = "preprocessing.preview";
    s.label = "Preprocessing preview";
    s.preview_geometry = geometry;
    auto status = project_coordinates(s, axis);
    if (!status)
        return Result<PlotSeries>{status.error()};
    return Result<PlotSeries>{std::move(s)};
}
} // namespace qualpf::application::presentation
