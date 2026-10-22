#include "pfsm_workflow.h"
#include "pfsm_candidate_recall.h"
#include <cfenv>
#include <omp.h>

namespace qualpf::application::pfsm_detail {
PfsmCandidateRecallResult integralPfsmRecallCandidates(const PfsmCandidateRecallInput &a,
    PfsmCandidateRecallResult out, const ExecutionContext &context) {
    using namespace profile;
    auto &report = *out.report;
    report.backend = PfsmCandidateRecallBackend::bounded_integral_relaxed;
    auto &diagnostic = report.integral.emplace();
    const auto &p = *a.problem;
    diagnostic.baseline_shifts = a.baseline.parameters.shifts;
    const auto fallback = [&](const char *reason) {
        checkpoint(context);
        report.full_fallback = true; report.fallback_reason = reason;
        report.recalled_indices = report.retained_indices = out.formal_indices = a.all_indices;
        // A failed shared/worker stage never publishes a partial selection.
        report.candidates.clear(); diagnostic.candidates.clear();
        return std::move(out);
    };
    try {
        auto start = Clock::now();
        const auto finishShared = [&] {
            const auto seconds = elapsed(start);
            diagnostic.shared_seconds += seconds; out.timing.shared += seconds;
        };
        if (!out.integral_data) {
            auto data = preparePfsmIntegralData(p.observations, p.source.break_before,
                p.physical_width->width.value, diagnostic.options, context);
            if (!data) {
                finishShared();
                if (data.error().code != ErrorCode::numerical_failure) throw Failure{data.error()};
                return fallback("integral_shared_numerical_failure");
            }
            out.integral_data = std::move(data).value();
        }
        const auto &data = *out.integral_data;
        diagnostic.fine_bins = data.fine.weights.size(); diagnostic.coarse_bins = data.coarse.weights.size();
        diagnostic.fine_denominator = data.fine.denominator;
        std::vector<ProfileExpansionInput> selected;
        for (const auto &phase : a.selected) {
            const auto found = std::find_if(a.universe->begin(), a.universe->end(),
                [&](const auto &card) { return card.id == phase.identity; });
            if (found == a.universe->end()) fail("pfsm.integral.selected", "Selected phase missing from frozen universe");
            selected.push_back(expansionInput(*found, p)); selected.back().interpolation_window.reset();
        }
        auto prepared = preparePfsmIntegralRound(out.integral_data, selected,
            a.baseline.parameters.shifts, a.baseline.residual, p.fit, context);
        if (!prepared) {
            finishShared();
            if (prepared.error().code != ErrorCode::numerical_failure) throw Failure{prepared.error()};
            return fallback("integral_baseline_numerical_failure");
        }
        const auto round = std::move(prepared).value();
        auto relaxation = preparePfsmRelaxedRound(out.integral_data, selected,
            a.baseline.parameters.shifts, p.lower_shift, p.upper_shift, p.fit, diagnostic.relaxed_options, context);
        finishShared();
        if (!relaxation) {
            if (relaxation.error().code != ErrorCode::numerical_failure) throw Failure{relaxation.error()};
            return fallback("integral_relaxed_shared_numerical_failure");
        }
        const auto relaxed = std::move(relaxation).value();
        diagnostic.coarse_baseline_q = round.coarse_baseline_q;
        report.local_baseline_q = diagnostic.fine_baseline_q = round.fine_baseline_q;
        diagnostic.tau = round.tau; diagnostic.relaxation_radius = relaxed.radius;
        diagnostic.bank_columns = relaxed.bank_columns;
        diagnostic.peak_buffer_bytes = data.buffer_bytes + round.buffer_bytes + relaxed.buffer_bytes;
        for (const auto &input : selected) diagnostic.peak_buffer_bytes += input.lines.capacity() * sizeof(ProfileLine);
        diagnostic.candidates.resize(a.all_indices.size());
        std::vector<std::size_t> workerBytes(std::size_t(std::max(1, a.threads)), 0);
        std::fenv_t environment; std::fegetenv(&environment);
        const auto workers = [&](auto work) {
            std::vector<std::optional<Error>> errors(a.all_indices.size());
            std::vector<std::uint8_t> allocationFailure(a.all_indices.size(), 0);
            const int threads = a.threads;
#pragma omp parallel for schedule(dynamic, 1) num_threads(threads)
            for (std::ptrdiff_t slot = 0; slot < static_cast<std::ptrdiff_t>(a.all_indices.size()); ++slot) {
                const auto i = std::size_t(slot);
                std::fenv_t previous; std::fegetenv(&previous); std::fesetenv(&environment);
                try {
                    checkpoint(context);
                    auto input = expansionInput((*a.universe)[a.all_indices[i]], p); input.interpolation_window.reset();
                    work(diagnostic.candidates[i], input);
                    auto &bytes = workerBytes[std::size_t(omp_get_thread_num())];
                    bytes = std::max(bytes, diagnostic.candidates[i].buffer_bytes +
                        diagnostic.candidates[i].relaxed.buffer_bytes + input.lines.capacity() * sizeof(ProfileLine));
                } catch (const std::bad_alloc &) { allocationFailure[i] = 1;
                } catch (const Failure &failure) { errors[i] = failure.error;
                } catch (const std::exception &failure) {
                    errors[i] = Error{ErrorCode::numerical_failure, "pfsm.integral.worker", failure.what()};
                } catch (...) { errors[i] = Error{ErrorCode::numerical_failure, "pfsm.integral.worker", "Unknown integral worker failure"}; }
                diagnostic.candidates[i].original_index = a.all_indices[i];
                std::fesetenv(&previous);
            }
            checkpoint(context);
            for (std::size_t i = 0; i < errors.size(); ++i) if (errors[i]) {
                if (errors[i]->code != ErrorCode::numerical_failure) throw Failure{*errors[i]};
                diagnostic.candidates[i].uncertainty.push_back("integral_candidate_numerical_failure");
            }
            if (std::find(allocationFailure.begin(), allocationFailure.end(), 1) != allocationFailure.end()) throw std::bad_alloc{};
        };
        start = Clock::now();
        workers([&](auto &e, const auto &input) {
            e = take(evaluatePfsmIntegralCandidate(round, input, p.lower_shift, p.upper_shift, p.fit, context));
        });
        diagnostic.integral_seconds = elapsed(start); out.timing.correlation += diagnostic.integral_seconds;
        start = Clock::now();
        workers([&](auto &e, const auto &input) { e.relaxed = take(evaluatePfsmRelaxedCandidate(relaxed, input, e, context)); });
        diagnostic.relaxed_seconds = elapsed(start); out.timing.review += diagnostic.relaxed_seconds;
        for (auto bytes : workerBytes) diagnostic.peak_buffer_bytes += bytes;
        for (auto &e : diagnostic.candidates) {
            if (!e.relaxed.complete && e.relaxed.uncertainty.empty()) e.relaxed.uncertainty = "relaxed_candidate_numerical_failure";
            if (!e.relaxed.uncertainty.empty()) e.uncertainty.push_back(e.relaxed.uncertainty);
            e.nnls_iterations += e.relaxed.iterations;
            diagnostic.coarse_evaluations += e.coarse_evaluations;
            diagnostic.fine_evaluations += e.fine_evaluations;
            diagnostic.relaxed_evaluations += e.relaxed.evaluations;
        }
        take(selectPfsmIntegralCandidates(diagnostic.candidates, diagnostic.options, context));
        auto selectedIndices = take(selectPfsmRelaxedCandidates(diagnostic.candidates, diagnostic.relaxed_options, context));
        report.recalled_indices = report.retained_indices = std::move(selectedIndices.indices);
        for (const auto &e : diagnostic.candidates) {
            PfsmCandidateRecallEvidence common;
            common.original_index = e.original_index; common.original_score = e.original.score; common.residual_score = e.residual.score;
            common.original_shift = e.original.shift; common.residual_shift = e.residual.shift; common.nnls_iterations = e.nnls_iterations;
            common.recalled = common.retained = e.retained;
            if (!e.uncertainty.empty()) common.protection_reason = e.uncertainty.front();
            if (e.retained) common.recall_reason = common.retention_reason = !common.protection_reason.empty()
                ? "numerical_protection" : "integral_channel_union";
            report.candidates.push_back(std::move(common));
        }
        if (p.candidate_recall.mode == PfsmCandidateRecallMode::enabled) out.formal_indices = report.retained_indices;
        return out;
    } catch (const std::bad_alloc &) { return fallback("integral_allocation_failure"); }
}
} // namespace qualpf::application::pfsm_detail
