#include "qualpf/numerics/runtime.h"
#include <Eigen/Core>
#include <mutex>
#include <optional>

namespace qualpf::numerics {
Status initializeLinearAlgebraCacheSizes(LinearAlgebraCacheSizes sizes) {
    if (sizes.l1 <= 0 || sizes.l2 < sizes.l1 || sizes.l3 < sizes.l2)
        return Status{Error{ErrorCode::invalid_input, "numerics.cache_sizes",
                            "Positive nondecreasing L1/L2/L3 cache sizes are required"}};
    static std::mutex mutex;
    static std::optional<LinearAlgebraCacheSizes> configured;
    const std::lock_guard lock(mutex);
    if (configured && *configured != sizes)
        return Status{Error{ErrorCode::invalid_input, "numerics.cache_sizes",
                            "Linear algebra cache sizes were already configured"}};
    if (!configured) {
        Eigen::setCpuCacheSizes(sizes.l1, sizes.l2, sizes.l3);
        configured = sizes;
    }
    return Status{std::monostate{}};
}
} // namespace qualpf::numerics
