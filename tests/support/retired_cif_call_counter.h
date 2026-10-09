#pragma once
#include <atomic>
#include <cstdint>

namespace qualpf::legacy {
// Monotonic instrumentation at the actual native CIF/card ABI forwarding call.
// It does not include table startup or unrelated structural consumers.
inline std::atomic<std::uint64_t> cif_native_call_counter{0};
inline std::uint64_t cif_native_calls() noexcept {
  return cif_native_call_counter.load(std::memory_order_relaxed);
}
} // namespace qualpf::legacy
