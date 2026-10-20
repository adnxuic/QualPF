#pragma once
#include "qualpf/profile/pfsm.h"
#include <algorithm>
#include <chrono>
#include <cmath>

namespace qualpf::application::pfsm_detail {
struct Failure {
    Error error;
};
[[noreturn]] inline void fail(const char *field, const char *message,
                              ErrorCode code = ErrorCode::invalid_input,
                              const char *reason = "invalid_pfsm_input") {
    throw Failure{{code, field, message, reason}};
}
inline void checkpoint(const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        fail("pfsm", "PFSM canceled", ErrorCode::cancelled, "cancelled");
}
template <class T> T take(Result<T> result) {
    if (!result)
        throw Failure{result.error()};
    return std::move(result).value();
}
using Clock = std::chrono::steady_clock;
inline double elapsed(Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}
inline profile::ProfileFitResult checkedFit(Result<profile::ProfileFitResult> result,
                                            const profile::ProfileFitInput &input, bool requireExact = true) {
    auto value = take(std::move(result));
    take(profile::validateProfileFitResult(value, input, requireExact));
    return value;
}
inline profile::ProfileScanResult checkedScan(Result<profile::ProfileScanResult> result, std::size_t count) {
    auto value = take(std::move(result));
    if (value.q.size() != count ||
        std::any_of(value.q.begin(), value.q.end(), [](double q) { return !std::isfinite(q) || q < 0; }))
        fail("pfsm.algorithm.output", "Invalid scan objective payload", ErrorCode::invalid_input,
             "invalid_algorithm_output");
    return value;
}
} // namespace qualpf::application::pfsm_detail
