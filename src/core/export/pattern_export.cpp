#include "qualpf/export/pattern_export.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <string_view>

namespace qualpf::exporting {
namespace {
Error error(std::string field, std::string reason) {
    return {ErrorCode::invalid_input, std::move(field), "Invalid export snapshot", std::move(reason)};
}
bool finite(const std::vector<double> &values) {
    return std::all_of(values.begin(), values.end(), [](double v) { return std::isfinite(v); });
}
std::string number(double value, int width, int precision, bool scientific = false) {
    // std::to_chars is explicit, locale independent, nearest/even formatting.
    std::array<char, 512> buffer{};
    const auto r = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                 scientific ? std::chars_format::scientific : std::chars_format::fixed, precision);
    if (r.ec != std::errc{})
        return std::string(width, '*');
    std::string text(buffer.data(), r.ptr);
    if (scientific) {
        auto at = text.find('e');
        if (at != std::string::npos)
            text[at] = 'E';
    }
    if (text.size() > static_cast<std::size_t>(width))
        return std::string(width, '*');
    return std::string(width - text.size(), ' ') + text;
}
bool valid_token(const std::string &token) {
    constexpr std::array<std::string_view, 8> tokens{
        "?", "neutron", "electron", "'Cu K\\a~1~'", "'Mo K\\a~1~'", "'Cr K\\a~1~'", "'Co K\\a~1~'", "'Fe K\\a~1~'"};
    return std::find(tokens.begin(), tokens.end(), token) != tokens.end();
}
} // namespace
Status validate_export_snapshot(const PatternExportSnapshot &s, PatternFormat format) {
    if (format < PatternFormat::xy_observed || format > PatternFormat::powder_cif)
        return Status{error("export.format", "unsupported_format")};
    if (s.coordinate != Coordinate::two_theta_degrees)
        return Status{error("export.coordinate", "unsupported_abscissa")};
    if (s.x.empty())
        return Status{error("export.pattern", "empty_input")};
    if (s.x.size() != s.observed.size())
        return Status{error("export.pattern", "invalid_dimensions")};
    if (!finite(s.x) || !finite(s.observed))
        return Status{error("export.pattern", "nonfinite_value")};
    const int coordinate_width = format == PatternFormat::powder_cif ? 15 : 16;
    if (std::any_of(s.x.begin(), s.x.end(), [coordinate_width](double value) {
            return number(value, coordinate_width, 6).find('*') != std::string::npos;
        }))
        return Status{error("export.coordinate", "unrepresentable_coordinate")};
    if (format != PatternFormat::powder_cif)
        return Status{std::monostate{}};
    if (!s.wavelength_angstrom || !std::isfinite(*s.wavelength_angstrom) || *s.wavelength_angstrom <= 0)
        return Status{error("export.wavelength", "invalid_wavelength")};
    if (!valid_token(s.radiation_token))
        return Status{error("export.radiation", "invalid_metadata")};
    if (!std::isfinite(s.range_min) || !std::isfinite(s.range_max) || s.range_min > s.range_max)
        return Status{error("export.range", "invalid_range")};
    if (s.d_spacing_angstrom.size() != s.x.size() || !finite(s.d_spacing_angstrom) ||
        std::any_of(s.d_spacing_angstrom.begin(), s.d_spacing_angstrom.end(), [](double v) { return v <= 0; }))
        return Status{error("export.d_spacing", "invalid_coordinate")};
    if (s.powder_cif_schema != PowderCifSchema::observed &&
        s.powder_cif_schema != PowderCifSchema::observed_net_background)
        return Status{error("export.background", "invalid_resolved_schema")};
    const bool with_background = s.powder_cif_schema == PowderCifSchema::observed_net_background;
    if (with_background != s.background.has_value() || with_background != s.net.has_value())
        return Status{error("export.background", "schema_payload_mismatch")};
    if (with_background && (s.background->size() != s.x.size() || s.net->size() != s.x.size() ||
                            !finite(*s.background) || !finite(*s.net)))
        return Status{error("export.background", "invalid_dimensions_or_values")};
    return Status{std::monostate{}};
}
Result<std::string> format_pattern(const PatternExportSnapshot &s, PatternFormat format) {
    auto checked = validate_export_snapshot(s, format);
    if (!checked)
        return Result<std::string>{checked.error()};
    std::string out;
    if (format != PatternFormat::powder_cif) {
        out = format == PatternFormat::xy_background ? "#      2theta    yback\r\n" : "#      2theta    yoss\r\n";
        for (std::size_t i = 0; i < s.x.size(); ++i)
            out += number(s.x[i], 16, 6) + " " + number(s.observed[i], 16, 9, true) + "\r\n";
        return Result<std::string>{std::move(out)};
    }
    out = "# POWDER PROFILE\r\n\r\n";
    out += "_pd_meas_2theta_range_min " + number(s.range_min, 10, 3) + "\r\n";
    out += "_pd_meas_2theta_range_max " + number(s.range_max, 10, 3) + "\r\n";
    std::array<char, 32> count{};
    const auto count_end = std::to_chars(count.data(), count.data() + count.size(), s.x.size()).ptr;
    const std::string count_text(count.data(), count_end);
    out += "_pd_proc_number_of_points " +
           (count_text.size() > 10 ? std::string(10, '*') : std::string(10 - count_text.size(), ' ') + count_text) +
           "\r\n\r\n";
    out += "_diffrn_radiation_wavelength   " + number(*s.wavelength_angstrom, 10, 6) + "\r\n";
    out += "_diffrn_radiation_type          " + s.radiation_token + "\r\n\r\n";
    out += "loop_\r\n     _pd_meas_2theta_scan\r\n     _pd_proc_d_spacing\r\n     _pd_meas_counts_total\r\n";
    const bool with_background = s.powder_cif_schema == PowderCifSchema::observed_net_background;
    if (with_background)
        out += "     _pd_proc_intensity_intensity_net\r\n     _pd_proc_intensity_bkg_calc\r\n";
    for (std::size_t i = 0; i < s.x.size(); ++i) {
        out += " " + number(s.x[i], 15, 6) + " " + number(s.d_spacing_angstrom[i], 15, 6) + " " +
               number(s.observed[i], 15, 6);
        if (with_background)
            out += " " + number((*s.net)[i], 15, 6) + " " + number((*s.background)[i], 15, 6);
        out += "\r\n";
    }
    return Result<std::string>{std::move(out)};
}
} // namespace qualpf::exporting
