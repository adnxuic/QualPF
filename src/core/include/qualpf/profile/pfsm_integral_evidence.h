#pragma once
#include "qualpf/profile/pfsm.h"

namespace qualpf::profile {
// Baseline integral/old-shift evidence from 3fa1123a; no legacy screen API.
struct PfsmSupportEvidence {
    std::optional<double> score;
    double shift = 0, visible_fraction = 0, edge_fraction = 0;
    std::size_t visible_groups = 0;
    bool uncertain = true;
};
struct PfsmRelaxedPoint {
    double q = 0;
    std::vector<double> shifts, scales;
};
struct PfsmRelaxedEvidence {
    PfsmRelaxedPoint best, null;
    double fixed_q = 0, gain = 0, kkt = 0;
    std::vector<std::size_t> released;
    std::size_t evaluations = 0, iterations = 0, cache_columns = 0, cache_bytes = 0, buffer_bytes = 0;
    std::string uncertainty;
    bool complete = false, selected = false;
};
struct PfsmScreenEvidence {
    std::size_t original_index = 0;
    std::optional<double> gain, normalized_gain;
    double coarse_shift = 0;
    PfsmSupportEvidence original, residual;
    std::vector<std::string> uncertainty;
    std::size_t nnls_iterations = 0, scan_evaluations = 0;
    bool main_selected = false, original_selected = false, residual_selected = false;
    bool retained = false;
    // Integral backend only; legacy evidence retains its original meanings.
    std::optional<double> fine_gain, normalized_fine_gain;
    double fine_shift = 0;
    std::size_t coarse_evaluations = 0, fine_evaluations = 0, buffer_bytes = 0;
    bool coarse_selected = false, integral_complete = false;
    PfsmRelaxedEvidence relaxed;
};
struct PfsmScreenSelection {
    std::vector<std::size_t> indices;
    bool full_fallback = false;
    std::string fallback_reason;
};
} // namespace qualpf::profile
