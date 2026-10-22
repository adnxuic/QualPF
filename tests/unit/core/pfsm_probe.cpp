// Text-only numerical probe for independent Python/SciPy comparison.
#include "qualpf/profile/pfsm.h"
#include "pfsm_work_report.h"
#include <iomanip>
#include <iostream>
using namespace qualpf;
using namespace qualpf::profile;
int main(int argc, char **argv) {
    PfsmWorkReport workReport;
    const bool cache = argc == 2 && std::string_view(argv[1]) == "--template-cache";
    if (argc > 1 && !cache)
        return 2;
    const ExecutionContext context{{}, NumericalPolicy::explicit_float64, {}};
    std::size_t n = 0, m = 0;
    double width = 0;
    if (!(std::cin >> n >> m >> width) || n > 1000000 || m > 64)
        return 2;
    ProfileFitInput input;
    auto data = std::make_shared<ProfileObservations>();
    for (std::size_t i = 0; i < n; ++i) {
        double x, y, b, w;
        if (!(std::cin >> x >> y >> b >> w))
            return 2;
        data->coordinates.push_back(x);
        data->observed.push_back(y);
        data->background.push_back(b);
        data->weights.push_back(w);
    }
    input.observations = data;
    for (std::size_t j = 0; j < m; ++j) {
        double lower, upper, initial;
        std::size_t count;
        if (!(std::cin >> lower >> upper >> initial >> count) || count > 100000)
            return 2;
        ProfileExpansionInput expansion;
        expansion.fwhm_degrees = width;
        expansion.coordinates = ProfileExpansionInput::Coordinates::two_theta_degrees;
        if (cache && n)
            expansion.interpolation_window = {data->coordinates.front() - upper,
                                              data->coordinates.back() - lower};
        for (std::size_t k = 0; k < count; ++k) {
            double q, y;
            if (!(std::cin >> q >> y))
                return 2;
            expansion.lines.push_back({k, q, y});
        }
        auto p = IntermediateLorentzianExpander{}.prepare(expansion, context);
        if (!p) {
            std::cerr << p.error().message;
            return 1;
        }
        input.phases.push_back({std::to_string(j), p.value(), lower, upper, initial});
    }
    auto result = NnlsTrfProfileFitter{}.fit(input, context);
    if (!result) {
        std::cerr << result.error().field << ": " << result.error().message;
        return 1;
    }
    const auto &r = result.value();
    std::cout << std::setprecision(17) << r.q << ' ' << r.rwp_net << ' ' << r.diagnostics.kkt_violation << ' '
              << int(r.termination) << '\n';
    for (std::size_t j = 0; j < m; ++j)
        std::cout << r.parameters.scales[j] << ' ' << r.parameters.shifts[j] << '\n';
    return 0;
}
