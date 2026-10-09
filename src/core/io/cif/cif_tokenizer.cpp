#include "cif_tokenizer.h"

namespace qualpf::io::cif {
namespace {
bool space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
Error error(ErrorCode code, Location location, std::string reason) {
    return {code, location, {}, {}, {}, std::move(reason)};
}
} // namespace

void Tokenizer::advance() {
    const char c = input_[at_.offset++];
    if (c == '\r') {
        if (at_.offset < input_.size() && input_[at_.offset] == '\n')
            ++at_.offset;
        ++at_.line;
        at_.column = 1;
    } else if (c == '\n') {
        ++at_.line;
        at_.column = 1;
    } else {
        ++at_.column;
    }
}

Token Tokenizer::token(TokenKind kind, Location start, ValueKind valueKind, std::string text) const {
    return {kind,
            {std::move(text), std::string(input_.substr(start.offset, at_.offset - start.offset)), valueKind,
             start}};
}

Result<Token> Tokenizer::quoted() {
    const Location start = at_;
    const char quote = input_[at_.offset];
    advance();
    const auto begin = at_.offset;
    while (at_.offset < input_.size()) {
        const char c = input_[at_.offset];
        if (c == '\r' || c == '\n')
            return error(ErrorCode::unterminated_quote, start, "Quoted values cannot cross a line boundary");
        if (c == quote && (at_.offset + 1 == input_.size() || space(input_[at_.offset + 1]))) {
            std::string value(input_.substr(begin, at_.offset - begin));
            advance();
            return token(TokenKind::value, start,
                         quote == '\'' ? ValueKind::single_quoted : ValueKind::double_quoted,
                         std::move(value));
        }
        advance();
    }
    return error(ErrorCode::unterminated_quote, start, "Quoted value has no closing delimiter");
}

Result<Token> Tokenizer::multiline() {
    const Location start = at_;
    advance();
    std::string value;
    while (at_.offset < input_.size()) {
        const char c = input_[at_.offset];
        if (c == ';' && at_.column == 1) {
            if (!value.empty() && value.back() == '\n')
                value.pop_back();
            advance();
            if (at_.offset < input_.size() && !space(input_[at_.offset]))
                return error(ErrorCode::invalid_text_delimiter, at_,
                             "Text closing semicolon must be followed by whitespace");
            return token(TokenKind::value, start, ValueKind::text, std::move(value));
        }
        value += (c == '\r' ? '\n' : c);
        advance();
    }
    return error(ErrorCode::unterminated_text, start, "Text field has no column-one closing semicolon");
}

Result<Token> Tokenizer::next() {
    if (at_.offset == 0) {
        if (input_.size() > limits_.bytes)
            return error(ErrorCode::resource_limit, at_, "Input exceeds the configured byte limit");
        // Validate every byte, including comments, so NUL cannot hide a suffix.
        Location scan;
        for (std::size_t i = 0; i < input_.size(); ++i) {
            const auto c = static_cast<unsigned char>(input_[i]);
            scan.offset = i;
            if ((c < 32 && !space(static_cast<char>(c))) || c == 127)
                return error(ErrorCode::invalid_byte, scan, "Unsupported control byte in CIF input");
            if (c == '\r' || (c == '\n' && (i == 0 || input_[i - 1] != '\r'))) {
                ++scan.line;
                scan.column = 1;
            } else if (c != '\n')
                ++scan.column;
        }
        if (input_.starts_with("#\\#CIF_2.0"))
            return error(ErrorCode::unsupported_construct, at_,
                         "CIF 2.0 is outside the supported syntax scope");
    }
    while (at_.offset < input_.size()) {
        if (space(input_[at_.offset]))
            advance();
        else if (input_[at_.offset] == '#') {
            while (at_.offset < input_.size() && input_[at_.offset] != '\r' && input_[at_.offset] != '\n')
                advance();
        } else
            break;
    }
    const Location start = at_;
    if (at_.offset == input_.size())
        return token(TokenKind::end, start, ValueKind::bare, {});
    if (++count_ > limits_.tokens)
        return error(ErrorCode::resource_limit, at_, "Input exceeds the configured token limit");
    const char c = input_[at_.offset];
    if (c == '\'' || c == '"')
        return quoted();
    if (c == ';' && at_.column == 1)
        return multiline();
    if (c == '$' || c == '[' || c == ']' || c == '{' || c == '}')
        return error(ErrorCode::unsupported_construct, at_,
                     "Frame references and CIF 2.0 containers are unsupported");
    while (at_.offset < input_.size() && !space(input_[at_.offset]))
        advance();
    std::string text(input_.substr(start.offset, at_.offset - start.offset));
    const auto lower = ascii_lower(text);
    TokenKind kind = TokenKind::value;
    ValueKind valueKind = ValueKind::bare;
    if (lower.starts_with("save_") || lower == "global_")
        return error(ErrorCode::unsupported_construct, start,
                     "Save frames and global blocks are unsupported");
    if (lower.starts_with("data_"))
        kind = TokenKind::data;
    else if (lower == "loop_")
        kind = TokenKind::loop;
    else if (lower == "stop_")
        kind = TokenKind::stop;
    else if (c == '_')
        kind = TokenKind::tag;
    else if (text == "?")
        valueKind = ValueKind::unknown;
    else if (text == ".")
        valueKind = ValueKind::inapplicable;
    return token(kind, start, valueKind, std::move(text));
}
} // namespace qualpf::io::cif
