#include "pfsm_simd.h"
#if defined(QUALPF_PFSM_AVX2) && defined(_MSC_VER)
#include <intrin.h>
#endif

namespace qualpf::profile::detail {
bool avx2Available() noexcept {
#if defined(QUALPF_PFSM_AVX2) && defined(_MSC_VER) && defined(_M_X64)
    static const bool available = [] {
        int info[4];
        __cpuidex(info, 0, 0);
        if (info[0] < 7) return false;
        __cpuidex(info, 1, 0);
        // AVX + OSXSAVE, then OS-enabled XMM/YMM state. XGETBV is guarded.
        constexpr int required = (1 << 28) | (1 << 27);
        if ((info[2] & required) != required || (_xgetbv(0) & 6) != 6) return false;
        __cpuidex(info, 7, 0);
        return (info[1] & (1 << 5)) != 0;
    }();
    return available;
#elif defined(QUALPF_PFSM_AVX2) && (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
    // GCC's runtime CPU table includes OS support; ordinary calls run after its
    // high-priority initializer. No AVX instructions may enter this source file.
    static const bool available = __builtin_cpu_supports("avx2") != 0;
    return available;
#else
    return false;
#endif
}
#if !defined(QUALPF_PFSM_AVX2)
SimdResult ilAvx2(std::span<const double>, std::span<const ProfileLine>, double, double,
                  std::span<double>, std::stop_token) { return SimdResult::fallback; }
SimdResult cachedIlAvx2(std::span<const double>, double, double, double,
                        std::span<const IlCoefficients>, std::span<double>, std::stop_token) {
    return SimdResult::fallback;
}
#endif
} // namespace qualpf::profile::detail
