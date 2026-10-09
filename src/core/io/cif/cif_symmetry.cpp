#include "cif_import.h"

#include <cstdint>

namespace qualpf::io::cif {

ImportResult<crystallography::SymmetryOperation>
parse_operation(const Value &value, std::string source, std::string block) {
  using Operation = crystallography::SymmetryOperation;
  const auto fail = [&](std::string code,
                        std::string message) -> ImportResult<Operation> {
    return ImportIssue{std::move(code),
                       source,
                       block,
                       "_space_group_symop_operation_xyz",
                       value.location,
                       std::move(message),
                       {},
                       {},
                       {}};
  };
  if (value.missing())
    return fail("missing_operator", "Symmetry operation is missing");
  std::string s;
  for (char c : ascii_lower(value.text)) {
    if (c != ' ' && c != '\t')
      s += c;
  }
  Operation::Rotation rotation{};
  Operation::Translation translation{};
  std::size_t i = 0;
  for (int row = 0; row < 3; ++row) {
    bool first = true;
    bool any = false;
    std::int64_t shift = 0;
    while (i < s.size() && s[i] != ',') {
      int sign = 1;
      if (s[i] == '+' || s[i] == '-')
        sign = s[i++] == '-' ? -1 : 1;
      else if (!first)
        return fail("malformed_operator",
                    "Operator terms require an explicit sign");
      first = false;
      if (i == s.size())
        return fail("malformed_operator", "Operator ends after a sign");
      if (s[i] >= 'x' && s[i] <= 'z') {
        const auto col = static_cast<unsigned>(s[i++] - 'x');
        auto &coefficient = rotation[static_cast<std::size_t>(row) * 3 + col];
        coefficient += sign;
        if (coefficient < -Operation::rotation_bound ||
            coefficient > Operation::rotation_bound)
          return fail("operator_range",
                      "Rotation coefficient exceeds the exact representation");
      } else {
        const auto integer = [&]() -> std::optional<std::int64_t> {
          const auto start = i;
          std::int64_t n = 0;
          while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            n = n * 10 + s[i++] - '0';
            if (n > 1'000'000)
              return std::nullopt;
          }
          return i == start ? std::nullopt : std::optional<std::int64_t>(n);
        };
        const auto numerator = integer();
        if (!numerator)
          return fail("malformed_operator",
                      "Expected x, y, z or an integer/rational translation");
        std::int64_t denominator = 1;
        if (i < s.size() && s[i] == '/') {
          ++i;
          const auto parsed = integer();
          if (!parsed || *parsed == 0)
            return fail("malformed_operator",
                        "Invalid translation denominator");
          denominator = *parsed;
        }
        const auto scaled = *numerator * Operation::translation_denominator;
        if (scaled % denominator != 0)
          return fail("unsupported_translation",
                      "Translation is not exactly representable in twelfths");
        shift = (shift + sign * (scaled / denominator)) %
                Operation::translation_denominator;
      }
      any = true;
    }
    if (!any)
      return fail("malformed_operator",
                  "Operator requires three nonempty coordinate expressions");
    translation[static_cast<std::size_t>(row)] =
        static_cast<std::int32_t>(shift);
    if (row < 2) {
      if (i == s.size() || s[i++] != ',')
        return fail("malformed_operator",
                    "Operator requires three coordinates");
    } else if (i != s.size())
      return fail("malformed_operator",
                  "Operator has more than three coordinates");
  }
  const auto operation = Operation::create(rotation, translation);
  if (!operation)
    return fail("invalid_operator", operation.error().reason);
  return operation.value();
}
} // namespace qualpf::io::cif
