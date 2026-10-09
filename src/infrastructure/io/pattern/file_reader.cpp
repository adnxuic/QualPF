#include "file_reader.h"
#include <fstream>
namespace qualpf::io {
Result<PatternRead> read_pattern(const std::filesystem::path &path) {
    std::ifstream input(path);
    if (!input)
        return Result<PatternRead>{Error{ErrorCode::missing_data, "pattern.file", "Cannot open pattern file",
                                         "pattern_read_failed"}};
    const auto utf8 = path.u8string();
    const auto extension = path.extension().u8string();
    return read_pattern(input, std::string(extension.begin(), extension.end()),
                        std::string(utf8.begin(), utf8.end()));
}
} // namespace qualpf::io
