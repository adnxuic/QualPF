#include "qualpf/application/presentation/quantification.h"
#include <algorithm>
namespace qualpf::application {
std::vector<double> quantify(std::span<const QuantificationInput> phases) {
    std::vector<double> rap(phases.size(), 0.0);
    bool valid = true;
    for (std::size_t i = 0; i < phases.size(); ++i) {
        if (!phases[i].calculated) {
            valid = false;
            break;
        }
        if (!phases[i].rir || *phases[i].rir <= 0.0) {
            valid = false;
            break;
        }
        double maxI = 0.0;
        for (double value : phases[i].intensity)
            maxI = std::max(maxI, value * phases[i].scale);
        rap[i] = maxI / *phases[i].rir;
    }
    double sum = 0.0;
    for (double r : rap)
        sum += r;
    std::vector<double> quant(phases.size(), 0.0);
    if (valid && sum > 0.0)
        for (std::size_t i = 0; i < phases.size(); ++i)
            quant[i] = 100.0 * rap[i] / sum;
    return valid && sum > 0.0 ? quant : std::vector<double>{};
}
} // namespace qualpf::application
