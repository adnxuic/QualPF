#pragma once
#include "qualpf/domain/pattern.h"
#include "qualpf/domain/result.h"
#include <utility>

namespace qualpf {
// The Classical search preparation retains its positive/normalized numerical
// convention. This describes private computation, never stored observations.
enum class AnalysisPurpose { peak_measurement, classical_search };

// An owning, checked input. Its observed samples have never had this background
// removed. Source/revision admission remains the Application transaction's job.
class AnalysisSpectrum {
  public:
    const Pattern &pattern() const noexcept { return pattern_; }
    AnalysisPurpose purpose() const noexcept { return purpose_; }
    static Result<AnalysisSpectrum> create(Pattern, AnalysisPurpose = AnalysisPurpose::peak_measurement);
  private:
    AnalysisSpectrum(Pattern p, AnalysisPurpose purpose) : pattern_(std::move(p)), purpose_(purpose) {}
    Pattern pattern_;
    AnalysisPurpose purpose_;
};
} // namespace qualpf
