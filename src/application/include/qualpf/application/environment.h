#pragma once
#include <string>
namespace qualpf::application {
class EnvironmentReader {
public:
    virtual ~EnvironmentReader() = default;
    virtual std::string environmentValue(const std::string &) const = 0;
};
} // namespace qualpf::application
