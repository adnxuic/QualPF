#include "qualpf/preprocessing/legacy.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace qualpf::preprocessing {
namespace {
std::string fixed(float value, int precision) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}
std::string fortran_width(float value, int width, int precision) {
    auto text = fixed(value, precision);
    if (int(text.size()) > width)
        return std::string(width, '*');
    return std::string(width - text.size(), ' ') + text;
}
} // namespace
Result<std::string> export_peak_text(const PeakSet &peaks, PeakTextFormat format) {
    if (int(format) < 0 || int(format) > 2)
        return Result<std::string>{Error{ErrorCode::invalid_input, "peaks.export.format", "Unknown format"}};
    if (peaks.peaks.empty())
        return Result<std::string>{std::string{}};
    for (const auto &p : peaks.peaks) {
        if (!std::isfinite(p.two_theta.value) || !p.d_spacing || !std::isfinite(p.d_spacing->value) ||
            !std::isfinite(p.intensity) || !p.fwhm || !std::isfinite(p.fwhm->value) ||
            std::abs(p.intensity) > std::numeric_limits<float>::max() ||
            std::abs(p.fwhm->value) > std::numeric_limits<float>::max())
            return Result<std::string>{
                Error{ErrorCode::invalid_input, "peaks.export", "Incomplete/nonfinite peak"}};
    }
    std::ostringstream out;
    out.imbue(std::locale::classic());
    if (format != PeakTextFormat::details) {
        out << (format == PeakTextFormat::two_theta ? "#2theta values\n" : "#d values\n");
        for (const auto &p : peaks.peaks) {
            auto text =
                fixed(float(format == PeakTextFormat::two_theta ? p.two_theta.value : p.d_spacing->value), 6);
            if (text.starts_with("0."))
                text.erase(0, 1);
            else if (text.starts_with("-0."))
                text.erase(1, 1);
            out << text << '\n';
        }
    } else {
        float maximum = -std::numeric_limits<float>::max();
        for (const auto &p : peaks.peaks)
            maximum = std::max(maximum, float(p.intensity));
        if (maximum == 0)
            return Result<std::string>{Error{ErrorCode::numerical_failure, "peaks.export.intensity",
                                             "Legacy zero maximum normalization is undefined"}};
        const std::string separator = "  " + std::string(63, '-') + "\n";
        out << separator << "                          Peak positions\n"
            << separator << "             2theta           d            FWHM     100.*I/Imax\n"
            << separator;
        float scale = 100.f / maximum;
        for (std::size_t i = 0; i < peaks.peaks.size(); ++i) {
            const auto &p = peaks.peaks[i];
            if (i + 1 > 99999)
                out << "*****";
            else
                out << std::setw(5) << i + 1;
            out << ")    " << fortran_width(float(p.two_theta.value), 10, 4) << "  "
                << fortran_width(float(p.d_spacing->value), 12, 6) << "    "
                << fortran_width(float(p.fwhm->value), 10, 4) << "    "
                << fortran_width(scale * float(p.intensity), 8, 2) << '\n';
        }
    }
    return Result<std::string>{out.str()};
}
} // namespace qualpf::preprocessing
