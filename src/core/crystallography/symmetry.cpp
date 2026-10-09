#include "qualpf/crystallography/symmetry.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace qualpf::crystallography {
namespace {
using Rotation = SymmetryOperation::Rotation;
using Translation = SymmetryOperation::Translation;
constexpr std::int64_t det(const Rotation &r) {
    return std::int64_t(r[0]) * (std::int64_t(r[4]) * r[8] - std::int64_t(r[5]) * r[7]) -
           std::int64_t(r[1]) * (std::int64_t(r[3]) * r[8] - std::int64_t(r[5]) * r[6]) +
           std::int64_t(r[2]) * (std::int64_t(r[3]) * r[7] - std::int64_t(r[4]) * r[6]);
}
constexpr int mod12(std::int64_t n) { return static_cast<int>((n % 12 + 12) % 12); }
Error error(std::string field, std::string reason) {
    return {ErrorCode::invalid_input, std::move(field), "Invalid symmetry input",
            std::move(reason)};
}
double wrap(double x) {
    double y = x - std::floor(x);
    return y == 0 || y == 1 ? 0 : y; // canonical positive zero, rounded upper endpoint
}
} // namespace

Result<SymmetryOperation> SymmetryOperation::create(Rotation r, Translation t) {
    for (auto n : r)
        if (n < -rotation_bound || n > rotation_bound)
            return Result<SymmetryOperation>(error("symmetry.rotation", "rotation_range"));
    if (det(r) != 1 && det(r) != -1)
        return Result<SymmetryOperation>(error("symmetry.rotation", "not_unimodular"));
    for (auto &n : t)
        n = mod12(n);
    return Result<SymmetryOperation>(SymmetryOperation(r, t));
}
std::int64_t SymmetryOperation::determinant() const noexcept { return det(rotation_); }
Result<SymmetryOperation> SymmetryOperation::compose(const SymmetryOperation &b) const {
    Rotation r{};
    Translation t{};
    for (int i = 0; i < 3; ++i) {
        std::int64_t tr = translation_[i];
        for (int j = 0; j < 3; ++j) {
            std::int64_t v = 0;
            for (int k = 0; k < 3; ++k)
                v += std::int64_t(rotation_[3 * i + k]) * b.rotation_[3 * k + j];
            if (v < -rotation_bound || v > rotation_bound)
                return Result<SymmetryOperation>(error("symmetry.rotation", "rotation_range"));
            r[3 * i + j] = static_cast<int>(v);
            tr += std::int64_t(rotation_[3 * i + j]) * b.translation_[j];
        }
        t[i] = mod12(tr);
    }
    return create(r, t);
}
Result<SymmetryOperation> SymmetryOperation::inverse() const {
    Rotation r{};
    Translation t{};
    const auto d = determinant();
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const int a = (j + 1) % 3, b = (j + 2) % 3, c = (i + 1) % 3, e = (i + 2) % 3;
            const auto v = (std::int64_t(rotation_[3 * a + c]) * rotation_[3 * b + e] -
                            std::int64_t(rotation_[3 * a + e]) * rotation_[3 * b + c]) *
                           d;
            if (v < -rotation_bound || v > rotation_bound)
                return Result<SymmetryOperation>(error("symmetry.rotation", "rotation_range"));
            r[3 * i + j] = static_cast<int>(v);
        }
    for (int i = 0; i < 3; ++i) {
        std::int64_t v = 0;
        for (int j = 0; j < 3; ++j)
            v -= std::int64_t(r[3 * i + j]) * translation_[j];
        t[i] = mod12(v);
    }
    return create(r, t);
}
Result<std::array<double, 3>> SymmetryOperation::wrap_fractional(std::array<double, 3> x) {
    for (auto &v : x) {
        if (!std::isfinite(v))
            return Result<std::array<double, 3>>(error("symmetry.coordinate", "non_finite"));
        v = wrap(v);
    }
    return Result<std::array<double, 3>>(x);
}
Result<std::array<double, 3>> SymmetryOperation::apply(std::array<double, 3> x,
                                                       bool wrapped) const {
    for (auto &v : x) {
        if (!std::isfinite(v))
            return Result<std::array<double, 3>>(error("symmetry.coordinate", "non_finite"));
        if (wrapped)
            v = wrap(v); // integral rotation makes reduction before multiplication
                         // exact modulo Z
    }
    std::array<double, 3> y{};
    for (int i = 0; i < 3; ++i) {
        y[i] = translation_[i] / 12.0;
        for (int j = 0; j < 3; ++j)
            y[i] += rotation_[3 * i + j] * x[j];
        if (!std::isfinite(y[i]))
            return Result<std::array<double, 3>>(
                error("symmetry.coordinate", "coordinate_overflow"));
        if (wrapped)
            y[i] = wrap(y[i]);
    }
    return Result<std::array<double, 3>>(y);
}

