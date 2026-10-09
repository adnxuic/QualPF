#pragma once
#include "qualpf/domain/pattern.h"
#include "qualpf/domain/result.h"

namespace qualpf::preprocessing {
// Fixed three-term Cu compatibility model. No resampling, peak detection or
// background estimation. Input ownership and all metadata remain with caller.
Status validate_kalpha2(const Pattern &pattern);
Result<std::vector<double>> strip_kalpha2(const Pattern &pattern);
} // namespace qualpf::preprocessing
