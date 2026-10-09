#include "cif_document.h"

#include <charconv>
#include <cmath>

namespace qualpf::io::cif {

std::string ascii_lower(std::string_view text) {
    std::string result(text);
    for (char &c : result) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
    }
    return result;
}

const Value *Block::scalar(std::string_view name) const {
    const auto key = ascii_lower(name);
    for (const auto &item : items) {
        if (item.tag.name == key)
            return &item.value;
    }
    return nullptr;
}

std::optional<LoopColumn> Block::column(std::string_view name) const {
    const auto key = ascii_lower(name);
    for (const auto &loop : loops) {
        for (std::size_t i = 0; i < loop.tags.size(); ++i) {
            if (loop.tags[i].name == key)
                return LoopColumn{&loop, i};
        }
    }
    return std::nullopt;
}

const char *error_name(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::invalid_byte:
        return "invalid_byte";
    case ErrorCode::unsupported_construct:
        return "unsupported_construct";
    case ErrorCode::unterminated_quote:
        return "unterminated_quote";
    case ErrorCode::unterminated_text:
        return "unterminated_text";
    case ErrorCode::invalid_text_delimiter:
        return "invalid_text_delimiter";
    case ErrorCode::missing_block:
        return "missing_block";
    case ErrorCode::invalid_block:
        return "invalid_block";
    case ErrorCode::duplicate_block:
        return "duplicate_block";
    case ErrorCode::unexpected_token:
        return "unexpected_token";
    case ErrorCode::missing_value:
        return "missing_value";
    case ErrorCode::duplicate_tag:
        return "duplicate_tag";
    case ErrorCode::empty_loop:
        return "empty_loop";
    case ErrorCode::incomplete_loop_row:
        return "incomplete_loop_row";
    case ErrorCode::resource_limit:
        return "resource_limit";
    case ErrorCode::invalid_number:
        return "invalid_number";
    case ErrorCode::missing_number:
        return "missing_number";
    }
    return "unknown_error";
}

Result<double> number(const Value &value) {
    const auto fail = [&](ErrorCode code, std::string reason) -> Result<double> {
        return Error{code, value.location, {}, {}, {}, std::move(reason)};
    };
    if (value.missing())
        return fail(ErrorCode::missing_number, "Missing numeric value");
    const auto &text = value.text;
    std::size_t i = 0;
    const auto digit = [](char c) { return c >= '0' && c <= '9'; };
    if (i < text.size() && (text[i] == '+' || text[i] == '-'))
        ++i;
    const auto begin = i;
    while (i < text.size() && digit(text[i]))
        ++i;
    std::size_t digits = i - begin;
    if (i < text.size() && text[i] == '.') {
        const auto start = ++i;
        while (i < text.size() && digit(text[i]))
            ++i;
        digits += i - start;
    }
    if (digits == 0)
        return fail(ErrorCode::invalid_number, "Expected a decimal number");
    std::string numeric = text.substr(0, i);
    bool uncertainty = false;
    const auto consume_uncertainty = [&]() {
        if (i == text.size() || text[i] != '(')
            return true;
        if (uncertainty)
            return false;
        uncertainty = true;
        const auto start = ++i;
        while (i < text.size() && digit(text[i]))
            ++i;
        if (i == start || i == text.size() || text[i] != ')')
            return false;
        ++i;
        return true;
    };
    if (!consume_uncertainty())
        return fail(ErrorCode::invalid_number, "Invalid uncertainty digits");
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        const auto start = i++;
        if (i < text.size() && (text[i] == '+' || text[i] == '-'))
            ++i;
        const auto exponent = i;
        while (i < text.size() && digit(text[i]))
            ++i;
        if (i == exponent)
            return fail(ErrorCode::invalid_number, "Missing exponent digits");
        numeric += text.substr(start, i - start);
        if (!consume_uncertainty())
            return fail(ErrorCode::invalid_number, "Invalid uncertainty digits");
    }
    if (i != text.size())
        return fail(ErrorCode::invalid_number, "Unexpected numeric suffix");
    std::string_view input = numeric;
    if (input.front() == '+')
        input.remove_prefix(1);
    double result = 0;
    const auto converted = std::from_chars(input.data(), input.data() + input.size(), result);
    if (converted.ec != std::errc{} || converted.ptr != input.data() + input.size() || !std::isfinite(result))
        return fail(ErrorCode::invalid_number, "Number is outside the finite binary64 range");
    return result;
}

} // namespace qualpf::io::cif
