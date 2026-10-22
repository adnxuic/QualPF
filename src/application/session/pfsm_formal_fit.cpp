#include "pfsm_formal_fit.h"
#include "pfsm_checks.h"

namespace qualpf::application::pfsm_detail {
using namespace profile;
double calculatePfsmFom(double deltaQ, double denominatorNet, double baselineRwp, double trialRwp) {
    // Rationalized difference avoids loss of significance near equal fits.
    const double denominator = baselineRwp + trialRwp;
    return denominator > 0 ? (deltaQ / denominatorNet) / denominator : 0;
}
PfsmFormalCandidateResult fitPfsmCandidate(PfsmFormalCandidateInput request,
    const IProfileExpander &expander, const IProfileFitter &fitter,
    const ProfileFitResult &baseline, const ExecutionContext &context) {
    checkpoint(context);
    PfsmFormalCandidateResult result;
    auto input = std::move(request.fit);
    auto begin = Clock::now();
    const auto profile = request.prepared && request.prepared->profile() &&
        dynamic_cast<const IntermediateLorentzianExpander *>(&expander) &&
        request.prepared->matches(request.expansion, context)
        ? request.prepared->profile() : take(expander.prepare(request.expansion, context));
    input.phases.push_back({request.identity, profile,
        request.lower_shift, request.upper_shift, std::clamp(0., request.lower_shift, request.upper_shift)});
    result.expansion_seconds = elapsed(begin);
    begin = Clock::now();
    const auto m = input.phases.size();
    std::vector<double> shifts = baseline.parameters.shifts;
    shifts.push_back(input.phases.back().initial_shift);
    input.fallback = baseline.parameters;
    input.fallback->shifts = shifts;
    input.fallback->scales.push_back(0);
    const double step = request.expansion.fwhm_degrees / 4;
    const double intervals = (request.upper_shift - request.lower_shift) / step;
    if (!std::isfinite(intervals) || intervals > 1000000)
        fail("pfsm.scan", "Shift scan requires more than one million steps");
    std::vector<double> scan{request.lower_shift, request.upper_shift, shifts.back()};
    for (std::size_t k = 1; k <= static_cast<std::size_t>(intervals); ++k) {
        const double shift = request.lower_shift + double(k) * step;
        if (shift < request.upper_shift)
            scan.push_back(shift);
    }
    std::sort(scan.begin(), scan.end());
    scan.erase(std::unique(scan.begin(), scan.end()), scan.end());
    const auto scanned = checkedScan(fitter.scanAt(input, shifts, m - 1, scan, context), scan.size());
    const auto &objective = scanned.q;
    std::vector<std::size_t> minima;
    for (std::size_t k = 0; k < scan.size(); ++k)
        if ((k == 0 || objective[k] <= objective[k - 1]) &&
            (k + 1 == scan.size() || objective[k] <= objective[k + 1]))
            minima.push_back(k);
    std::stable_sort(minima.begin(), minima.end(),
                     [&](auto a, auto b) { return objective[a] < objective[b]; });
    if (minima.size() > 8)
        minima.resize(8);
    const auto addStart = [&](double shift) {
        auto start = baseline.parameters.shifts;
        start.push_back(shift);
        if (std::find(input.starts.begin(), input.starts.end(), start) == input.starts.end())
            input.starts.push_back(std::move(start));
    };
    for (auto k : minima)
        addStart(scan[k]);
    addStart(input.phases.back().initial_shift);
    addStart(request.lower_shift);
    addStart(request.upper_shift);
    auto fitted = checkedFit(fitter.fit(input, context), input);
    fitted.diagnostics.residual_evaluations += scanned.residual_evaluations;
    fitted.diagnostics.nnls_iterations += scanned.nnls_iterations;
    // A direct fallback keeps even floating-point summation differences from
    // creating a negative improvement. It is an existing feasible full solution.
    if (fitted.q > baseline.q) {
        fitted.parameters = *input.fallback;
        fitted.q = baseline.q;
        fitted.rwp_net = baseline.rwp_net;
        fitted.termination = ProfileFitTermination::feasible_fallback;
        fitted.diagnostics.kkt_checked = false;
    }
    auto evidence = std::make_shared<PfsmCandidateEvidence>();
    evidence->original_index = request.original_index;
    evidence->generation = request.generation;
    evidence->parameters = std::move(fitted.parameters);
    evidence->diagnostics = std::move(fitted.diagnostics);
    evidence->termination = fitted.termination;
    evidence->q = fitted.q;
    evidence->rwp_net = fitted.rwp_net;
    evidence->delta_q = baseline.q - fitted.q;
    evidence->delta_rwp = calculatePfsmFom(evidence->delta_q, baseline.denominator_net,
        baseline.rwp_net, fitted.rwp_net);
    result.evidence = std::move(evidence);
    result.solve_seconds = elapsed(begin);
    return result;
}
} // namespace qualpf::application::pfsm_detail
