#pragma once

#include "qualpf/domain/pattern.h"
#include "qualpf/domain/peaks.h"
#include "qualpf/domain/reference_card.h"
#include "qualpf/domain/result.h"

#include <cstddef>
#include <string>
#include <vector>

namespace qualpf {

struct SearchParameters {
    // No algorithm defaults: the application must resolve settings explicitly.
    AngularWidthDegrees delta_two_theta;
    double weight_position = 0.0;
    double weight_intensity = 0.0;
    double weight_phases = 0.0;
    double minimum_score = 0.0;
    std::size_t maximum_entries = 0; // Zero means zero retained rows, not "unlimited".
    bool filter_strongest = false;
    std::size_t strongest_count = 0;
    bool include_deleted_cards = false;
    bool residual_search = false;
};

// All synchronous borrows. In particular, full observed x/y and calculation_range
// must be supplied: experimental peaks alone cannot replace the legacy FOM input.
struct ScoreInput {
    const Pattern& pattern;
    const PeakSet& experimental_peaks;
    const ReferenceCard& candidate;
    const SearchParameters& parameters;
};
struct ScoreValues {
    double total = 0.0;
    double position = 0.0;
    double intensity = 0.0;
    double scale = 0.0;
};
struct CandidateScore {
    std::string database_id;
    std::string card_id;
    Result<ScoreValues> outcome; // Failure/cancelled is never a synthetic numeric score.
};
struct AcceptedPhase {
    std::string database_id;
    std::string card_id;
    double scale = 0.0;
    std::optional<double> rir;
    std::optional<double> fraction; // Fraction [0,1]; absent until actually calculated.
};
struct ComputationProvenance {
    std::string implementation_id;
    std::string implementation_revision;
    NumericalPolicy numerical_policy = NumericalPolicy::unspecified;
    std::string reference_data_revision;
};
struct SearchResults {
    DatasetId dataset;
    std::vector<CandidateScore> candidates; // Preserve backend order; no sorting policy here.
    std::vector<AcceptedPhase> accepted_phases;
    bool complete = false; // Partial/cancelled results are not silently marked complete.
    std::optional<ComputationProvenance> provenance;
};

} // namespace qualpf
