#pragma once
#include "qualpf/domain/analysis_spectrum.h"

namespace qualpf::preprocessing::detail {
// Only numerical adapters consume this private prepared buffer. Public algorithm
// ports always receive AnalysisSpectrum, including its independent background.
Result<Pattern> prepare_analysis_signal(const AnalysisSpectrum &);
}
