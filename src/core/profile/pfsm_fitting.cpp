#include "pfsm_internal.h"
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

namespace qualpf::profile::detail {
namespace {
unsigned observationDenormalMode() {
#if defined(__SSE2__) || defined(_M_X64)
    return _mm_getcsr() & 0x8040;
#else
    return 0;
#endif
}
PreparedProfileObservationsHandle prepareObservations(std::shared_ptr<const ProfileObservations> observations,
                                                      const ExecutionContext &context, bool frozen) {
    checkpoint(context);
    if (context.numerical_policy != NumericalPolicy::explicit_float64 || !observations)
        fail("pfsm.observations", "Owned observations and explicit binary64 policy are required");
    if (frozen)
        observations = std::make_shared<const ProfileObservations>(*observations);
    auto prepared = std::make_shared<PreparedProfileObservations>();
    prepared->observations = std::move(observations);
    prepared->frozen = frozen;
    prepared->denormals = observationDenormalMode();
    const auto &data = *prepared->observations;
    const auto n = data.coordinates.size();
    if (!n || data.observed.size() != n || data.background.size() != n || data.weights.size() != n)
        fail("pfsm.observations", "Observation arrays must be nonempty and aligned");
    countWork(&WorkCounters::observation_preparations);
    auto &z = prepared->normalized_net;
    auto &sqrt_weights = prepared->sqrt_weights;
    auto &net_denominator = prepared->net_denominator;
    auto &observed_denominator = prepared->observed_denominator;
    auto &normalization = prepared->normalization;
    z.resize(static_cast<Eigen::Index>(n));
    sqrt_weights.resize(static_cast<Eigen::Index>(n));
    Vector observed(static_cast<Eigen::Index>(n));
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(data.coordinates[i]) || (i && data.coordinates[i] <= data.coordinates[i - 1]) ||
            !std::isfinite(data.observed[i]) || !std::isfinite(data.background[i]) ||
            !std::isfinite(data.weights[i]) || data.weights[i] < 0)
            fail("pfsm.observations", "Invalid coordinates, intensities, background or weights");
        const auto k = static_cast<Eigen::Index>(i);
        sqrt_weights[k] = std::sqrt(data.weights[i]);
        z[k] = sqrt_weights[k] * (data.observed[i] - data.background[i]);
        observed[k] = sqrt_weights[k] * data.observed[i];
        net_denominator += z[k] * z[k];
        observed_denominator += observed[k] * observed[k];
    }
    // Use the same deterministic weighted-square reduction as final Q, so the
    // empty (background-only) model has Rwp_net exactly one.
    normalization = std::sqrt(net_denominator);
    if (!(normalization > 0) || !std::isfinite(net_denominator) || !std::isfinite(observed_denominator))
        fail("pfsm.denominator", "No finite positive weighted net signal", ErrorCode::missing_data);
    z /= normalization;
    return prepared;
}
PreparedProfileObservationsHandle observationsForFit(const ProfileFitInput &input, const ExecutionContext &context) {
    checkpoint(context);
    if (context.numerical_policy != NumericalPolicy::explicit_float64 || !input.observations)
        fail("pfsm.observations", "Owned observations and explicit binary64 policy are required");
    if (input.prepared_observations && input.prepared_observations->frozen &&
        input.prepared_observations->observations == input.observations &&
        input.prepared_observations->rounding == std::fegetround() &&
        input.prepared_observations->denormals == observationDenormalMode()) {
        countWork(&WorkCounters::observation_reuses);
        return input.prepared_observations;
    }
    return prepareObservations(input.observations, context, false);
}
}
ReducedProblem::ReducedProblem(const ProfileFitInput &in, const ExecutionContext &ctx)
    : prepared_(observationsForFit(in, ctx)), input(in), context(ctx),
      z(prepared_->normalized_net), sqrt_weights(prepared_->sqrt_weights),
      net_denominator(prepared_->net_denominator), observed_denominator(prepared_->observed_denominator),
      normalization(prepared_->normalization) {
    const auto n = input.observations->coordinates.size(), m = input.phases.size();
    for (std::size_t j = 0; j < m; ++j) {
        const auto &p = input.phases[j];
        if (!p.profile || !std::isfinite(p.lower_shift) || !std::isfinite(p.upper_shift) ||
            !std::isfinite(p.initial_shift) || p.lower_shift > p.upper_shift ||
            p.initial_shift < p.lower_shift || p.initial_shift > p.upper_shift)
            fail("pfsm.phase", "Missing template or invalid finite shift bounds/initial shift");
        const double h = p.profile->fwhmDegrees();
        if (!std::isfinite(h) || h <= 0 || (j && h != width))
            fail("pfsm.fwhm", "All templates must share one positive finite FWHM");
        width = h;
        approximated = approximated || p.profile->usesApproximation();
        if (p.lower_shift < p.upper_shift)
            free.push_back(j);
    }
    const auto &o = input.options;
    if (!std::isfinite(o.ftol) || !std::isfinite(o.xtol) || !std::isfinite(o.gtol) || o.ftol <= 0 ||
        o.xtol <= 0 || o.gtol <= 0)
        fail("pfsm.options", "Positive finite stopping tolerances are required");
    columns.resize(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(m));
    unweighted_columns.resize(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(m));
    linear_workspace.resize(static_cast<Eigen::Index>(n), static_cast<Eigen::Index>(m));
    last_shifts = Vector::Constant(static_cast<Eigen::Index>(m), std::numeric_limits<double>::quiet_NaN());
}
void ReducedProblem::anchorColumns(const Vector &shifts) {
    clearAnchor();
    if (shifts.size() != last_shifts.size())
        return;
    for (Eigen::Index index = 0; index < shifts.size(); ++index)
        if (std::bit_cast<std::uint64_t>(shifts[index]) != std::bit_cast<std::uint64_t>(last_shifts[index]))
            return;
    anchor_.shifts = shifts;
    anchor_.weighted = columns;
    anchor_.unweighted = unweighted_columns;
    anchor_.valid = true;
}
void ReducedProblem::update(const Vector &shifts) {
    checkpoint(context);
    if (shifts.size() != columns.cols())
        fail("pfsm.shifts", "Shift dimensions disagree");
    for (Eigen::Index j = 0; j < shifts.size(); ++j) {
        const auto &p = input.phases[static_cast<std::size_t>(j)];
        if (!std::isfinite(shifts[j]) || shifts[j] < p.lower_shift || shifts[j] > p.upper_shift)
            fail("pfsm.shifts", "Shift is outside its finite bounds");
        if (shifts[j] == last_shifts[j])
            continue;
        if (anchor_.valid &&
            std::bit_cast<std::uint64_t>(shifts[j]) == std::bit_cast<std::uint64_t>(anchor_.shifts[j])) {
            columns.col(j) = anchor_.weighted.col(j);
            unweighted_columns.col(j) = anchor_.unweighted.col(j);
            last_shifts[j] = shifts[j];
            linear_workspace.valid[static_cast<std::size_t>(j)] = false;
            countWork(&WorkCounters::anchor_restores);
            continue;
        }
        countWork(&WorkCounters::formal_columns);
        auto values =
            exact_mode ? p.profile->evaluateExact(input.observations->coordinates, shifts[j], false, context)
                       : p.profile->evaluate(input.observations->coordinates, shifts[j], false, context);
        if (!values)
            throw Failure{values.error()};
        if (values.value().intensity.size() != static_cast<std::size_t>(columns.rows()))
            fail("pfsm.template", "Template returned misaligned samples");
        for (Eigen::Index i = 0; i < columns.rows(); ++i) {
            const double value = values.value().intensity[static_cast<std::size_t>(i)];
            if (!std::isfinite(value) || value < 0)
                fail("pfsm.template", "Invalid template intensity");
            unweighted_columns(i, j) = value;
            columns(i, j) = (value * sqrt_weights[i]) / normalization;
        }
        if (!columns.col(j).allFinite())
            fail("pfsm.template", "Weighted template overflow", ErrorCode::numerical_failure);
        linear_workspace.valid[static_cast<std::size_t>(j)] = false;
        last_shifts[j] = shifts[j];
    }
}
Evaluation ReducedProblem::evaluate(const Vector &shifts, bool difference) {
    update(shifts);
    Evaluation e;
    e.linear = nnls(columns, z, input.options.maximum_nnls_iterations, linear_workspace, context);
    counts.residual_evaluations++;
    if (difference)
        counts.difference_evaluations++;
    counts.nnls_iterations += e.linear.iterations;
    e.scales = e.linear.scales;
    // Linear diagnostics retain scales/KKT only; transfer the full residual
    // instead of allocating and copying another observation-sized vector.
    e.residual = std::move(e.linear.residual);
    e.cost = .5 * e.residual.squaredNorm();
    if (!std::isfinite(e.cost))
        fail("pfsm.residual", "Nonfinite residual", ErrorCode::numerical_failure);
    return e;
}
Evaluation ReducedProblem::explicitParameters(const ProfileFitParameters &parameters) {
    if (parameters.scales.size() != input.phases.size())
        fail("pfsm.fallback", "Fallback scale dimensions disagree");
    const Vector shifts = vectorOf(parameters.shifts);
    auto e = evaluate(shifts); // Validate the fallback's location and diagnose
                               // the template matrix.
    Vector scales = vectorOf(parameters.scales);
    if (!scales.allFinite() || (scales.array() < 0).any())
        fail("pfsm.fallback", "Fallback scales must be nonnegative and finite");
    const Vector residual = z - columns * scales;
    const double cost = .5 * residual.squaredNorm();
    if (cost < e.cost) {
        e.scales = scales;
        e.residual = residual;
        e.cost = cost;
    }
    return e;
}
double ReducedProblem::scanObjective(const Evaluation &e, const Vector &shifts,
                                     std::vector<double> &calculated) {
    update(shifts);
    const auto &data = *input.observations;
    const auto n = data.coordinates.size();
    calculated.assign(n, 0);
    // Retain result()'s phase-major construction and original-unit reduction.
    // 2*cost*Dnet would change rounding and therefore scan minima/start choices.
    for (std::size_t j = 0; j < input.phases.size(); ++j)
        for (std::size_t i = 0; i < n; ++i) {
            if ((i & 255) == 0)
                checkpoint(context);
            calculated[i] += e.scales[static_cast<Eigen::Index>(j)] *
                unweighted_columns(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j));
        }
    double q = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if ((i & 255) == 0)
            checkpoint(context);
        const double residual = (data.observed[i] - data.background[i]) - calculated[i];
        calculated[i] += data.background[i];
        const double weighted = sqrt_weights[static_cast<Eigen::Index>(i)] * residual;
        q += weighted * weighted;
        if (!std::isfinite(calculated[i]) || !std::isfinite(residual))
            throw Failure{{ErrorCode::invalid_input, "pfsm.algorithm.output", "Invalid fitted profile payload",
                           "invalid_algorithm_output"}};
    }
    if (!std::isfinite(q))
        fail("pfsm.residual", "Final residual overflow", ErrorCode::numerical_failure);
    if (!std::isfinite(std::sqrt(q / net_denominator)))
        throw Failure{{ErrorCode::invalid_input, "pfsm.algorithm.output", "Invalid fitted profile payload",
                       "invalid_algorithm_output"}};
    return q;
}
ProfileFitResult ReducedProblem::result(const Evaluation &e, const Vector &shifts,
                                        ProfileFitTermination termination, double optimality) {
    update(shifts);
    ProfileFitResult out;
    out.parameters = {valuesOf(e.scales), valuesOf(shifts)};
    out.denominator_net = net_denominator;
    out.denominator_observed = observed_denominator;
    out.termination = termination;
    out.diagnostics = counts;
    out.diagnostics.interpolated_iterations = approximated;
    out.diagnostics.exact_final_verification = !approximated || exact_mode;
    out.diagnostics.rank = numericalRank(columns, context);
    out.diagnostics.rank_deficient =
        e.linear.deficient || out.diagnostics.rank < static_cast<std::size_t>(columns.cols());
    out.diagnostics.kkt_violation = e.linear.kkt;
    out.diagnostics.optimality = optimality;
    out.diagnostics.kkt_checked = (e.linear.scales.array() == e.scales.array()).all();
    out.diagnostics.zero_templates = e.linear.zero;
    const auto n = input.observations->coordinates.size();
    out.calculated.resize(n);
    // update() owns the exact samples used to form each weighted column,
    // including zero-weight points. Reuse them without undoing normalization or
    // evaluating the full IL a second time. enableExact() invalidates both caches.
    for (std::size_t j = 0; j < input.phases.size(); ++j) {
        for (std::size_t i = 0; i < n; ++i) {
            if ((i & 255) == 0)
                checkpoint(context);
            const double value =
                unweighted_columns(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j));
            out.calculated[i] += out.parameters.scales[j] * value;
        }
        out.diagnostics.unidentifiable_shifts.push_back(out.parameters.scales[j] == 0);
        const auto &phase = input.phases[j];
        out.diagnostics.at_bound.push_back(std::min(std::abs(out.parameters.shifts[j] - phase.lower_shift),
                                                    std::abs(out.parameters.shifts[j] - phase.upper_shift)) <=
                                           1e-8 * width);
    }
    out.residual.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        out.residual[i] =
            (input.observations->observed[i] - input.observations->background[i]) - out.calculated[i];
        out.calculated[i] += input.observations->background[i];
        const double weighted = sqrt_weights[static_cast<Eigen::Index>(i)] * out.residual[i];
        out.q += weighted * weighted;
    }
    if (!std::isfinite(out.q))
        fail("pfsm.residual", "Final residual overflow", ErrorCode::numerical_failure);
    out.rwp_net = std::sqrt(out.q / net_denominator);
    if (observed_denominator > 0)
        out.rwp_observed = std::sqrt(out.q / observed_denominator);
    return out;
}
} // namespace qualpf::profile::detail