namespace detail {
struct SymmetryTableBuilder {
    static consteval SymmetryOperation operation(Rotation r, Translation t) {
        for (auto n : r)
            if (n < -SymmetryOperation::rotation_bound || n > SymmetryOperation::rotation_bound)
                throw "rotation range";
        if (det(r) != 1 && det(r) != -1)
            throw "invalid determinant";
        for (auto n : t)
            if (n < 0 || n >= 12)
                throw "translation range";
        return SymmetryOperation(r, t);
    }
};
struct SpaceGroupRecord {
    int number;
    SpaceGroupSetting setting;
    std::string_view xhm, hall, old1, old2, schoenflies;
    int primitive, centering, centrosymmetry, offset, count;
};
#include "space_group_data.inc"
} // namespace detail

int SpaceGroup::number() const noexcept { return record_->number; }
std::string_view SpaceGroup::hermann_mauguin() const noexcept { return record_->xhm; }
std::string_view SpaceGroup::hall() const noexcept { return record_->hall; }
std::string_view SpaceGroup::schoenflies() const noexcept { return record_->schoenflies; }
std::array<std::string_view, 2> SpaceGroup::alternative_symbols() const noexcept {
    return {record_->old1, record_->old2};
}
const SpaceGroupSetting &SpaceGroup::setting() const noexcept { return record_->setting; }
int SpaceGroup::primitive_count() const noexcept { return record_->primitive; }
int SpaceGroup::centering_count() const noexcept { return record_->centering; }
int SpaceGroup::centrosymmetry() const noexcept { return record_->centrosymmetry; }
std::span<const SymmetryOperation> SpaceGroup::operations() const noexcept {
    return {detail::operations + record_->offset, static_cast<std::size_t>(record_->count)};
}
Result<SpaceGroup> SpaceGroupRegistry::by_id(int id) const {
    if (id < 0 || id >= setting_count)
        return Result<SpaceGroup>(error("space_group.id", "invalid_id"));
    return Result<SpaceGroup>(SpaceGroup(detail::records + id));
}
Result<std::vector<SpaceGroup>> SpaceGroupRegistry::settings(int number) const {
    if (number < 1 || number > group_count)
        return Result<std::vector<SpaceGroup>>(error("space_group.number", "invalid_number"));
    std::vector<SpaceGroup> result;
    for (const auto &r : detail::records)
        if (r.number == number)
            result.push_back(SpaceGroup(&r));
    return Result<std::vector<SpaceGroup>>(std::move(result));
}
Result<SpaceGroup> SpaceGroupRegistry::by_number(int n, std::optional<int> index) const {
    if (n < 1 || n > group_count)
        return Result<SpaceGroup>(error("space_group.number", "invalid_number"));
    for (const auto &r : detail::records)
        if (r.number == n) {
            if (!index)
                return by_id(r.setting.standard_id);
            if (r.setting.index == *index)
                return by_id(r.setting.id);
        }
    return Result<SpaceGroup>(error("space_group.setting", "invalid_setting"));
}
namespace {
std::string lower(std::string_view s, bool compact = true) {
    std::string out;
    for (char c : s) {
        if (compact && (c == ' ' || c == '\t'))
            continue;
        out += c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
    }
    return out;
}
void replace_all(std::string &s, std::string_view from, std::string_view to) {
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}
std::string prepare(std::string_view input) {
    std::string s(input);
    replace_all(s, "_", "");
    replace_all(s, "~", "");
    replace_all(s, " ", "");
    replace_all(s, "\t", "");
    replace_all(s, "(1)", "1");
    auto pos = s.find("{origin@-1");
    if (pos != std::string::npos && pos > 0)
        s = s.substr(0, pos) + ":2";
    pos = s.find("(origin");
    if (pos != std::string::npos)
        s.resize(pos);
    if (s.size() > 1) {
        char last = s.back();
        if (last == 's' || last == 'S' || last == 'z' || last == 'Z') {
            const bool first = last == 's' || last == 'S';
            s.pop_back();
            if (s.back() != ':')
                s += ':';
            s += first ? '1' : '2';
            if (first && std::none_of(std::begin(detail::origin_one_aliases),
                                      std::end(detail::origin_one_aliases), [&](auto alias) {
                                          return lower(alias, false) == lower(s, false);
                                      }))
                s.resize(s.size() - 2);
        }
    }
    return lower(s);
}
bool hm_matches(const detail::SpaceGroupRecord &r, const std::string &s) {
    std::string name(r.xhm);
    if (s.find(':') == std::string::npos &&
        (name.ends_with(":2") || name.ends_with(":R") || name.ends_with(":H")))
        name.resize(name.size() - 2);
    if (lower(name) == s)
        return true;
    if (r.number >= 195) {
        auto cubic = name;
        auto pos = cubic.find("-3");
        if (pos != std::string::npos) {
            cubic.erase(pos, 1);
            if (lower(cubic) == s)
                return true;
        }
    }
    auto shortened = name;
    replace_all(shortened, " 1", "");
    if (lower(shortened) == s)
        return true;
    if (!r.setting.choice.empty() && r.setting.choice != "Unknown") {
        auto choice =
            shortened.substr(0, shortened.find(':')) + ":" + std::string(r.setting.choice);
        if (lower(choice) == s)
            return true;
        replace_all(choice, ":", "");
        if (lower(choice, false) == s)
            return true;
    }
    if (!r.old1.empty() &&
        !(name.ends_with(":1") &&
          lower(std::string_view(name).substr(0, name.size() - 2)) == lower(r.old1)) &&
        lower(r.old1) == s)
        return true;
    return r.old2.starts_with('H') && lower(r.old2) == s;
}
} // namespace
Result<SpaceGroup> SpaceGroupRegistry::find(std::string_view symbol, SymbolKind kind) const {
    if (kind != SymbolKind::hall && kind != SymbolKind::hermann_mauguin &&
        kind != SymbolKind::either)
        return Result<SpaceGroup>(error("space_group.symbol_kind", "invalid_symbol_kind"));
    // Deliberately reject overlong/control input instead of Legacy's silent
    // truncation.
    if (symbol.size() > 40)
        return Result<SpaceGroup>(error("space_group.symbol", "identifier_too_long"));
    for (unsigned char c : symbol)
        if ((c < 32 && c != '\t') || c >= 127)
            return Result<SpaceGroup>(error("space_group.symbol", "invalid_character"));
    const auto s = prepare(symbol);
    if (s.size() < 2)
        return Result<SpaceGroup>(error("space_group.symbol", "invalid_identifier"));
    if (kind != SymbolKind::hall)
        for (const auto &r : detail::records)
            if (hm_matches(r, s))
                return by_id(r.setting.id);
    if (kind != SymbolKind::hermann_mauguin)
        for (const auto &r : detail::records)
            if (lower(r.hall) == s)
                return by_id(r.setting.id);
    return Result<SpaceGroup>(error("space_group.symbol", "unknown_identifier"));
}
} // namespace qualpf::crystallography
