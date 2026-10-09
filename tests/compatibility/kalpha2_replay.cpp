#include "qualpf/preprocessing/kalpha2.h"
#include <cstdlib>
#include <iomanip>
#include <iostream>
int main() {
    int n, mode, repeats, rad, seeded, background, flag;
    double wave;
    if (!(std::cin >> n >> mode >> repeats >> rad >> wave >> seeded >> background >> flag))
        return 2;
    qualpf::Pattern p;
    if (rad >= 0 && rad <= 2)
        p.radiation.kind = qualpf::RadiationKind(rad);
    p.radiation.lines.push_back({qualpf::WavelengthAngstrom{wave}, 1});
    if (flag != -1)
        p.processing.kalpha2_subtracted = flag != 0;
    for (int i = 0; i < n; ++i) {
        std::string x, y;
        std::cin >> x >> y;
        p.two_theta.push_back(qualpf::TwoThetaDegrees{std::strtod(x.c_str(), nullptr)});
        p.observed.push_back(std::strtod(y.c_str(), nullptr));
    }
    for (int i = 0; i < repeats; ++i) {
        auto r = qualpf::preprocessing::strip_kalpha2(p);
        if (!r) {
            std::cout << "ERROR " << r.error().reason << '\n';
            return 0;
        }
        p.observed = std::move(r).value();
        p.processing.kalpha2_subtracted = true;
    }
    std::cout << "OK\n" << std::setprecision(17);
    for (double y : p.observed)
        std::cout << y << '\n';
}
