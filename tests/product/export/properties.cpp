#include "qualpf/application/export/pattern_export_input.h"
#include "qualpf/domain/radiation.h"
#include "qualpf/export/binary32_coordinates.h"
#include <clocale>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
qualpf::Pattern pattern() {
    qualpf::Pattern p;
    p.two_theta = {qualpf::TwoThetaDegrees{10.0625}, qualpf::TwoThetaDegrees{20.1875}};
    p.observed = {-0.0, 1.0000001192092896};
    p.radiation.lines = {{qualpf::radiation::default_wavelength, 1}};
    return p;
}
} // namespace
int main() {
    using namespace qualpf;
    using namespace qualpf::exporting;
    using namespace qualpf::application::exporting;
    try {
        auto p = pattern();
        auto prepared = prepare_pattern_export(p, {}, PatternFormat::xy_observed);
        check(bool(prepared), "Ordinary owning XY input");
        auto copy = prepared.value();
        p.observed[0] = 99;
        check(std::signbit(copy.observed[0]), "Owning snapshot preserves signed zero");
        auto bytes = format_pattern(copy, PatternFormat::xy_observed);
        check(bool(bytes) && bytes.value() == "#      2theta    yoss\r\n       10.062500 -0.000000000E+00\r\n       "
                                              "20.187500  1.000000119E+00\r\n",
              "Frozen decimal boundary");
        for (const auto *locale : {"C", "German_Germany.1252", "de_DE.UTF-8", "Chinese_China.936"})
            if (std::setlocale(LC_NUMERIC, locale))
                check(format_pattern(copy, PatternFormat::xy_observed).value() == bytes.value(),
                      "Locale-independent text");
        std::setlocale(LC_NUMERIC, "C");
        p = pattern();
        p.background = SampledSignal{{2, 3}, SampleWindow{0}};
        for (const auto count : {0u, 2u})
            for (const auto method : {BackgroundExportMethod::none, BackgroundExportMethod::enabled}) {
                ExportMetadata metadata;
                metadata.background_control_point_count = count;
                metadata.background_method = method;
                auto result = prepare_pattern_export(p, metadata, PatternFormat::powder_cif);
                check(bool(result), "All four upstream schema combinations prepare");
                const bool enabled = count > 0 && method == BackgroundExportMethod::enabled;
                check(result.value().powder_cif_schema ==
                          (enabled ? PowderCifSchema::observed_net_background : PowderCifSchema::observed),
                      "Resolved schema truth table");
                check(result.value().background.has_value() == enabled && result.value().net.has_value() == enabled,
                      "Arrays are payload, not the schema predicate");
                auto text = format_pattern(result.value(), PatternFormat::powder_cif);
                check(bool(text) &&
                          (text.value().find("_pd_proc_intensity_intensity_net") != std::string::npos) == enabled,
                      "Tags follow resolved schema");
                if (enabled) {
                    auto supplied = result.value();
                    (*supplied.net)[0] = 42;
                    supplied.d_spacing_angstrom[0] = 7;
                    auto formatted = format_pattern(supplied, PatternFormat::powder_cif);
                    check(bool(formatted) && formatted.value().find("      42.000000") != std::string::npos &&
                              formatted.value().find("       7.000000") != std::string::npos,
                          "Formatter consumes supplied d and net");
                    supplied.powder_cif_schema = PowderCifSchema::observed;
                    auto mismatch = format_pattern(supplied, PatternFormat::powder_cif);
                    check(!mismatch && mismatch.error().reason == "schema_payload_mismatch",
                          "Formatter never guesses schema from arrays");
                }
            }
        p = pattern();
        p.observed = {3e38, 3e38};
        p.background = SampledSignal{{-3e38, -3e38}, SampleWindow{0}};
        check(bool(prepare_pattern_export(p, {}, PatternFormat::powder_cif)),
              "Inactive background does not derive an unused overflowing net");
        p = pattern();
        ExportMetadata active;
        active.background_control_point_count = 2;
        auto absent = prepare_pattern_export(p, active, PatternFormat::powder_cif);
        check(!absent && absent.error().reason == "incomplete_background", "Active schema requires aligned payload");
        auto zero = resolve_export_binary32_d(TwoThetaDegrees{0}, radiation::default_wavelength);
        check(!zero && zero.error().reason == "nonfinite_d_spacing", "Proposed CIF zero rejection remains pending B2");
        auto tiny = resolve_export_binary32_d(TwoThetaDegrees{10}, WavelengthAngstrom{1e-300});
        check(!tiny && tiny.error().reason == "invalid_wavelength",
              "Proposed transported-wave rejection remains pending B2");
        for (const auto x : {-1.0, 181.0, 10.0}) {
            p = pattern();
            p.two_theta = {TwoThetaDegrees{x}, TwoThetaDegrees{std::nextafter(x, INFINITY)}};
            auto xy = prepare_pattern_export(p, {}, PatternFormat::xy_observed);
            check(bool(xy), "XY accepts negative/>180/collapsed transported positions");
            check(xy.value().x[0] == xy.value().x[1], "Collapse preserves both source rows");
            auto cif = prepare_pattern_export(p, {}, PatternFormat::powder_cif);
            check(x < 0 ? !cif && cif.error().reason == "nonpositive_d_spacing" : bool(cif),
                  "CIF derives supported sign from portable d");
        }
        p = pattern();
        p.two_theta[1] = p.two_theta[0];
        check(!prepare_pattern_export(p, {}, PatternFormat::xy_observed), "Canonical source ordering remains strict");
        p = pattern();
        p.radiation.lines.push_back({WavelengthAngstrom{1e-300}, 1});
        check(bool(prepare_pattern_export(p, {}, PatternFormat::powder_cif)),
              "Unused secondary wavelength transport cannot restrict this formatter");
        check(std::signbit(export_binary32_transport(-0.0)), "Integer transport preserves negative zero");
        std::cout << "PASS " << checks
                  << " ownership/schema/serialization properties; B2 policy unapproved; Qt=0 Fortran=0\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL " << checks << ' ' << error.what() << '\n';
        return 1;
    }
}
