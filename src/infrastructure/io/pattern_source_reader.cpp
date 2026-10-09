#include "pattern_source_reader.h"
#include "infrastructure/io/pattern/file_reader.h"
#include <filesystem>
namespace qualpf::infrastructure {
Result<io::PatternRead> PatternSourceReader::readDetailed(const std::string &identity) const {
    return io::read_pattern(std::filesystem::u8path(identity));
}
Result<Pattern> PatternSourceReader::read(const std::string &identity) const {
    auto loaded = io::read_pattern(std::filesystem::u8path(identity));
    if (!loaded)
        return Result<Pattern>{loaded.error()};
    return Result<Pattern>{std::move(loaded).value().pattern};
}
} // namespace qualpf::infrastructure
