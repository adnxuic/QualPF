#pragma once

#include "qualpf/domain/search.h"

#include <memory>
#include <stop_token>
#include <string>

namespace qualpf {

struct ExecutionContext {
    std::stop_token cancellation;
    NumericalPolicy numerical_policy = NumericalPolicy::unspecified; // Must be resolved before execution.
    std::string reference_data_revision;
};

struct BackgroundRequest {
    enum class Model { unspecified, none, chebyshev, polynomial, cosine_fourier, spline, bezier, bruckner };
    Model model = Model::unspecified;
    bool automatic = false;
    std::size_t coefficient_count = 0;
    std::size_t filter_iterations = 0;
    std::size_t filter_window_points = 0;
    std::optional<AngularRange> filter_range;
    std::vector<double> coefficients;
    std::vector<TwoThetaDegrees> control_x;
    std::vector<double> control_y;
    // Algorithm-specific settings remain to be frozen from reference runs.
    std::string parameter_contract_revision;
};
struct SmoothingRequest {
    enum class Method { unspecified, savitzky_golay, moving_average };
    Method method = Method::unspecified;
    std::size_t window_points = 0;
    std::size_t polynomial_order = 0;
};
struct PeakSearchRequest {
    AngularRange data_range;
    AngularRange search_range;
    double threshold = 0.0;
    int sensitivity = 0;
    std::size_t maximum_peaks = 0;
    bool append = false;
};

// No implementation in phase 3. Returned values own their memory; implementations
// may not retain input references or access hidden application/Fortran state.
class IScoringBackend {
public:
    virtual ~IScoringBackend() = default;
    virtual Result<ScoreValues> score(const ScoreInput&, const ExecutionContext&) = 0;
};
class IPeakSearchBackend {
public:
    virtual ~IPeakSearchBackend() = default;
    virtual Result<PeakSet> find(const Pattern&, const PeakSet* existing,
                                const PeakSearchRequest&, const ExecutionContext&) = 0;
};
class IPreprocessingBackend {
public:
    virtual ~IPreprocessingBackend() = default;
    virtual Result<Pattern> background(const Pattern&, const BackgroundRequest&, const ExecutionContext&) = 0;
    virtual Result<Pattern> smooth(const Pattern&, const SmoothingRequest&, const ExecutionContext&) = 0;
    virtual Result<Pattern> crop(const Pattern&, AngularRange, const ExecutionContext&) = 0;
    virtual Result<Pattern> strip_kalpha2(const Pattern&, const ExecutionContext&) = 0;
};

// The composition root supplies backends per workflow/session. Null means not
// implemented, not permission to fall back to a process-global Fortran function.
struct MigrationBackends {
    std::unique_ptr<IScoringBackend> scoring;
    std::unique_ptr<IPeakSearchBackend> peak_search;
    std::unique_ptr<IPreprocessingBackend> preprocessing;
};

} // namespace qualpf
