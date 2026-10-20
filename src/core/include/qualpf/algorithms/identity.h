#pragma once
#include <cstdint>
#include <string>

namespace qualpf {
// Stable implementation ID and typed interface/parameter contract version.
// IDs are resolved by explicit factories, never by a process-global registry.
struct AlgorithmIdentity {
    std::string id;
    std::uint32_t version = 1;
    friend bool operator==(const AlgorithmIdentity &, const AlgorithmIdentity &) = default;
};
} // namespace qualpf