namespace qualpf::profile {
Result<PreparedProfileObservationsHandle> prepareProfileObservations(
    std::shared_ptr<const ProfileObservations> observations, const ExecutionContext &context) try {
    return Result<PreparedProfileObservationsHandle>{detail::prepareObservations(std::move(observations), context, true)};
} catch (const detail::Failure &failure) {
    return Result<PreparedProfileObservationsHandle>{failure.error};
}
std::shared_ptr<const ProfileObservations> profileObservationSnapshot(const PreparedProfileObservationsHandle &prepared) {
    return prepared ? prepared->observations : nullptr;
}
Status validateProfileFitResult(const ProfileFitResult &value, const ProfileFitInput &input,
                                bool requireExact) {
    const auto invalid = [] {
        return Status{Error{ErrorCode::invalid_input, "pfsm.algorithm.output", "Invalid fitted profile payload",
                            "invalid_algorithm_output"}};
    };
    if (!input.observations)
        return invalid();
    const auto m = input.phases.size(), n = input.observations->coordinates.size();
    if ((requireExact && !value.diagnostics.exact_final_verification) ||
        value.parameters.scales.size() != m || value.parameters.shifts.size() != m ||
        value.calculated.size() != n || value.residual.size() != n || !std::isfinite(value.q) ||
        value.q < 0 || !std::isfinite(value.rwp_net) || value.rwp_net < 0 ||
        !std::isfinite(value.denominator_net) || value.denominator_net <= 0)
        return invalid();
    for (std::size_t j = 0; j < m; ++j)
        if (!std::isfinite(value.parameters.scales[j]) || value.parameters.scales[j] < 0 ||
            !std::isfinite(value.parameters.shifts[j]) ||
            value.parameters.shifts[j] < input.phases[j].lower_shift ||
            value.parameters.shifts[j] > input.phases[j].upper_shift)
            return invalid();
    for (std::size_t i = 0; i < n; ++i)
        if (!std::isfinite(value.calculated[i]) || !std::isfinite(value.residual[i]))
            return invalid();
    if (std::abs(value.rwp_net - std::sqrt(value.q / value.denominator_net)) >
        64 * std::numeric_limits<double>::epsilon() * std::max(1., value.rwp_net))
        return invalid();
    return Status{std::monostate{}};
}
namespace {
void checkScan(const ProfileFitInput &input, std::span<const double> shifts, std::size_t phase) {
    if (phase >= input.phases.size() || shifts.size() != input.phases.size())
        detail::fail("pfsm.scan", "Scan phase or shift dimensions disagree");
    for (std::size_t j = 0; j < shifts.size(); ++j)
        if (!std::isfinite(shifts[j]) || shifts[j] < input.phases[j].lower_shift ||
            shifts[j] > input.phases[j].upper_shift)
            detail::fail("pfsm.shifts", "Shift is outside its finite bounds");
}
} // namespace
Result<ProfileScanResult> IProfileFitter::scanAt(const ProfileFitInput &input,
    std::span<const double> fixedShifts, std::size_t phase, std::span<const double> values,
    const ExecutionContext &context) const try {
    detail::checkpoint(context);
    checkScan(input, fixedShifts, phase);
    std::vector<double> shifts(fixedShifts.begin(), fixedShifts.end());
    ProfileScanResult out;
    out.q.reserve(values.size());
    for (double value : values) {
        detail::checkpoint(context);
        shifts[phase] = value;
        if (!std::isfinite(value) || value < input.phases[phase].lower_shift ||
            value > input.phases[phase].upper_shift)
            detail::fail("pfsm.shifts", "Shift is outside its finite bounds");
        auto result = evaluateAt(input, shifts, context);
        if (!result)
            return Result<ProfileScanResult>{result.error()};
        auto valid = validateProfileFitResult(result.value(), input, false);
        if (!valid)
            return Result<ProfileScanResult>{valid.error()};
        out.q.push_back(result.value().q);
        out.residual_evaluations += result.value().diagnostics.residual_evaluations;
        out.nnls_iterations += result.value().diagnostics.nnls_iterations;
    }
    return Result<ProfileScanResult>{std::move(out)};
} catch (const detail::Failure &failure) {
    return Result<ProfileScanResult>{failure.error};
} catch (const std::exception &error) {
    return Result<ProfileScanResult>{Error{ErrorCode::numerical_failure, "pfsm.fit", error.what()}};
}
Result<ProfileScanResult> NnlsTrfProfileFitter::scanAt(const ProfileFitInput &input,
    std::span<const double> fixedShifts, std::size_t phase, std::span<const double> values,
    const ExecutionContext &context) const try {
    checkScan(input, fixedShifts, phase);
    detail::ReducedProblem problem(input, context);
    auto shifts = detail::vectorOf(fixedShifts);
    ProfileScanResult out;
    out.q.reserve(values.size());
    std::vector<double> calculated;
    for (double value : values) {
        detail::checkpoint(context);
        shifts[static_cast<Eigen::Index>(phase)] = value;
        const auto evaluation = problem.evaluate(shifts);
        out.q.push_back(problem.scanObjective(evaluation, shifts, calculated));
    }
    out.residual_evaluations = problem.counts.residual_evaluations;
    out.nnls_iterations = problem.counts.nnls_iterations;
    return Result<ProfileScanResult>{std::move(out)};
} catch (const detail::Failure &failure) {
    return Result<ProfileScanResult>{failure.error};
} catch (const std::exception &error) {
    return Result<ProfileScanResult>{Error{ErrorCode::numerical_failure, "pfsm.fit", error.what()}};
}
Result<ProfileFitResult> NnlsTrfProfileFitter::evaluateAt(const ProfileFitInput &input,
                                                          std::span<const double> shifts,
                                                          const ExecutionContext &context) const try {
    detail::ReducedProblem problem(input, context);
    const auto x = detail::vectorOf(shifts);
    const auto evaluation = problem.evaluate(x);
    return Result<ProfileFitResult>{problem.result(evaluation, x, ProfileFitTermination::fixed_shifts, 0)};
} catch (const detail::Failure &failure) {
    return Result<ProfileFitResult>{failure.error};
} catch (const std::exception &error) {
    return Result<ProfileFitResult>{Error{ErrorCode::numerical_failure, "pfsm.fit", error.what()}};
}
Result<ProfileFitResult> NnlsTrfProfileFitter::fit(const ProfileFitInput &input,
                                                   const ExecutionContext &context) const try {
    detail::ReducedProblem problem(input, context);
    detail::Vector initial(static_cast<Eigen::Index>(input.phases.size()));
    for (std::size_t j = 0; j < input.phases.size(); ++j)
        initial[static_cast<Eigen::Index>(j)] = input.phases[j].initial_shift;
    const auto initial_termination =
        problem.free.empty() ? ProfileFitTermination::fixed_shifts : ProfileFitTermination::feasible_fallback;
    detail::OuterResult best{problem.evaluate(initial), initial, initial_termination, 0};
    if (input.fallback) {
        auto fallback = problem.explicitParameters(*input.fallback);
        if (fallback.cost <= best.evaluation.cost)
            best = {std::move(fallback), detail::vectorOf(input.fallback->shifts), initial_termination, 0};
    }
    auto starts = input.starts;
    if (starts.empty())
        starts.push_back(detail::valuesOf(initial));
    for (const auto &start : starts) {
        detail::checkpoint(context);
        auto candidate = detail::trf(problem, detail::vectorOf(start));
        ++problem.counts.starts;
        if (candidate.evaluation.cost <= best.evaluation.cost)
            best = std::move(candidate);
    }
    if (problem.approximated) {
        // The cache accelerates scans and iterations only. Re-solve all scales
        // with direct IL before publishing a fit or comparing its feasible
        // fallback.
        problem.enableExact();
        best.evaluation = problem.evaluate(best.shifts);
        if ((initial.array() != best.shifts.array()).any()) {
            auto initialEvaluation = problem.evaluate(initial);
            if (initialEvaluation.cost < best.evaluation.cost)
                best = {std::move(initialEvaluation), initial, initial_termination, 0};
        }
        if (input.fallback) {
            auto fallback = problem.explicitParameters(*input.fallback);
            if (fallback.cost < best.evaluation.cost)
                best = {std::move(fallback), detail::vectorOf(input.fallback->shifts), initial_termination,
                        0};
        }
    }
    return Result<ProfileFitResult>{
        problem.result(best.evaluation, best.shifts, best.termination, best.optimality)};
} catch (const detail::Failure &failure) {
    return Result<ProfileFitResult>{failure.error};
} catch (const std::exception &error) {
    return Result<ProfileFitResult>{Error{ErrorCode::numerical_failure, "pfsm.fit", error.what()}};
}
} // namespace qualpf::profile
