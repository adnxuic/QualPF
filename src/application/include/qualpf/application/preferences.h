#pragma once
#include <cstddef>
#include <string>
#include <vector>
namespace qualpf::application {
// Opaque UI values; Application does not interpret Qt metatypes or serialize them.
using PreferencePayload = std::vector<std::byte>;
class PreferenceStore {
  public:
    virtual ~PreferenceStore() = default;
    virtual PreferencePayload read(const std::string &, const PreferencePayload &fallback) const = 0;
    virtual void write(const std::string &, const PreferencePayload &) = 0;
    virtual bool contains(const std::string &) const = 0;
    virtual void remove(const std::string &) = 0;
    virtual std::string fileName() const = 0;
};
} // namespace qualpf::application
