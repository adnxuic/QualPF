#pragma once
#include "cif_document.h"

namespace qualpf::io::cif {

enum class TokenKind { value, tag, data, loop, stop, end };
struct Token {
    TokenKind kind;
    Value value;
};

// Streaming tokenizer; input must outlive this object. Returned tokens own bytes.
class Tokenizer {
  public:
    explicit Tokenizer(std::string_view input, Limits limits = {}) : input_(input), limits_(limits) {}
    [[nodiscard]] Result<Token> next();

  private:
    [[nodiscard]] Result<Token> quoted();
    [[nodiscard]] Result<Token> multiline();
    [[nodiscard]] Token token(TokenKind kind, Location start, ValueKind valueKind, std::string text) const;
    void advance();
    std::string_view input_;
    Limits limits_;
    Location at_;
    std::size_t count_ = 0;
};

} // namespace qualpf::io::cif
