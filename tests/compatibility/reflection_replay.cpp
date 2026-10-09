#include "qualpf/crystallography/reflection.h"
#include <iomanip>
#include <iostream>
using namespace qualpf;
using namespace qualpf::crystallography;
void hkl(Hkl h) { std::cout << '[' << h.h << ',' << h.k << ',' << h.l << ']'; }
void members(std::span<const Hkl> values) {
    std::cout << '[';
    bool first = true;
    for (auto h : values) {
        if (!first)
            std::cout << ',';
        first = false;
        hkl(h);
    }
    std::cout << ']';
}
int main() {
    std::cout << std::setprecision(17) << '[';
    int id, number, setting, np, ne;
    bool first_case = true;
    while (std::cin >> id >> number >> setting >> np >> ne) {
        UnitCellParameters p;
        double wave;
        for (auto &v : p.lengths_angstrom)
            std::cin >> v;
        for (auto &v : p.angles_degrees)
            std::cin >> v;
        std::cin >> wave;
        auto cell = UnitCell::create(p);
        auto group = SpaceGroupRegistry{}.by_number(number, setting);
        if (!cell || !group)
            return 2;
        if (!first_case)
            std::cout << ',';
        first_case = false;
        std::cout << "{\"id\":" << id << ",\"probes\":[";
        for (int i = 0; i < np; ++i) {
            Hkl h;
            std::cin >> h.h >> h.k >> h.l;
            if (i)
                std::cout << ',';
            std::cout << "{\"hkl\":";
            hkl(h);
            auto orbit = reflection_class(h, group.value(), FriedelPolicy::merge);
            auto single = reflection_class(h, group.value(), FriedelPolicy::separate);
            auto absent = systematically_absent(h, group.value());
            if (!orbit || !single || !absent) {
                std::cout << ",\"error\":\"" << orbit.error().reason << "\"}";
                continue;
            }
            auto d = cell.value().d_spacing(h);
            auto angle = two_theta(d.value(), WavelengthAngstrom{wave});
            if (!angle)
                return 3;
            std::cout << ",\"canonical\":";
            hkl(orbit.value().canonical());
            std::cout << ",\"multiplicity\":" << orbit.value().multiplicity()
                      << ",\"absent\":" << int(absent.value()) << ",\"d\":" << d.value().value
                      << ",\"two_theta\":" << angle.value().value << ",\"orbit\":";
            members(single.value().members());
            std::cout << ",\"powder_orbit\":";
            members(orbit.value().members());
            std::cout << ",\"images\":[";
            bool first = true;
            for (auto op : group.value().operations()) {
                if (!first)
                    std::cout << ',';
                first = false;
                hkl(reciprocal_image(h, op).value());
            }
            std::cout << "]}";
        }
        std::cout << "],\"enumerations\":[";
        for (int i = 0; i < ne; ++i) {
            int mode;
            double low, high;
            std::cin >> mode >> low >> high;
            ReflectionGenerationOptions options;
            options.wavelength = WavelengthAngstrom{wave};
            if (mode == 0)
                options.minimum_d = DSpacingAngstrom{low};
            else {
                options.minimum_d = DSpacingAngstrom{wave / 2};
                options.minimum_two_theta = TwoThetaDegrees{low};
                options.maximum_two_theta = TwoThetaDegrees{high};
            }
            if (i)
                std::cout << ',';
            auto output = generate_reflections(cell.value(), group.value(), options);
            if (!output) {
                std::cout << "{\"error\":\"" << output.error().reason << "\"}";
                continue;
            }
            std::cout << '[';
            bool first = true;
            for (const auto &r : output.value().reflections()) {
                if (!first)
                    std::cout << ',';
                first = false;
                std::cout << "{\"hkl\":";
                hkl(r.hkl);
                std::cout << ",\"multiplicity\":" << r.multiplicity << ",\"d\":" << r.d.value
                          << ",\"two_theta\":" << r.two_theta.value << '}';
            }
            std::cout << ']';
        }
        std::cout << "]}";
    }
    std::cout << "]\n";
}
