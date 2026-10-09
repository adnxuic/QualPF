#include "qualpf/application/peak_coordinates.h"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <limits>
#include <string>

namespace qualpf::application {
namespace {
// Bounded source-compatible GETNUM arithmetic. Do not replace with strtod:
// the approved parser accumulates decimal terms in binary32.
std::optional<float> number(std::string token, bool &unsafe) {
    for (auto &c : token)
        if (c == 'e')
            c = 'E';
    if (token.empty())
        return {};
    const std::size_t first = token[0] == '-' ? 1 : 0;
    const auto e = token.find('E');
    const auto end = e == std::string::npos ? token.size() : e;
    const auto dot = token.find('.');
    if (first == end || (dot != std::string::npos && dot >= end))
        return {};
    int power = int(dot == std::string::npos ? end : dot) - 1 - int(first);
    float sum = 0;
    double integer = 0;
    bool digit = false;
    for (auto i = first; i < end; ++i) {
        const char c = token[i];
        if (c == '.' && i == dot)
            continue;
        if (c < '0' || c > '9')
            return {};
        digit = true;
        if (power >= 0) {
            integer = integer * 10 + (c - '0');
            if (integer > std::numeric_limits<int>::max()) {
                unsafe = true;
                return {};
            }
        }
        const float term = float(c - '0') * std::pow(10.f, float(power--));
        sum = sum + term;
        if (!std::isfinite(sum)) {
            unsafe = true;
            return {};
        }
    }
    if (!digit)
        return {};
    if (first)
        sum = -sum;
    if (e != std::string::npos) {
        auto i = e + 1;
        bool negative = false;
        if (i < token.size() && (token[i] == '+' || token[i] == '-'))
            negative = token[i++] == '-';
        if (i == token.size())
            return {};
        int exponent = 0;
        for (; i < token.size(); ++i) {
            if (token[i] < '0' || token[i] > '9')
                return {};
            if (exponent > 38) {
                unsafe = true;
                return {};
            }
            exponent = exponent * 10 + token[i] - '0';
        }
        if (exponent > 38) {
            unsafe = true;
            return {};
        }
        float scale = std::pow(10.f, exponent);
        if (negative)
            scale = 1.f / scale;
        sum = sum * scale;
    }
    if (!std::isfinite(sum)) {
        unsafe = true;
        return {};
    }
    return sum;
}
} // namespace
Result<std::vector<ParsedPeakCoordinateEntry>>
parsePeakCoordinates(std::string_view bytes, PeakCoordinateRepresentation representation) {
    const auto fail = [](std::size_t line, const char *reason = "invalid_record") {
        return Result<std::vector<ParsedPeakCoordinateEntry>>{
            Error{ErrorCode::invalid_input, "peaks.file",
                  "Invalid peak coordinate record at line " + std::to_string(line), reason}};
    };
    if (std::fegetround() != FE_TONEAREST)
        return fail(1, "unsupported_rounding_mode");
    std::vector<ParsedPeakCoordinateEntry> result;
    std::size_t line = 0;
    while (!bytes.empty()) {
        ++line;
        const auto newline = bytes.find('\n');
        auto record = bytes.substr(0, newline);
        bytes = newline == std::string_view::npos ? std::string_view{} : bytes.substr(newline + 1);
        // Fortran formatted (a) consumes the whole record but retains 250 chars.
        std::string text(record.substr(0, 250));
        for (auto &c : text) {
            if (static_cast<unsigned char>(c) > 127)
                return fail(line, "unsupported_encoding");
            if (static_cast<unsigned char>(c) < 32 || c == 127)
                c = ' ';
        }
        if (!text.empty() && (text[0] == '#' || text[0] == '!'))
            continue;
        std::size_t pos = 0, count = 0;
        float coordinate = 0;
        while (pos < text.size()) {
            pos = text.find_first_not_of(' ', pos);
            if (pos == std::string::npos)
                break;
            auto end = text.find(' ', pos);
            if (end == std::string::npos)
                end = text.size();
            bool unsafe = false;
            auto value = number(text.substr(pos, end - pos), unsafe);
            if (unsafe)
                return fail(line, "unrepresentable_numeric_token");
            if (!value)
                break; // GETNUM retains the preceding numeric prefix.
            if (++count > 40)
                return fail(line, "too_many_numeric_tokens");
            if (count == 1)
                coordinate = *value;
            pos = end;
        }
        if (!count)
            return fail(line);
        result.push_back({representation, double(coordinate),
                          representation == PeakCoordinateRepresentation::d_spacing
                              ? std::optional<double>{coordinate}
                              : std::nullopt,
                          line});
    }
    if (result.empty())
        return fail(line ? line : 1, "no_accepted_coordinates");
    return Result<std::vector<ParsedPeakCoordinateEntry>>{std::move(result)};
}
} // namespace qualpf::application
