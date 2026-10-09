#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace qualpf::io::cif {

struct Location {
  std::size_t offset = 0; // byte offset in the original input
  std::size_t line = 1;
  std::size_t column = 1;
};

enum class ErrorCode {
  invalid_byte,
  unsupported_construct,
  unterminated_quote,
  unterminated_text,
  invalid_text_delimiter,
  missing_block,
  invalid_block,
  duplicate_block,
  unexpected_token,
  missing_value,
  duplicate_tag,
  empty_loop,
  incomplete_loop_row,
  resource_limit,
  invalid_number,
  missing_number
};

struct Error {
  ErrorCode code;
  Location location;
  std::string source;
  std::string block;
  std::string field;
  std::string reason;
};

template <class T> using Result = std::variant<T, Error>;

enum class ValueKind {
  bare,
  single_quoted,
  double_quoted,
  text,
  unknown,
  inapplicable
};

struct Value {
  std::string text; // delimiters removed; semicolon text uses LF line endings
  std::string raw;  // exact bytes, including delimiters and original newlines
  ValueKind kind = ValueKind::bare;
  Location location;
  [[nodiscard]] bool missing() const noexcept {
    return kind == ValueKind::unknown || kind == ValueKind::inapplicable;
  }
};

struct Tag {
  std::string name;     // ASCII lower-case lookup identity
  std::string spelling; // original bytes
  Location location;
};
struct Item {
  Tag tag;
  Value value;
};
struct Loop {
  Location location;
  std::vector<Tag> tags;
  std::vector<Value> values; // row major, a complete multiple of tags.size()
  [[nodiscard]] std::size_t rows() const noexcept {
    return tags.empty() ? 0 : values.size() / tags.size();
  }
};
struct LoopColumn {
  const Loop *loop;
  std::size_t column;
  [[nodiscard]] const Value &at(std::size_t row) const {
    return loop->values.at(row * loop->tags.size() + column);
  }
};
struct Block {
  std::string name; // data_ suffix, original case
  Location location;
  std::vector<Item> items;
  std::vector<Loop> loops;
  [[nodiscard]] const Value *scalar(std::string_view name) const;
  [[nodiscard]] std::optional<LoopColumn> column(std::string_view name) const;
};
struct Document {
  std::string source;
  std::vector<Block> blocks;
  // Exact input bytes, retained for source hashing and selection reload.
  std::string original_bytes;
};

// Syntax limits bound memory use before any crystallographic interpretation.
struct Limits {
  std::size_t bytes = 64 * 1024 * 1024;
  std::size_t tokens = 2'000'000;
  std::size_t blocks = 4096;
};

[[nodiscard]] std::string ascii_lower(std::string_view text);
[[nodiscard]] const char *error_name(ErrorCode code) noexcept;

// Decimal CIF number with optional uncertainty digits and decimal exponent.
// Uncertainty is validated and retained in Value; it is not a physical
// tolerance. This conversion is locale independent and never accepts
// partial/NaN/Inf input.
[[nodiscard]] Result<double> number(const Value &value);

} // namespace qualpf::io::cif
