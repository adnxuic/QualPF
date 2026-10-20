#include "qualpf/preprocessing/algorithms.h"

namespace qualpf::preprocessing {
Result<BackgroundResult> CppBackgroundEstimator::estimate(const Pattern &pattern, const BackgroundOptions &options,
    WavelengthAngstrom wavelength, std::optional<std::span<const BackgroundPoint>> points,
    const ExecutionContext &context) const {
    if (context.cancellation.stop_requested())
        return Result<BackgroundResult>{Error{ErrorCode::cancelled, "preprocessing", "Preprocessing canceled"}};
    return points ? background_from_points(pattern, options, wavelength, *points)
                  : background(pattern, options, wavelength);
}
} // namespace qualpf::preprocessing
