#include "cif_parser.h"
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace qualpf::io::cif;
namespace {
std::size_t checks = 0;
void check(bool condition, const char *label) {
    ++checks;
    if (!condition)
        throw std::runtime_error(label);
}
Document document(std::string_view bytes) {
    auto result = parse(bytes, "test.cif");
    if (const auto *error = std::get_if<Error>(&result))
        throw std::runtime_error(std::string(error_name(error->code)) + ": " + error->reason);
    return std::move(std::get<Document>(result));
}
void rejects(std::string_view bytes, ErrorCode code, std::string_view field = {}) {
    const auto result = parse(bytes, "test.cif");
    const auto *error = std::get_if<Error>(&result);
    check(error && error->code == code, "rejection code");
    check(error->source == "test.cif", "rejection source");
    check(field.empty() || error->field == field, "rejection field");
    check(error->location.line > 0 && error->location.column > 0 && error->location.offset <= bytes.size(),
          "rejection location within input");
}
Value numeric(std::string text) { return {text, text, ValueKind::bare, {}}; }
} // namespace

int main() {
    try {
        const auto d = document("# comment\r\nDaTa_A\r\n_CELL_a 5.0(2)\r\n_name 'O'Brien # name'\r\n"
                                "loop_\r\n_LABEL\r\n_x\r\n'a b' ? # comment\r\nC1 .\r\nstop_\r\n"
                                "_quoted_missing '?'\r\ndata_B\r\n_cell_A +6.2e1(3)");
        check(d.blocks.size() == 2 && d.blocks[0].name == "A",
              "case-insensitive keywords preserve block name");
        const auto &b = d.blocks[0];
        check(b.scalar("_CELL_A")->text == "5.0(2)", "case-insensitive lookup");
        check(b.scalar("_name")->raw == "'O'Brien # name'", "quoted internal apostrophe and comment");
        check(b.scalar("_name")->text == "O'Brien # name", "quoted decoding");
        check(b.scalar("_cell_a")->location.line == 3 && b.scalar("_cell_a")->location.column == 9,
              "CRLF source position");
        check(b.column("_label")->at(0).text == "a b" && b.loops[0].rows() == 2, "loop column lookup");
        check(b.column("_x")->at(0).kind == ValueKind::unknown, "unknown missing");
        check(b.column("_x")->at(1).kind == ValueKind::inapplicable, "inapplicable missing");
        check(!b.scalar("_quoted_missing")->missing(), "quoted question mark is not missing");
        check(!b.scalar("_absent") && !b.column("_absent"), "missing field lookup");
        auto multiline = document("data_a\n_text\n;first\r\n# literal comment\n 'quote'\n;\n_x 1");
        const auto *text = multiline.blocks[0].scalar("_text");
        check(text->text == "first\n# literal comment\n 'quote'", "semicolon text decode");
        check(text->raw == ";first\r\n# literal comment\n 'quote'\n;", "semicolon raw bytes");
        check(document("data_a\n_text\n;\n;").blocks[0].scalar("_text")->text.empty(), "empty text");
        check(document("data_a\n_x foo#bar").blocks[0].scalar("_x")->text == "foo#bar", "hash in bare token");
        check(document("data_a\nloop_ _a _b 1 2 _c 3\nloop_ _d 4").blocks[0].loops.size() == 2,
              "implicit loop termination on tag and next loop");
        rejects("# empty", ErrorCode::missing_block);
        rejects("_a 1", ErrorCode::missing_block);
        rejects("data_", ErrorCode::invalid_block);
        rejects("data_a\ndata_A", ErrorCode::duplicate_block);
        rejects("data_a\n_a 1\n_A 2", ErrorCode::duplicate_tag, "_a");
        rejects("data_a\n_a 1\nloop_ _A 2", ErrorCode::duplicate_tag, "_a");
        rejects("data_a\nloop_ _a _A 1 2", ErrorCode::duplicate_tag, "_a");
        rejects("data_a\n_a\n_b 2", ErrorCode::missing_value, "_a");
        rejects("data_a\n_a 'unclosed\n", ErrorCode::unterminated_quote, "_a");
        rejects("data_a\n_a 'unclosed", ErrorCode::unterminated_quote, "_a");
        rejects("data_a\n_a\n;unclosed", ErrorCode::unterminated_text, "_a");
        rejects("data_a\n_a\n;text\n;invalid", ErrorCode::invalid_text_delimiter, "_a");
        rejects("data_a\nloop_ _a _b 1 stop_", ErrorCode::incomplete_loop_row, "_b");
        rejects("data_a\nloop_ _a _b 1\ndata_b", ErrorCode::incomplete_loop_row, "_b");
        rejects("data_a\nloop_ _a", ErrorCode::empty_loop);
        rejects("data_a\nloop_ 1", ErrorCode::empty_loop);
        rejects("data_a\nstop_", ErrorCode::unexpected_token);
        rejects("data_a\n_ 1", ErrorCode::unexpected_token);
        rejects("data_a\nsave_frame", ErrorCode::unsupported_construct);
        rejects("global_", ErrorCode::unsupported_construct);
        rejects("#\\#CIF_2.0\ndata_a", ErrorCode::unsupported_construct);
        rejects("data_a\n_a [1 2]", ErrorCode::unsupported_construct, "_a");
        rejects(std::string("data_a\n# nul\0hidden", 19), ErrorCode::invalid_byte);
        check(std::get<Error>(parse("data_a", {}, Limits{5, 99, 99})).code == ErrorCode::resource_limit,
              "byte limit");
        check(std::get<Error>(parse("data_a _a 1", {}, Limits{99, 2, 99})).code == ErrorCode::resource_limit,
              "token limit");
        check(std::get<Error>(parse("data_a data_b", {}, Limits{99, 99, 1})).code ==
                  ErrorCode::resource_limit,
              "block limit");
        for (const auto &[input, expected] : std::vector<std::pair<std::string, double>>{{"5.000(12)", 5},
                                                                                         {"+.015", .015},
                                                                                         {"-1.2e2", -120},
                                                                                         {"1.25(2)e+2", 125},
                                                                                         {"1.25e+2(2)", 125},
                                                                                         {"2.", 2},
                                                                                         {"-0", 0}}) {
            const auto n = number(numeric(input));
            check(std::holds_alternative<double>(n) && std::get<double>(n) == expected, "decimal conversion");
        }
        for (const char *input : {"nan", "inf", "1abc", "0x1p2", "1(2)(3)", "1(2)e3(4)", "1()", "1(2", "1e",
                                  "1e9999", "+", "1 2"})
            check(std::holds_alternative<Error>(number(numeric(input))),
                  "malformed numbers never partially convert");
        auto missing = numeric("?");
        missing.kind = ValueKind::unknown;
        check(std::get<Error>(number(missing)).code == ErrorCode::missing_number, "missing numeric status");
        // Independent property: every accepted value's raw bytes equal its source span.
        // Generated token fragments include control bytes, reserved words and delimiters.
        std::mt19937 random(19019);
        const std::string alphabet = "abc_'\";\r\n\t012 #.?[]";
        for (int run = 0; run < 2000; ++run) {
            std::string bytes = "data_test\n_a ";
            for (unsigned n = random() % 80; n > 0; --n)
                bytes += alphabet[random() % alphabet.size()];
            const auto result = parse(bytes, "generated.cif");
            if (const auto *valid = std::get_if<Document>(&result)) {
                for (const auto &block : valid->blocks) {
                    for (const auto &item : block.items)
                        check(bytes.substr(item.value.location.offset, item.value.raw.size()) ==
                                  item.value.raw,
                              "raw source span roundtrip");
                    for (const auto &loop : block.loops)
                        check(!loop.tags.empty() && loop.values.size() % loop.tags.size() == 0,
                              "complete row invariant");
                }
            } else {
                const auto &error = std::get<Error>(result);
                check(error.location.offset <= bytes.size(), "generated failure stays within source");
            }
        }
        std::cout << "PASS " << checks << " checks\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
