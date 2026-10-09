#pragma once
#include "qualpf/domain/result.h"
#include <optional>
#include <string>
#include <vector>

namespace qualpf::exporting {
enum class PatternFormat { xy_observed, xy_background, powder_cif };
enum class Coordinate { two_theta_degrees, d_angstrom, inverse_d, inverse_d_squared };
enum class PowderCifSchema { observed, observed_net_background };

// Fully owning, resolved export values. No session, native descriptor, widget or
// file target. Snapshot preparation owns numerical/metadata interpretation.
struct PatternExportSnapshot {
    Coordinate coordinate = Coordinate::two_theta_degrees;
    PowderCifSchema powder_cif_schema = PowderCifSchema::observed;
    std::vector<double> x;
    std::vector<double> observed;
    std::vector<double> d_spacing_angstrom;
    std::optional<std::vector<double>> background;
    std::optional<std::vector<double>> net;
    double range_min = 0, range_max = 0;
    std::optional<double> wavelength_angstrom;
    std::string radiation_token;
};

// Native Windows compatibility contract: ASCII bytes, CRLF, final CRLF.
// Formatting never narrows numbers or derives columns.
Result<std::string> format_pattern(const PatternExportSnapshot &, PatternFormat);
Status validate_export_snapshot(const PatternExportSnapshot &, PatternFormat);
} // namespace qualpf::exporting
