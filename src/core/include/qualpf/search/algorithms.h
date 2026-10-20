#pragma once
#include "qualpf/algorithms/identity.h"
#include "qualpf/search/classic.h"
#include "qualpf/domain/analysis_spectrum.h"

namespace qualpf::search {
inline AlgorithmIdentity classic_match_contract() { return {"qualpf.classic.associations.binary32", 1}; }
// Owned association evidence, with no final FOM. Float storage is intentional:
// widening/reducing these buffers would change the approved mixed precision.
struct ClassicMatch {
    AlgorithmIdentity contract = classic_match_contract();
    ScoreDisposition disposition = ScoreDisposition::no_reflections_in_range;
    std::vector<std::size_t> reflection_indices;
    std::vector<std::optional<std::size_t>> experimental_indices;
    std::vector<float> sample_x, sample_y, card_x, card_y, merged;
    std::vector<bool> associated;
    std::size_t associated_reflections = 0, associated_peaks = 0;
    float position_difference_sum = 0;
};
struct ClassicMatchInput {
    const AnalysisSpectrum &spectrum;
    const PeakSet &experimental_peaks;
    const ReferenceCard &candidate;
    const SearchParameters &parameters;
};
class IClassicMatcher {
  public:
    virtual ~IClassicMatcher() = default;
    virtual AlgorithmIdentity identity() const = 0;
    virtual AlgorithmIdentity produces() const = 0;
    virtual Result<ClassicMatch> match(const ClassicMatchInput &, const ExecutionContext &) const = 0;
};
class ClassicMatcher final : public IClassicMatcher {
  public:
    AlgorithmIdentity identity() const override { return {"qualpf.classic.matcher.cpp", 2}; }
    AlgorithmIdentity produces() const override { return classic_match_contract(); }
    Result<ClassicMatch> match(const ClassicMatchInput &, const ExecutionContext &) const override;
};
// Fixed Classical operations, never assembly/replacement points.
Result<ScoreTrace> calculate_classic_fom(const ScoreInput &, const ClassicMatch &, const ExecutionContext &);
Result<bool> passes_stored_strongest(std::span<const double> d, std::span<const double> delta_d,
    std::span<const DSpacingAngstrom> stored_top, const ExecutionContext &);
Result<bool> accepts_record_quality(std::string_view quality, bool include_deleted, const ExecutionContext &);
Result<ResidualResult> update_classic_residual(const PeakSet &, const ReferenceCard &, double,
    const SearchParameters &, const ExecutionContext &);
Result<ScoreTrace> score_classic(const ClassicMatchInput &, const IClassicMatcher &,
                                const ExecutionContext &);
} // namespace qualpf::search
