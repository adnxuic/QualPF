#include "qualpf/search/rir.h"
#include "qualpf/domain/radiation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::search;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) throw std::runtime_error("Profile RIR contract at line " + std::to_string(at.line()));
}
static std::vector<double> run(std::vector<ProfileRirInput> phases) {
    auto value = quantify_profile_rir(phases); check(bool(value)); return std::move(value).value();
}
static void near(double a, double b) { check(std::abs(a-b) < 1e-12); }
int main() try {
    // Prescribed amounts 250:750, with unequal RIR and card normalizations.
    std::vector<ProfileRirInput> phases{{5,100,2}, {3,1000,4}};
    auto fractions = run(phases); near(fractions[0],25); near(fractions[1],75);
    near(run({{1,1000,1}, {1,1000,3}})[0],75); // Equal scales do not mean equal mass.
    for (double factor : {.001, 10., 1000.}) {
        auto scaled = phases;
        for (auto &p : scaled) p.scale *= factor;
        auto f = run(scaled); near(f[0],25); near(f[1],75);
        scaled = phases;
        scaled[0].reference_maximum *= factor; scaled[0].scale /= factor;
        f = run(scaled); near(f[0],25); near(f[1],75);
    }
    std::reverse(phases.begin(),phases.end()); near(run(phases)[0],75);
    near(run({{5,100,2}})[0],100);
    fractions = run({{0,100,2}, {3,1000,4}}); near(fractions[0],0); near(fractions[1],100);
    // Integral of peak-top IL is H/sqrt(2^(2/3)-1). A common H multiplies
    // both strongest-line areas and cancels in normalized RIR mass fractions.
    for (double h : {.01,.2,1.}) {
        const double integral = h/std::sqrt(std::pow(2.,2./3)-1);
        const double a = 5*100*integral/2, b = 3*1000*integral/4;
        near(100*a/(a+b),25); near(100*b/(a+b),75);
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (auto bad : std::vector<ProfileRirInput>{{1,100,{}},{1,100,0},{1,100,-1},
            {1,100,nan},{1,100,inf},{1,0,1},{1,nan,1},{-1,100,1},{nan,100,1},{inf,100,1}}) {
        const std::vector<ProfileRirInput> mixed{{5,100,2},bad};
        auto invalid = quantify_profile_rir(mixed);
        check(!invalid && invalid.error().field == "quantification.phase[1]");
    }
    check(!quantify_profile_rir(std::vector<ProfileRirInput>{{0,100,2}}));
    check(!quantify_profile_rir(std::vector<ProfileRirInput>{{1e308,100,2}}));
    check(!quantify_profile_rir(std::vector<ProfileRirInput>{{1e-300,1e-300,2}}));
    check(!quantify_profile_rir(std::vector<ProfileRirInput>{}));
    auto maximum = rir_reference_maximum(std::vector<double>{50,100,1}); check(maximum && maximum.value()==100);
    check(rir_reference_maximum({}).value()==0);
    check(!rir_reference_maximum(std::vector<double>{1,nan}));
    check(!rir_reference_maximum(std::vector<double>{1,-1}));
    Radiation copper; copper.kind = RadiationKind::x_ray;
    copper.lines = {{WavelengthAngstrom{radiation::CU_KA1_WAVE},2}, {WavelengthAngstrom{radiation::CU_KA2_WAVE},1}};
    check(profile_rir_supports_radiation(copper));
    copper.lines = {{WavelengthAngstrom{1.5406},1}}; check(profile_rir_supports_radiation(copper));
    copper.kind = RadiationKind::neutron; check(!profile_rir_supports_radiation(copper));
    copper.kind = RadiationKind::x_ray; copper.lines[0].wavelength = WavelengthAngstrom{.7107};
    check(!profile_rir_supports_radiation(copper));
    std::cout << "Profile/RIR analytic and invariance contracts passed\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
