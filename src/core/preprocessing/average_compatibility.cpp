#include "average_compatibility.h"

namespace qualpf::preprocessing::detail {
Vector average_compatibility(const Vector &input, int width) {
    auto result = average(input, width);
    if (width == 0 && !input.empty())
        result.back() = input.back();
    return result;
}
} // namespace qualpf::preprocessing::detail
