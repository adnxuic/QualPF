#pragma once
#include "qualpf/profile/pfsm_integral_evidence.h"

namespace qualpf::profile {
// Frozen v1 approximation; independent of the legacy sensitivity settings.
struct PfsmIntegralOptions {
    unsigned version = 1;
    std::size_t fine_bins = 2048, coarse_positions = 65, neighborhoods = 4, fine_positions = 8;
    std::size_t fine_budget = 64, coarse_budget = 16, original_budget = 16, residual_budget = 16;
    double tail_mass_epsilon = 1e-4;
    friend bool operator==(const PfsmIntegralOptions &, const PfsmIntegralOptions &) = default;
};
Status validatePfsmIntegralOptions(const PfsmIntegralOptions &);
struct PfsmIntegralOverlap {
    double lower = 0, upper = 0, density = 0;
    std::size_t sample = 0, bin = 0;
};
struct PfsmIntegralLayer {
    std::vector<double> weights, net;
    double denominator = 0;
};
struct PfsmIntegralData {
    std::shared_ptr<const ProfileObservations> original;
    PfsmIntegralOptions options;
    double width = 0, radius = 0;
    std::vector<double> edges;
    std::vector<PfsmIntegralOverlap> overlaps;
    std::vector<std::pair<double, double>> regions;
    PfsmIntegralLayer fine, coarse;
    std::size_t buffer_bytes = 0;
};
struct PfsmIntegralColumns {
    std::vector<double> fine, coarse;
};
// Unit-area IL mass between offsets. No peak sampling or window renormalization.
Result<double> pfsmIntegralMass(double lower, double upper, double width);
Result<std::shared_ptr<const PfsmIntegralData>> preparePfsmIntegralData(
    std::shared_ptr<const ProfileObservations>, std::span<const std::uint8_t> breakBefore,
    double width, const PfsmIntegralOptions &, const ExecutionContext &);
Result<PfsmIntegralColumns> integratePfsmScreenTemplate(const PfsmIntegralData &,
    std::span<const ProfileLine> expandedLines, double shift, const ExecutionContext &);
struct PfsmIntegralRound {
    std::shared_ptr<const PfsmIntegralData> data;
    std::vector<PfsmIntegralColumns> selected_columns;
    std::vector<double> support_positions, original_response, residual_response, norm_response;
    double coarse_baseline_q = 0, fine_baseline_q = 0, tau = 0;
    bool initial = true;
    std::size_t nnls_iterations = 0, buffer_bytes = 0;
};
Result<PfsmIntegralRound> preparePfsmIntegralRound(std::shared_ptr<const PfsmIntegralData>,
    std::span<const ProfileExpansionInput> selected, std::span<const double> shifts,
    std::span<const double> residual, const ProfileFitOptions &, const ExecutionContext &);
Result<PfsmScreenEvidence> evaluatePfsmIntegralCandidate(const PfsmIntegralRound &,
    const ProfileExpansionInput &, double lowerShift, double upperShift,
    const ProfileFitOptions &, const ExecutionContext &);
Result<PfsmScreenSelection> selectPfsmIntegralCandidates(std::span<PfsmScreenEvidence>,
    const PfsmIntegralOptions &, const ExecutionContext &);
} // namespace qualpf::profile
