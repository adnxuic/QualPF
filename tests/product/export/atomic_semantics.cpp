#define main old_atomic_controls
#include "atomic_controls.cpp"
#undef main
#include <limits>
int main() {
    if (old_atomic_controls())
        return 1;
    try {
        for (int boundary = 0; boundary != 3; ++boundary) {
            exporting::PatternExportSnapshot s;
            s.x = {10, 20};
            s.observed = {1, 2};
            s.d_spacing_angstrom = {1, 2};
            s.wavelength_angstrom = 1.5405930280685425;
            s.range_min = 10;
            s.range_max = 20;
            if (boundary == 0)
                s.d_spacing_angstrom[0] = std::numeric_limits<double>::infinity();
            if (boundary == 1)
                s.d_spacing_angstrom[0] = -1;
            if (boundary == 2)
                s.wavelength_angstrom = 0;
            FakeTarget target;
            check(!write_pattern_export_to(target, s, qualpf::exporting::PatternFormat::powder_cif) && target.opens == 0 &&
                      target.writes == 0 && target.commits == 0 && target.target == "original",
                  "All three B2 candidates reject before opening the atomic target");
        }
        std::cout << "PASS B2 nonfinite d/nonpositive d/zero transported wave atomic controls\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
