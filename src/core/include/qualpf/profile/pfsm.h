#pragma once

#include "qualpf/algorithms/identity.h"
#include "qualpf/migration/backends.h"
#include <memory>
#include <span>

namespace qualpf::profile {

struct ProfileLine {
    std::uint64_t identity = 0;
    double position = 0;
    double intensity = 0;
};
struct ProfileExpansionInput {
    enum class Coordinates { d_spacing_angstrom, two_theta_degrees };
    Coordinates coordinates = Coordinates::d_spacing_angstrom;
    std::vector<ProfileLine> lines;
    Radiation radiation;
    double fwhm_degrees = 0;
    // Optional bounded cache of the unchanged IL function. Outside this window
    // evaluation remains direct. Bounds are unshifted 2theta coordinates.
    std::optional<std::pair<double, double>> interpolation_window;
};
// Shared validation and ordered radiation expansion; no grid or tail filtering.
Result<std::vector<ProfileLine>> expandProfileLines(const ProfileExpansionInput &, const ExecutionContext &);
struct ProfileSamples {
    std::vector<double> intensity;
    std::vector<double> shift_derivative;
};
// Immutable, owning, reentrant templates. All angles, widths and shifts are
// 2theta degrees.
class IProfileTemplate {
  public:
    virtual ~IProfileTemplate() = default;
    virtual double fwhmDegrees() const noexcept = 0;
    virtual Result<ProfileSamples> evaluate(std::span<const double> coordinates, double shift,
                                            bool derivative, const ExecutionContext &) const = 0;
    virtual bool usesApproximation() const noexcept { return false; }
    virtual Result<ProfileSamples> evaluateExact(std::span<const double> x, double shift, bool derivative,
                                                 const ExecutionContext &context) const {
        return evaluate(x, shift, derivative, context);
    }
};
using ProfileTemplate = std::shared_ptr<const IProfileTemplate>;
class IProfileExpander {
  public:
    virtual ~IProfileExpander() = default;
    virtual AlgorithmIdentity identity() const = 0;
    virtual Result<ProfileTemplate> prepare(const ProfileExpansionInput &,
                                            const ExecutionContext &) const = 0;
};
class IntermediateLorentzianExpander final : public IProfileExpander {
  public:
    AlgorithmIdentity identity() const override { return {"qualpf.profile.il.fixed", 1}; }
    Result<ProfileTemplate> prepare(const ProfileExpansionInput &, const ExecutionContext &) const override;
};

struct ProfileObservations {
    std::vector<double> coordinates, observed, background, weights;
};
struct ProfileFitPhase {
    std::string identity;
    ProfileTemplate profile;
    double lower_shift = -.5, upper_shift = .5, initial_shift = 0;
};
struct ProfileFitParameters {
    std::vector<double> scales, shifts;
};
struct ProfileFitOptions {
    double ftol = 1e-8, xtol = 1e-8, gtol = 1e-8;
    std::size_t maximum_evaluations = 0;     // 0 resolves to 100 * max(1, phase count), per start.
    std::size_t maximum_nnls_iterations = 0; // 0 resolves to max(100, 30*m*m).
};
enum class ProfileFitTermination {
    fixed_shifts,
    gradient,
    cost,
    step,
    cost_and_step,
    budget,
    no_progress,
    feasible_fallback
};
struct ProfileFitDiagnostics {
    std::size_t residual_evaluations = 0, difference_evaluations = 0, nnls_iterations = 0;
    std::size_t starts = 0, rank = 0;
    double kkt_violation = 0, optimality = 0;
    bool rank_deficient = false;
    bool kkt_checked = true;
    // With interpolation, optimality/termination describe the iterative model;
    // Q, scales and KKT from fit() are always verified using exact templates.
    bool interpolated_iterations = false, exact_final_verification = true;
    std::vector<bool> zero_templates, unidentifiable_shifts, at_bound;
};
struct ProfileFitResult {
    ProfileFitParameters parameters;
    std::vector<double> calculated, residual;
    double q = 0, denominator_net = 0, denominator_observed = 0, rwp_net = 0;
    std::optional<double> rwp_observed;
    ProfileFitTermination termination = ProfileFitTermination::fixed_shifts;
    ProfileFitDiagnostics diagnostics;
};
struct ProfileFitInput {
    std::shared_ptr<const ProfileObservations> observations;
    std::vector<ProfileFitPhase> phases;
    std::vector<std::vector<double>> starts; // Degrees, including fixed coordinates.
    std::optional<ProfileFitParameters> fallback;
    ProfileFitOptions options;
};
// Shared full-payload validation also used by the default scan adapter.
Status validateProfileFitResult(const ProfileFitResult &, const ProfileFitInput &, bool requireExact = true);
struct ProfileScanResult {
    std::vector<double> q; // Same order and original units as the supplied scan values.
    std::size_t residual_evaluations = 0, nnls_iterations = 0;
};
class IProfileFitter {
  public:
    virtual ~IProfileFitter() = default;
    virtual AlgorithmIdentity identity() const = 0;
    // Fixed-shift scan evaluation may use explicitly enabled interpolation.
    // Only fit() guarantees exact final verification and feasible fallbacks.
    virtual Result<ProfileFitResult> evaluateAt(const ProfileFitInput &, std::span<const double> shifts,
                                                const ExecutionContext &) const = 0;
    // Compatible replacement fitters need only implement evaluateAt()/fit().
    // The default adapter validates every full result before retaining its Q.
    virtual Result<ProfileScanResult> scanAt(const ProfileFitInput &, std::span<const double> fixedShifts,
                                             std::size_t phase, std::span<const double> values,
                                             const ExecutionContext &) const;
    virtual Result<ProfileFitResult> fit(const ProfileFitInput &, const ExecutionContext &) const = 0;
};
class NnlsTrfProfileFitter final : public IProfileFitter {
  public:
    AlgorithmIdentity identity() const override { return {"qualpf.profile.nnls-trf", 1}; }
    Result<ProfileFitResult> evaluateAt(const ProfileFitInput &, std::span<const double>,
                                        const ExecutionContext &) const override;
    Result<ProfileScanResult> scanAt(const ProfileFitInput &, std::span<const double>, std::size_t,
                                     std::span<const double>, const ExecutionContext &) const override;
    Result<ProfileFitResult> fit(const ProfileFitInput &, const ExecutionContext &) const override;
};
} // namespace qualpf::profile
