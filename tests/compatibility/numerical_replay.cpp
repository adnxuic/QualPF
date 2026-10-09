// Shared explicit input protocol with numerical_capture.f90; no expected generation.
#include "qualpf/numerics/basic.h"
#include "qualpf/numerics/interpolation.h"
#include "qualpf/numerics/linear_algebra.h"
#include "qualpf/numerics/polynomial.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::numerics;
template<class T> T take(Result<T> r) {
    if (!r) throw std::runtime_error(r.error().field + ": " + r.error().message);
    return std::move(r).value();
}
void emit(double x) { std::cout << std::setprecision(17) << x << '\n'; }
int main() {
    try {
        int op, n, m;
        if (!(std::cin >> op >> n >> m) || n < 1 || n > 10000 || m < 1 || m > 10000)
            throw std::runtime_error("Invalid wire dimensions");
        std::vector<double> x(n), y(n);
        for (auto &v : x) std::cin >> v;
        for (auto &v : y) std::cin >> v;
        switch (op) {
        case 1: emit(take(sum(x))); emit(take(dot(x,y))); emit(take(norm(std::span(x).first(2)))); break;
        case 2:
            for (int i=0;i<m;++i) { double q; std::cin >> q; emit(double(take(nearest_index(x,q)))); }
            break;
        case 3: case 11: {
            double wave; std::cin >> wave;
            for (double q : x) emit(op == 3 ? take(to_d_spacing(TwoThetaDegrees{q},WavelengthAngstrom{wave})).value
                                          : take(to_two_theta(DSpacingAngstrom{q},WavelengthAngstrom{wave})).value);
            break;
        }
        case 4: case 10: { const auto s=take(statistics(x)); emit(s.mean); emit(s.variance); break; }
        case 5: emit(take(trapezoid(x,y,SinglePointIntegral::legacy_sample_value))); break;
        case 6: {
            double left,right; std::cin >> left >> right;
            SplineBoundary b;
            if (left < 0.99e30) b.first_derivative=left;
            if (right < 0.99e30) b.last_derivative=right;
            auto model=take(cubic_spline(x,y,b));
            for (int i=0;i<m;++i) { double q; std::cin >> q; emit(take(model.evaluate(q,OutOfRange::extrapolate))); }
            break;
        }
        case 7: case 8: {
            const int cols=op == 7 ? n : m;
            Matrix a{std::size_t(n),std::size_t(cols),std::vector<double>(n*cols)};
            for (auto &v:a.values) std::cin >> v;
            const auto s=take(op == 7 ? solve_linear(a.view(),y) : least_squares(a.view(),y));
            for (double v:s.x) emit(v);
            if (op == 8) emit(s.residual_norm);
            break;
        }
        case 9: {
            LeastSquaresOptions options{LeastSquaresMethod::svd,RankTolerance{1e-5},true};
            auto p=take(fit_polynomial(x,y,m-1,options));
            for (double v:p.coefficients) emit(v);
            emit(p.center); emit(p.scale); break;
        }
        default: throw std::runtime_error("Unknown operation");
        }
        if (!std::cin) throw std::runtime_error("Truncated input");
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
