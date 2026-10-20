#pragma once
#include "qualpf/domain/result.h"
#include <cstddef>

namespace qualpf::numerics {
struct LinearAlgebraCacheSizes {
    std::ptrdiff_t l1 = 0, l2 = 0, l3 = 0;
    bool operator==(const LinearAlgebraCacheSizes &) const = default;
};
// Optional process-start configuration, before any numerical work or workers.
// Cache blocking affects reduction order. Omission retains Eigen's automatic
// CPU detection. Repeating identical values is legal; changing them is refused.
// This configures the private backend shared by Core dense algebra and PFSM.
Status initializeLinearAlgebraCacheSizes(LinearAlgebraCacheSizes);
} // namespace qualpf::numerics
