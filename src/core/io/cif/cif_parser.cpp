#include "cif_parser.h"
#include "cif_tokenizer.h"

#include <unordered_set>

namespace qualpf::io::cif {

Result<Document> parse(std::string_view bytes, std::string source,
                       Limits limits) {
  Tokenizer tokenizer(bytes, limits);
  Document document{std::move(source), {}, {}};
  if (bytes.size() <= limits.bytes)
    document.original_bytes = bytes;
  std::unordered_set<std::string> blocks, tags;
  Block *block = nullptr;
  std::string field;
  const auto contextualize = [&](Error error) -> Result<Document> {
    error.source = document.source;
    error.block = block ? block->name : std::string{};
    error.field = field;
    return error;
  };
  const auto fail = [&](ErrorCode code, const Token &at, std::string reason) {
    return contextualize(
        {code, at.value.location, {}, {}, {}, std::move(reason)});
  };
  auto next = tokenizer.next();
  for (;;) {
    if (const auto *error = std::get_if<Error>(&next))
      return contextualize(*error);
    Token current = std::move(std::get<Token>(next));
    field.clear();
    if (current.kind == TokenKind::end) {
      if (!block)
        return fail(ErrorCode::missing_block, current, "CIF has no data block");
      return document;
    }
    if (current.kind == TokenKind::data) {
      auto name = current.value.text.substr(5);
      if (name.empty())
        return fail(ErrorCode::invalid_block, current,
                    "Data block requires a name");
      if (!blocks.insert(ascii_lower(name)).second)
        return fail(ErrorCode::duplicate_block, current,
                    "Duplicate data block name");
      if (document.blocks.size() >= limits.blocks)
        return fail(ErrorCode::resource_limit, current,
                    "Input exceeds the configured block limit");
      document.blocks.push_back(
          {std::move(name), current.value.location, {}, {}});
      block = &document.blocks.back();
      tags.clear();
      next = tokenizer.next();
      continue;
    }
    if (!block)
      return fail(ErrorCode::missing_block, current,
                  "Expected a data block before fields");
    if (current.kind == TokenKind::tag) {
      field = ascii_lower(current.value.text);
      if (field.size() == 1)
        return fail(ErrorCode::unexpected_token, current,
                    "Tag requires a name after underscore");
      if (!tags.insert(field).second)
        return fail(ErrorCode::duplicate_tag, current,
                    "Duplicate tag within data block");
      next = tokenizer.next();
      if (const auto *error = std::get_if<Error>(&next))
        return contextualize(*error);
      Token value = std::move(std::get<Token>(next));
      if (value.kind != TokenKind::value)
        return fail(ErrorCode::missing_value, value,
                    "Scalar tag requires one value");
      block->items.push_back(
          {{field, current.value.text, current.value.location},
           std::move(value.value)});
      next = tokenizer.next();
      continue;
    }
    if (current.kind != TokenKind::loop)
      return fail(ErrorCode::unexpected_token, current,
                  "Expected a tag, loop or data block");
    Loop loop{current.value.location, {}, {}};
    next = tokenizer.next();
    for (;;) {
      if (const auto *error = std::get_if<Error>(&next))
        return contextualize(*error);
      const auto &tag = std::get<Token>(next);
      if (tag.kind != TokenKind::tag)
        break;
      field = ascii_lower(tag.value.text);
      if (field.size() == 1)
        return fail(ErrorCode::unexpected_token, tag,
                    "Loop tag requires a name");
      if (!tags.insert(field).second)
        return fail(ErrorCode::duplicate_tag, tag,
                    "Duplicate tag within data block");
      loop.tags.push_back({field, tag.value.text, tag.value.location});
      next = tokenizer.next();
    }
    if (loop.tags.empty())
      return fail(ErrorCode::empty_loop, current, "Loop requires tag names");
    for (;;) {
      field = loop.tags[loop.values.size() % loop.tags.size()].name;
      if (const auto *error = std::get_if<Error>(&next))
        return contextualize(*error);
      auto &value = std::get<Token>(next);
      if (value.kind != TokenKind::value)
        break;
      loop.values.push_back(std::move(value.value));
      next = tokenizer.next();
    }
    const auto &end = std::get<Token>(next);
    if (loop.values.empty())
      return fail(ErrorCode::empty_loop, end,
                  "Loop requires at least one complete row");
    if (loop.values.size() % loop.tags.size() != 0)
      return fail(ErrorCode::incomplete_loop_row, end,
                  "Loop row has fewer values than tag names");
    block->loops.push_back(std::move(loop));
    if (end.kind == TokenKind::stop)
      next = tokenizer.next();
  }
}
} // namespace qualpf::io::cif
