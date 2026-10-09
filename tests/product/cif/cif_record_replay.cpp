#include "qualpf/application/database/cif_card_record.h"
#include <charconv>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#ifdef QUALPF_CIF_REPLAY_SQL
#include "infrastructure/database/qt_sql/cif_database_writer.h"
#include <QCoreApplication>
#endif

namespace {
void json_string(std::string_view text) {
    constexpr auto hex = "0123456789abcdef";
    std::cout << '"';
    for (unsigned char c : text) {
        if (c == '"' || c == '\\')
            std::cout << '\\' << c;
        else if (c < 32)
            std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
        else
            std::cout << c;
    }
    std::cout << '"';
}
void field(const char *name, std::string_view text) {
    std::cout << ",\"" << name << "\":";
    json_string(text);
}
void error(const qualpf::database::WriteError &issue) {
    std::cout << "RECORD_JSON {\"success\":false";
    field("code", issue.code);
    field("source", issue.source);
    field("block", issue.block);
    field("field", issue.field);
    field("reason", issue.reason);
    std::cout << "}\n";
}
void dump(const qualpf::database::CifRecord &record) {
    std::cout << std::setprecision(17) << "RECORD_JSON {\"success\":true,\"id\":" << record.identity;
    field("source", record.source);
    field("block", record.block);
    field("name", record.name);
    field("mineral", record.mineral);
    field("formula", record.formula);
    field("group", record.space_group);
    field("system", record.crystal_system);
    field("subfile", record.subfile);
    std::cout << ",\"nat\":" << record.atom_count << ",\"nrefl\":" << record.reflection_count
              << ",\"z\":" << record.formula_units << ",\"volume\":" << record.volume
              << ",\"density\":" << record.density << ",\"mu\":" << record.linear_absorption
              << ",\"rir\":" << record.rir << ",\"cell\":[";
    for (std::size_t i = 0; i < record.cell.size(); ++i) {
        if (i)
            std::cout << ',';
        std::cout << record.cell[i];
    }
    std::cout << "],\"species\":[";
    for (std::size_t i = 0; i < record.elements.size(); ++i) {
        if (i)
            std::cout << ',';
        json_string(record.elements[i]);
    }
    std::cout << "],\"lines\":[";
    for (std::size_t i = 0; i < record.lines.size(); ++i) {
        if (i)
            std::cout << ',';
        const auto &line = record.lines[i];
        std::cout << '[' << line.hkl[0] << ',' << line.hkl[1] << ',' << line.hkl[2] << ','
                  << line.multiplicity << ',' << line.d << ',' << line.relative_intensity << ']';
    }
    std::cout << "]}\n";
}
} // namespace
int main(int argc, char **argv) {
#ifdef QUALPF_CIF_REPLAY_SQL
    QCoreApplication app(argc, argv);
    if (argc != 4)
        return 2;
#else
    if (argc != 3)
        return 2;
#endif
    const std::string_view identity_text(argv[2]);
    std::int32_t identity;
    const auto converted =
        std::from_chars(identity_text.data(), identity_text.data() + identity_text.size(), identity);
    if (converted.ec != std::errc{} || converted.ptr != identity_text.data() + identity_text.size())
        return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input)
        return 3;
    const std::string bytes{std::istreambuf_iterator<char>(input), {}};
    auto calculated = qualpf::application::CifCardService::from_bytes(bytes, argv[1]);
    if (const auto *issue = std::get_if<qualpf::application::CifCardIssue>(&calculated)) {
        error({issue->code, issue->source, issue->block, issue->field,
               issue->message + " (" + issue->core_reason + ")"});
        return 0;
    }
    auto adapted =
        qualpf::application::cif_card_record(std::get<qualpf::application::CifCard>(calculated), identity);
    if (const auto *issue = std::get_if<qualpf::database::WriteError>(&adapted)) {
        error(*issue);
        return 0;
    }
    const auto &record = std::get<qualpf::database::CifRecord>(adapted);
#ifdef QUALPF_CIF_REPLAY_SQL
    qualpf::database::CifDatabaseWriter writer;
    if (auto issue = writer.create(QString::fromLocal8Bit(argv[3]))) {
        error(*issue);
        return 0;
    }
    if (auto issue = writer.write(record)) {
        error(*issue);
        return 0;
    }
    writer.close();
#endif
    dump(record);
    return 0;
}
