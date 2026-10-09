#include "qualpf/application/export/pattern_export_input.h"
#include "qualpf/domain/radiation.h"
#include "qualpf/export/binary32_coordinates.h"
#include <array>
#include <cmath>

namespace qualpf::application::exporting {
namespace {
Error invalid(std::string field, std::string reason) {
    return {ErrorCode::invalid_input, std::move(field), "Cannot prepare pattern export", std::move(reason)};
}
bool transport(double value) {
    return std::isfinite(value) && std::isfinite(qualpf::exporting::export_binary32_transport(value));
}
std::string radiation_token(int code, float wavelength) {
    if (code == 1)
        return "neutron";
    if (code == 2)
        return "electron";
    // Source-bound stock recognition only. These are not new product defaults.
    constexpr std::array<float, 6> waves{
        static_cast<float>(radiation::CU_KA1_WAVE), .70931715f, .71073f, 2.2897260f, 1.7889960f, 1.9360410f};
    constexpr std::array<const char *, 6> tokens{"'Cu K\\a~1~'", "'Mo K\\a~1~'", "'Mo K\\a~1~'",
                                                 "'Cr K\\a~1~'", "'Co K\\a~1~'", "'Fe K\\a~1~'"};
    std::string result = "?";
    for (std::size_t i = 0; i < waves.size(); ++i)
        if (std::abs(static_cast<float>(wavelength - waves[i])) < .001f)
            result = tokens[i];
    return result;
}
} // namespace
Result<qualpf::exporting::PatternExportSnapshot> prepare_pattern_export(const Pattern &p, ExportMetadata metadata,
                                                                        qualpf::exporting::PatternFormat format) {
    using qualpf::exporting::PatternExportSnapshot;
    using qualpf::exporting::PatternFormat;
    auto fail = [](std::string field, std::string reason) {
        return Result<PatternExportSnapshot>{invalid(std::move(field), std::move(reason))};
    };
    if (format < PatternFormat::xy_observed || format > PatternFormat::powder_cif)
        return fail("export.format", "unsupported_format");
    if (p.two_theta.empty())
        return fail("export.pattern", "empty_input");
    if (p.two_theta.size() != p.observed.size())
        return fail("export.pattern", "invalid_dimensions");
    // Canonical source ordering precedes transport. Equal binary32 samples are
    // allowed; the original binary64 grid must still be strictly increasing.
    for (std::size_t i = 1; i < p.two_theta.size(); ++i)
        if (!(p.two_theta[i - 1].value < p.two_theta[i].value))
            return fail("export.coordinate", "invalid_source_grid");
    PatternExportSnapshot out;
    std::size_t first = 0;
    const std::vector<double> *y = &p.observed;
    if (format == PatternFormat::xy_background) {
        if (!p.background || !p.background->window)
            return fail("export.background", "unknown_window");
        first = p.background->window->first;
        y = &p.background->values;
    }
    if (y->empty() || first > p.two_theta.size() || y->size() > p.two_theta.size() - first)
        return fail("export.background", "invalid_window");
    for (std::size_t i = 0; i < y->size(); ++i) {
        const double x = p.two_theta[first + i].value;
        if (!transport(x) || !transport((*y)[i]))
            return fail("export.pattern", "nonfinite_or_unrepresentable_value");
        out.x.push_back(qualpf::exporting::export_binary32_transport(x));
        out.observed.push_back(qualpf::exporting::export_binary32_transport((*y)[i]));
    }
    if (format == PatternFormat::powder_cif) {
        if (metadata.editing)
            return fail("export.session", "private_editor_active");
        if (p.radiation.lines.empty())
            return fail("export.wavelength", "missing_wavelength");
        // Only the first line is consumed by this frozen export contract.
        // Canonical radiation validation remains upstream and unchanged.
        const auto first_wave = p.radiation.lines.front().wavelength.value;
        if (!transport(first_wave) || qualpf::exporting::export_binary32_transport(first_wave) <= 0)
            return fail("export.wavelength", "invalid_wavelength");
        if (metadata.radiation_code < 0 || metadata.radiation_code > 2)
            return fail("export.radiation", "invalid_metadata");
        out.wavelength_angstrom =
            qualpf::exporting::export_binary32_transport(p.radiation.lines.front().wavelength.value);
        out.radiation_token = radiation_token(metadata.radiation_code, static_cast<float>(*out.wavelength_angstrom));
        const auto range = p.calculation_range.value_or(AngularRange{p.two_theta.front(), p.two_theta.back()});
        if (!transport(range.minimum.value) || !transport(range.maximum.value))
            return fail("export.range", "invalid_range");
        out.range_min = qualpf::exporting::export_binary32_transport(range.minimum.value);
        out.range_max = qualpf::exporting::export_binary32_transport(range.maximum.value);
        // Export-specific arithmetic contract; existing legacy_d consumers keep
        // their unchanged implementation and numerical authority.
        for (const auto x : out.x) {
            const auto d = qualpf::exporting::resolve_export_binary32_d(TwoThetaDegrees{x},
                                                                        WavelengthAngstrom{*out.wavelength_angstrom});
            if (!d)
                return Result<PatternExportSnapshot>{d.error()};
            out.d_spacing_angstrom.push_back(d.value().d_angstrom);
        }
        // Match the real Application preflight even if the background is not
        // selected by the native has_back-equivalent schema predicate.
        if (p.background) {
            if (!p.background->window || p.background->window->first != 0 ||
                p.background->values.size() != out.x.size())
                return fail("export.background", "incomplete_background");
            for (const auto value : p.background->values)
                if (!transport(value))
                    return fail("export.background", "nonfinite_or_unrepresentable_value");
        }
        if (metadata.background_method != BackgroundExportMethod::none &&
            metadata.background_method != BackgroundExportMethod::enabled)
            return fail("export.background", "invalid_metadata");
        const bool has_back = metadata.background_control_point_count > 0 &&
                              metadata.background_method == BackgroundExportMethod::enabled;
        if (has_back) {
            if (!p.background)
                return fail("export.background", "incomplete_background");
            out.powder_cif_schema = qualpf::exporting::PowderCifSchema::observed_net_background;
            out.background.emplace();
            out.net.emplace();
            for (std::size_t i = 0; i < out.x.size(); ++i) {
                if (!transport(p.background->values[i]))
                    return fail("export.background", "nonfinite_or_unrepresentable_value");
                const float background =
                    static_cast<float>(qualpf::exporting::export_binary32_transport(p.background->values[i]));
                const float net = static_cast<float>(out.observed[i]) - background;
                if (!std::isfinite(net))
                    return fail("export.net", "nonfinite_value");
                out.background->push_back(background);
                out.net->push_back(net);
            }
        }
    }
    // Validate all resolved columns before exposing an owning DTO.
    auto checked = qualpf::exporting::validate_export_snapshot(out, format);
    if (!checked)
        return Result<PatternExportSnapshot>{checked.error()};
    return Result<PatternExportSnapshot>{std::move(out)};
}
} // namespace qualpf::application::exporting
