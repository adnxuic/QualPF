#pragma once
#include "cif_document.h"

namespace qualpf::io::cif {
// Returns a complete owned document or one locatable error, never partial data.
[[nodiscard]] Result<Document> parse(std::string_view bytes, std::string source = {}, Limits limits = {});
} // namespace qualpf::io::cif
