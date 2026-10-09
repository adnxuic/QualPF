#include "../../src/core/preprocessing/legacy_reduction.h"
#include <iomanip>
#include <iostream>

int main() {
    int kind;
    double a, b, c;
    std::cout << std::setprecision(17);
    while (std::cin >> kind >> a >> b >> c) {
        if (kind == 32)
            std::cout << qualpf::preprocessing::detail::reference_fma(float(a), float(b), float(c));
        else
            std::cout << qualpf::preprocessing::detail::cholesky_multiply_add(a, b, c);
        std::cout << '\n';
    }
}
