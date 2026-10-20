#include "qualpf/numerics/linear_algebra.h"
#include "qualpf/numerics/runtime.h"
#include <stdexcept>
using namespace qualpf::numerics;
static void check(bool ok) {
    if (!ok)
        throw std::runtime_error("Linear algebra startup configuration contract");
}
int main() {
    check(!initializeLinearAlgebraCacheSizes({0, 0, 0}));
    check(!initializeLinearAlgebraCacheSizes({49152, 32768, 25165824}));
    check(bool(initializeLinearAlgebraCacheSizes({49152, 1310720, 25165824})));
    check(bool(initializeLinearAlgebraCacheSizes({49152, 1310720, 25165824})));
    check(!initializeLinearAlgebraCacheSizes({32768, 2097152, 25165824}));
    // Exact integer products cross the backend's blocked reduction sizes.
    Matrix a{2, 5000, std::vector<double>(10000, 1)};
    for (std::size_t i = 5000; i < a.values.size(); ++i)
        a.values[i] = 2;
    Matrix b{5000, 2, std::vector<double>(10000)};
    for (std::size_t i = 0; i < 5000; ++i) {
        b.values[2 * i] = 3;
        b.values[2 * i + 1] = 4;
    }
    const auto result = multiply(a.view(), b.view());
    check(bool(result));
    check(result.value().values == std::vector<double>{15000, 20000, 30000, 40000});
}
