#include "qualpf/crystallography/symmetry.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
using namespace qualpf::crystallography;
int main() {
    SpaceGroupRegistry registry;
    std::cout << "{\"groups\":[";
    for (int id = 0; id < registry.setting_count; ++id) {
        const auto g = registry.by_id(id).value();
        const auto s = g.setting();
        if (id)
            std::cout << ',';
        std::cout << "{\"id\":" << id << ",\"number\":" << g.number()
                  << ",\"setting_index\":" << s.index << ",\"standard_id\":" << s.standard_id
                  << ",\"reported_setting_index\":" << s.index
                  << ",\"operation_count\":" << g.operations().size()
                  << ",\"primitive_count\":" << g.primitive_count()
                  << ",\"centering_count\":" << g.centering_count()
                  << ",\"centrosymmetry\":" << g.centrosymmetry();
        auto field = [](const char *key, std::string_view value) {
            std::cout << ',' << std::quoted(key) << ':' << std::quoted(std::string(value));
        };
        field("xhm", g.hermann_mauguin());
        field("hall", g.hall());
        field("choice", s.choice);
        field("old1", g.alternative_symbols()[0]);
        field("old2", g.alternative_symbols()[1]);
        field("schoenflies", g.schoenflies());
        field("to_setting", s.to_setting);
        field("to_standard", s.to_standard);
        std::cout << ",\"operations\":[";
        bool first = true;
        for (const auto &op : g.operations()) {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << "{\"rotation\":[";
            for (int i = 0; i < 9; ++i) {
                if (i)
                    std::cout << ',';
                std::cout << op.rotation()[i];
            }
            std::cout << "],\"translation_twelfths\":[";
            for (int i = 0; i < 3; ++i) {
                if (i)
                    std::cout << ',';
                std::cout << op.translation_twelfths()[i];
            }
            std::cout << "]}";
        }
        std::cout << "]}";
    }
    std::cout << "],\"defaults\":{";
    for (int n = 1; n <= 230; ++n) {
        if (n > 1)
            std::cout << ',';
        std::cout << std::quoted(std::to_string(n)) << ':'
                  << registry.by_number(n).value().setting().id;
    }
    std::cout << "},\"lookups\":[";
    std::string line;
    bool first = true;
    while (std::getline(std::cin, line)) {
        if (line.size() < 2)
            return 2;
        auto result = registry.find(line.substr(2), line[0] == 'M'   ? SymbolKind::hermann_mauguin
                                                    : line[0] == 'H' ? SymbolKind::hall
                                                                     : SymbolKind::either);
        if (line[0] == 'N') {
            int n = 0, index = 0;
            std::istringstream(line.substr(2)) >> n >> index;
            result = registry.by_number(n, index);
        }
        if (!first)
            std::cout << ',';
        first = false;
        std::cout << (result ? result.value().setting().id : -1);
    }
    std::cout << "]}\n";
}
