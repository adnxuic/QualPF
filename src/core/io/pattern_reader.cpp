#include "qualpf/io/pattern_reader.h"
#include "qualpf/domain/radiation.h"
#include "qualpf/domain/validation.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string_view>

namespace qualpf::io {
namespace {
using Lines = std::vector<std::string>;
struct Columns {
    std::vector<float> values;
    bool complete = true;
};
std::string trim(std::string s) {
    auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::string upper(std::string s) {
    for (auto &c : s)
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
    return s;
}
// A narrow compatibility number decoder, not strtof: decimal digits were
// accumulated in binary32 in Getnum. Keep those roundings for measured spectra.
std::optional<float> number(std::string s) {
    s = upper(s);
    if (s.empty() || s.front() == '+')
        return {};
    const std::size_t first = s.front() == '-' ? 1 : 0;
    const auto e = s.find('E');
    const auto end = e == std::string::npos ? s.size() : e;
    auto dot = s.find('.');
    if (dot == std::string::npos)
        dot = end;
    if (first == end || dot > end)
        return {};
    int power = int(dot) - int(first) - 1;
    float value = 0;
    bool digit = false;
    for (auto i = first; i < end; ++i) {
        if (s[i] == '.' && i == dot)
            continue;
        if (s[i] < '0' || s[i] > '9')
            return {};
        digit = true;
        const float contribution = float(s[i] - '0') * std::pow(10.0f, float(power--));
        value = value + contribution;
    }
    if (!digit)
        return {};
    if (first)
        value = -value;
    if (e != std::string::npos) {
        auto i = e + 1;
        bool negative = i < s.size() && s[i] == '-';
        if (i < s.size() && (s[i] == '-' || s[i] == '+'))
            ++i;
        if (i == s.size())
            return {};
        int exponent = 0;
        for (; i < s.size(); ++i) {
            if (s[i] < '0' || s[i] > '9' || exponent > 1000)
                return {};
            exponent = exponent * 10 + s[i] - '0';
        }
        // Integer exponentiation has different rounding from powf(float).
        float factor = 1, base = 10;
        while (exponent) {
            if (exponent & 1)
                factor *= base;
            exponent >>= 1;
            if (exponent)
                base *= base;
        }
        if (negative)
            factor = 1.0f / factor;
        value *= factor;
    }
    return value;
}
Columns columns(std::string line, std::size_t limit = 40) {
    for (auto &c : line)
        if (static_cast<unsigned char>(c) < 32 || static_cast<unsigned char>(c) >= 127)
            c = ' ';
    std::istringstream input(line);
    Columns out;
    std::string token;
    while (out.values.size() < limit && input >> token) {
        auto v = number(token);
        if (!v) {
            out.complete = false;
            break;
        }
        out.values.push_back(*v);
    }
    return out;
}
// Vendor scalar fields use the separate S_TO_R mantissa/divisor convention.
// UXD's get_next_number first truncates at the first non-decimal character.
std::optional<float> scalar(std::string text, bool decimal_prefix = false) {
    text = upper(trim(text));
    if (text.empty() || (decimal_prefix && text.front() == '+'))
        return {};
    std::size_t i = 0;
    const float sign = text.front() == '-' ? -1.0f : 1.0f;
    if (text.front() == '-' || text.front() == '+')
        ++i;
    while (i < text.size() && text[i] == ' ')
        ++i;
    auto mantissa = [&](float &top, float &bottom) {
        bool dot = false, digits = false;
        while (i < text.size()) {
            auto c = text[i];
            if (c == '.' && !dot) {
                dot = true;
                ++i;
                continue;
            }
            if (c < '0' || c > '9')
                break;
            const float shifted = 10.0f * top;
            top = shifted + float(c - '0');
            if (dot)
                bottom *= 10.0f;
            digits = true;
            ++i;
        }
        return digits;
    };
    float top = 0, bottom = 1;
    if (!mantissa(top, bottom))
        return {};
    float factor = 1;
    if (!decimal_prefix && i < text.size() && (text[i] == 'E' || text[i] == 'D')) {
        ++i;
        float esign = 1;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
            if (text[i] == '-')
                esign = -1;
            ++i;
        }
        float etop = 0, ebottom = 1;
        if (!mantissa(etop, ebottom) || etop > 1000)
            return {};
        if (ebottom == 1) {
            auto exponent = int(etop);
            float base = 10;
            while (exponent) {
                if (exponent & 1)
                    factor *= base;
                exponent >>= 1;
                if (exponent)
                    base *= base;
            }
            if (esign < 0)
                factor = 1.0f / factor;
        } else
            factor = std::pow(10.0f, esign * etop / ebottom);
    }
    const float signed_factor = sign * factor;
    const float scaled = signed_factor * top;
    return scaled / bottom;
}
struct Data {
    std::vector<float> x, y;
    std::vector<RadiationLine> waves;
    float step = 0;
};
bool comment(const std::string &s) { return !s.empty() && (s[0] == '#' || s[0] == '!' || s[0] == '>'); }
bool xy(const Lines &lines, Data &out) {
    bool initial = true;
    for (auto line : lines) {
        if (initial && (trim(line).empty() || comment(trim(line))))
            continue;
        initial = false;
        line = line.substr(0, 200);
        if (trim(line).empty() || comment(line))
            continue;
        auto c = columns(line).values;
        if (out.x.empty() && c.size() == 1) {
            out.waves = {{WavelengthAngstrom{c[0]}, 1}};
            continue;
        }
        if (c.size() < 2)
            return false;
        if (!out.x.empty() && c[0] < out.x.back())
            return false;
        out.x.push_back(c[0]);
        out.y.push_back(c[1]);
    }
    if (out.x.empty())
        return false;
    float sum = 0;
    for (std::size_t i = 1; i < out.x.size(); ++i)
        sum += out.x[i] - out.x[i - 1];
    out.step = out.x.size() > 1 ? sum / float(out.x.size() - 1) : 0;
    return true;
}
std::vector<float> block(const Lines &lines, std::size_t first, bool commas = false) {
    std::vector<float> values;
    for (auto i = first; i < lines.size(); ++i) {
        auto line = trim(lines[i]);
        if (commas)
            std::erase(line, ','); // Legacy count blocks remove separators.
        if (line.empty())
            continue;
        auto c = columns(line, line.size());
        if (!c.complete || c.values.empty())
            break;
        values.insert(values.end(), c.values.begin(), c.values.end());
    }
    return values;
}
bool grid(Data &out, float start, float step) {
    out.step = step;
    if (out.y.empty() || !std::isfinite(start) || !std::isfinite(step) || step <= 0)
        return false;
    for (std::size_t i = 0; i < out.y.size(); ++i) {
        const float offset = float(i) * step;
        out.x.push_back(start + offset);
    }
    return true;
}
bool counts(const Lines &lines, Data &out, const std::string &ext) {
    std::size_t first = 0;
    if (ext == ".XDA")
        first = 2;
    else if (ext == ".XDD") {
        first = 1;
        if (!lines.empty() && lines[0].starts_with("/*")) {
            while (first < lines.size() && !trim(lines[first - 1]).ends_with("*/"))
                ++first;
        }
    } else {
        while (first < lines.size()) {
            auto s = trim(lines[first]);
            if (!s.empty() && s[0] >= '0' && s[0] <= '9')
                break;
            ++first;
        }
    }
    if (first >= lines.size())
        return false;
    auto h = columns(lines[first]).values;
    if (h.size() < 3)
        return false;
    const float step = h[ext == ".XDA" ? 2 : 1];
    out.y = block(lines, first + 1);
    if (ext != ".XDA" && ext != ".XDD") {
        const float count = std::floor((h[2] - h[0]) / step + 1.0f);
        if (!std::isfinite(count) || count <= 1)
            return false;
        if (count < float(out.y.size()))
            out.y.resize(std::size_t(count));
    }
    return grid(out, h[0], step);
}
bool vendor(const Lines &lines, Data &out, const std::string &ext) {
    float start = std::numeric_limits<float>::quiet_NaN(), step = start, end = start;
    bool xml_data = false, xml_two_theta = false;
    if (ext == ".CPI") {
        if (lines.size() < 7)
            return false;
        auto a = scalar(lines[1]), b = scalar(lines[3]), w = scalar(lines[5]);
        if (!a || !b || !w)
            return false;
        start = *a;
        step = *b;
        out.waves = {{WavelengthAngstrom{*w}, 1}};
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        auto line = trim(lines[i]);
        auto key = upper(line);
        auto pos = line.find('=');
        auto vals = columns(pos == std::string::npos ? "" : line.substr(pos + 1)).values;
        if (ext == ".UXD") {
            const auto value = scalar(pos == std::string::npos ? "" : line.substr(pos + 1), true);
            vals = value ? std::vector<float>{*value} : std::vector<float>{};
            if ((key.starts_with("_START") || key.starts_with("_STEPSIZE") || key.starts_with("_WL")) &&
                !value)
                return false;
            if (key.starts_with("_START") && !vals.empty())
                start = vals[0];
            if (key.starts_with("_STEPSIZE") && !vals.empty())
                step = vals[0];
            if (key.starts_with("_WL") && key.size() > 3 && key[3] >= '1' && key[3] <= '3' && !vals.empty()) {
                auto index = std::size_t(key[3] - '1');
                out.waves.resize(std::max(out.waves.size(), index + 1));
                out.waves[index] = {WavelengthAngstrom{vals[0]},
                                    index == 0 ? 1 : (index == 1 ? .5 : double(.2f))};
            }
            if (key.starts_with("_WLRATIO") && !vals.empty())
                for (auto &w : out.waves)
                    w.relative_weight = vals[0];
            if (key.starts_with("_COUNTS")) {
                out.y = block(lines, i + 1);
                break;
            }
        } else if (ext == ".UDF") {
            std::replace(line.begin(), line.end(), ',', ' ');
            auto p = line.find(' ');
            vals = columns(p == std::string::npos ? "" : line.substr(p + 1)).values;
            if (key.starts_with("LABDAALPHA1") && vals.size() >= 2)
                out.waves = {{WavelengthAngstrom{vals[0]}, 1}};
            if (key.starts_with("DATAANGLERANGE") && vals.size() >= 2)
                start = vals[0];
            if (key.starts_with("SCANSTEPSIZE") && !vals.empty())
                step = vals[0];
            if (key.starts_with("RAWSCAN")) {
                out.y = block(lines, i + 1, true);
                break;
            }
        } else if (ext == ".CPI" && key == "SCANDATA") {
            out.y = block(lines, i + 1);
            break;
        } else if (ext == ".XRDML") {
            if (key.starts_with("<DATAPOINTS")) {
                xml_data = true;
                continue;
            }
            if (key.starts_with("</DATAPOINTS"))
                break;
            if (xml_data && key.starts_with("<POSITIONS")) {
                xml_two_theta = key.find("2THETA") != std::string::npos;
                continue;
            }
            if (key.starts_with("</POSITIONS")) {
                xml_two_theta = false;
                continue;
            }
            auto a = line.find('>'), b = line.rfind('<');
            if (a == std::string::npos || b <= a)
                continue;
            auto payload = line.substr(a + 1, b - a - 1);
            if (xml_data && (key.starts_with("<INTENSITIES") || key.starts_with("<COUNTS"))) {
                out.y = columns(payload, line.size()).values;
                break; // The first measured block is the Legacy input pattern.
            } else if ((!xml_data && key.starts_with("<KALPHA1>")) ||
                       (xml_two_theta &&
                        (key.starts_with("<STARTPOSITION>") || key.starts_with("<ENDPOSITION>")))) {
                const auto value = scalar(payload);
                if (!value)
                    return false;
                if (key.starts_with("<KALPHA1>"))
                    out.waves = {{WavelengthAngstrom{*value}, 1}};
                if (key.starts_with("<STARTPOSITION>"))
                    start = *value;
                if (key.starts_with("<ENDPOSITION>"))
                    end = *value;
            }
        }
    }
    if (ext == ".XRDML" && out.y.size() > 1)
        step = (end - start) / float(out.y.size() - 1);
    return grid(out, start, step);
}
bool gsas(const Lines &lines, Data &out) {
    for (std::size_t i = 0; i < lines.size(); ++i) {
        std::istringstream header(lines[i]);
        std::string bank, mode, format;
        int index, count, records;
        float start, step, unused1, unused2;
        if (!(header >> bank >> index >> count >> records >> mode >> start >> step >> unused1 >> unused2 >>
              format) ||
            bank != "BANK")
            continue;
        if (mode != "CONST" || count <= 0 || (format != "STD" && format != "ESD"))
            return false;
        for (++i; i < lines.size() && out.y.size() < std::size_t(count); ++i) {
            auto width = format == "STD" ? 8u : 16u;
            for (std::size_t j = 0; j < lines[i].size() && out.y.size() < std::size_t(count); j += width) {
                if (format == "STD" && lines[i].size() - j < 2)
                    return false;
                auto value =
                    number(trim(lines[i].substr(j + (format == "STD" ? 2 : 0), format == "STD" ? 6 : 8)));
                if (!value)
                    return false;
                out.y.push_back(*value);
            }
        }
        return out.y.size() == std::size_t(count) && grid(out, start / 100.0f, step / 100.0f);
    }
    return false;
}
Error failure(std::string message, ErrorCode code = ErrorCode::invalid_input) {
    return {code, "pattern.file", std::move(message), "pattern_read_failed"};
}
} // namespace
Result<PatternRead> read_pattern(std::istream &input, std::string extension, std::string source_id) {
    Lines lines;
    for (std::string line; std::getline(input, line);)
        lines.push_back(std::move(line));
    if (input.bad())
        return Result<PatternRead>{failure("Cannot read pattern file", ErrorCode::missing_data)};
    auto ext = upper(extension);
    Data data;
    bool ok = false;
    if (ext == ".XY" || ext == ".XYE")
        ok = xy(lines, data);
    else if (ext == ".DAT" || ext == ".TXT" || ext == ".POW") {
        ok = xy(lines, data);
        if (!ok) {
            data = {};
            ok = counts(lines, data, ext);
        }
    } else if (ext == ".DBW" || ext == ".XDD" || ext == ".XDA")
        ok = counts(lines, data, ext);
    else if (ext == ".UXD" || ext == ".CPI" || ext == ".UDF" || ext == ".XRDML")
        ok = vendor(lines, data, ext);
    else if (ext == ".GDA")
        ok = gsas(lines, data);
    else if (ext == ".CIF" || ext == ".RTV")
        return Result<PatternRead>{
            failure("Use Import Powder CIF (Legacy) for this retained format", ErrorCode::unsupported)};
    else
        return Result<PatternRead>{
            failure("Unsupported pattern format: " + extension, ErrorCode::unsupported)};
    if (!ok || data.x.size() < 2)
        return Result<PatternRead>{failure("Expected at least two pattern samples")};
    PatternRead result;
    result.file_wavelength = !data.waves.empty();
    result.nominal_step = data.step;
    auto &p = result.pattern;
    p.id = {1};
    p.source_id = std::move(source_id);
    p.radiation.kind = RadiationKind::x_ray;
    p.radiation.lines = data.waves.empty() ? std::vector<RadiationLine>{{radiation::default_wavelength, 1}}
                                           : std::move(data.waves);
    p.original_observed.emplace(data.y.begin(), data.y.end());
    const float maximum = *std::max_element(data.y.begin(), data.y.end());
    for (std::size_t i = 0; i < data.x.size(); ++i) {
        p.two_theta.emplace_back(data.x[i]);
        const float scaled = 1000.0f * data.y[i];
        p.observed.push_back(scaled / maximum);
    }
    p.corrected_two_theta = p.two_theta;
    p.calculation_range = AngularRange{p.two_theta.front(), p.two_theta.back()};
    p.processing = {false, false, true};
    p.original_kalpha2_subtracted = false;
    p.intensity_scale = IntensityScale::relative;
    p.source_precision = SourcePrecision::float32_abi; // text compatibility precision, no ABI involved
    auto issues = validate(p);
    if (!issues.empty())
        return Result<PatternRead>{issues.front()};
    return Result<PatternRead>{std::move(result)};
}
} // namespace qualpf::io
