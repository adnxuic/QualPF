#pragma once
#include "qualpf/profile/pfsm_integral_screen.h"

namespace qualpf::profile {
// Frozen bounded-old-shift v1; formal fitting never consumes these parameters.
struct PfsmRelaxedOptions {
    unsigned version = 1;
    double radius_widths = 1.5, maximum_span_fraction = .5;
    std::size_t old_coordinates = 2, bank_positions = 257, starts = 2, stages = 4;
    std::size_t maximum_evaluations = 205, candidate_cache = 32, budget = 32;
    friend bool operator==(const PfsmRelaxedOptions &, const PfsmRelaxedOptions &) = default;
};
struct PfsmRelaxedBank {
    std::vector<double> shifts;
    std::vector<std::vector<double>> columns;
};
struct PfsmRelaxedRound {
    std::shared_ptr<const PfsmIntegralData> data;
    PfsmRelaxedOptions options;
    std::vector<PfsmRelaxedBank> banks;
    std::vector<double> old_shifts;
    ProfileFitOptions fit;
    double radius = 0, lower_shift = 0, upper_shift = 0;
    std::size_t buffer_bytes = 0, bank_columns = 0;
};
Status validatePfsmRelaxedOptions(const PfsmRelaxedOptions &);
Result<PfsmRelaxedRound> preparePfsmRelaxedRound(std::shared_ptr<const PfsmIntegralData>,
    std::span<const ProfileExpansionInput>, std::span<const double>, double lower, double upper,
    const ProfileFitOptions &, const PfsmRelaxedOptions &, const ExecutionContext &);
Result<PfsmRelaxedEvidence> evaluatePfsmRelaxedCandidate(const PfsmRelaxedRound &,
    const ProfileExpansionInput &, const PfsmScreenEvidence &, const ExecutionContext &);
// Add Top-32/protection to an already selected integral-v1 union.
Result<PfsmScreenSelection> selectPfsmRelaxedCandidates(std::span<PfsmScreenEvidence>,
    const PfsmRelaxedOptions &, const ExecutionContext &);
} // namespace qualpf::profile
