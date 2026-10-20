#pragma once
#include "qualpf/preprocessing/geometric_fwhm.h"
#include <map>

namespace qualpf::preprocessing::geometric_detail {
struct Noise {
    FwhmNoiseModel model = FwhmNoiseModel::unknown;
    std::vector<double> variance;
    std::map<std::pair<std::size_t, std::size_t>, double> upper;
    bool known() const { return model != FwhmNoiseModel::unknown; }
    double sigma(std::size_t first, std::size_t last) const;
    double covariance(std::size_t i, std::size_t j) const;
};
Result<Noise> prepare_noise(const GeometricFwhmInput &, const std::vector<bool> &, const ExecutionContext &);
Status propagate(const GeometricFwhmInput &, const std::vector<double> &, const Noise &, PeakFwhmDiagnostic &);
} // namespace qualpf::preprocessing::geometric_detail
