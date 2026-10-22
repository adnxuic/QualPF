#include "candidate_recall_diagnostics.h"
#include <QJsonArray>
using namespace qualpf::profile;
namespace {
QJsonArray numbers(const std::vector<double> &v) {QJsonArray a;for(auto n:v)a.append(n);return a;}
QJsonArray indices(const std::vector<std::size_t> &v) {QJsonArray a;for(auto n:v)a.append(qint64(n));return a;}
QJsonArray strings(const std::vector<std::string> &v) {QJsonArray a;for(const auto &s:v)a.append(QString::fromStdString(s));return a;}
QJsonObject parameters(const ProfileFitParameters &p) {return {{"scales",numbers(p.scales)},{"shifts",numbers(p.shifts)}};}
QJsonObject physical(const PfsmCandidateRecallPhysicalFit &p) {
    return {{"parameters",parameters(p.parameters)},{"q",p.q},{"gain",p.gain},{"difference_gain",p.difference_gain},
        {"kkt",p.kkt},{"baseline_fallback",p.baseline_fallback},{"approximated",p.approximated}};
}
}
QJsonObject pfsmCandidateRecallLimitsJson(const PfsmCandidateRecallLimits &limits) {
    return {{"recall_candidates",limits.recall_candidates},{"local_shortlist",limits.local_shortlist},
        {"protected_per_channel",limits.protected_per_channel},{"review_limit",limits.review_limit},
        {"formal_limit",limits.formal_limit}};
}
QJsonObject pfsmIntegralRecallReportJson(const qualpf::application::PfsmIntegralRecallReport &d) {
    return {{"source_revision", "3fa1123a16ab746f80f946dea55f4b9f5b633e48"}, {"implementation", "baseline"},
        {"baseline_shifts",numbers(d.baseline_shifts)}, {"fine_bins",qint64(d.fine_bins)}, {"coarse_bins",qint64(d.coarse_bins)}, {"bank_columns",qint64(d.bank_columns)},
        {"peak_buffer_bytes",qint64(d.peak_buffer_bytes)}, {"coarse_evaluations",qint64(d.coarse_evaluations)},
        {"fine_evaluations",qint64(d.fine_evaluations)}, {"relaxed_evaluations",qint64(d.relaxed_evaluations)},
        {"coarse_baseline_q",d.coarse_baseline_q}, {"fine_baseline_q",d.fine_baseline_q},
        {"fine_denominator",d.fine_denominator}, {"tau",d.tau}, {"relaxation_radius",d.relaxation_radius},
        {"shared_seconds",d.shared_seconds}, {"integral_seconds",d.integral_seconds}, {"relaxed_seconds",d.relaxed_seconds},
        {"fine_budget",qint64(d.options.fine_budget)}, {"coarse_budget",qint64(d.options.coarse_budget)},
        {"original_budget",qint64(d.options.original_budget)}, {"residual_budget",qint64(d.options.residual_budget)},
        {"relaxed_budget",qint64(d.relaxed_options.budget)}, {"nnls_limit",qint64(d.relaxed_options.maximum_evaluations)},
        {"tail_mass_epsilon",d.options.tail_mass_epsilon}};
}
QJsonObject pfsmIntegralRecallCandidateJson(const PfsmScreenEvidence &e) {
    const auto &v=e.relaxed;
    QJsonObject out{{"coarse_shift",e.coarse_shift},{"fine_shift",e.fine_shift},
        {"coarse_evaluations",qint64(e.coarse_evaluations)},{"fine_evaluations",qint64(e.fine_evaluations)},
        {"integral_complete",e.integral_complete},{"uncertainty",strings(e.uncertainty)},
        {"fine_selected",e.main_selected},{"coarse_selected",e.coarse_selected},
        {"original_selected",e.original_selected},{"residual_selected",e.residual_selected},
        {"relaxed_selected",v.selected},{"relaxed_complete",v.complete},{"relaxed_uncertainty",QString::fromStdString(v.uncertainty)},
        {"relaxed_gain",v.gain},{"relaxed_fixed_q",v.fixed_q},{"relaxed_best_q",v.best.q},{"relaxed_null_q",v.null.q},
        {"relaxed_best_shifts",numbers(v.best.shifts)},{"relaxed_best_scales",numbers(v.best.scales)},
        {"relaxed_null_shifts",numbers(v.null.shifts)},{"relaxed_null_scales",numbers(v.null.scales)},
        {"released",indices(v.released)},{"relaxed_evaluations",qint64(v.evaluations)},
        {"relaxed_kkt",v.kkt},{"candidate_cache_columns",qint64(v.cache_columns)},
        {"candidate_cache_bytes",qint64(v.cache_bytes)},{"buffer_bytes",qint64(e.buffer_bytes+v.buffer_bytes)}};
    if(e.gain)out["coarse_gain"]=*e.gain;
    if(e.normalized_gain)out["normalized_coarse_gain"]=*e.normalized_gain;
    if(e.fine_gain)out["fine_gain"]=*e.fine_gain;
    if(e.normalized_fine_gain)out["normalized_fine_gain"]=*e.normalized_fine_gain;
    return out;
}
QJsonObject pfsmCandidateRecallCandidateJson(const PfsmCandidateRecallCandidate &c) {
    QJsonObject out{{"original_index",qint64(c.recall.original_index)},
        {"initialized",c.initialized},{"reviewed",c.reviewed},{"seeds",numbers(c.seeds)},
        {"original_seeds",numbers(c.recall.original_seeds)},{"residual_seeds",numbers(c.recall.residual_seeds)},
        {"nnls_calls",qint64(c.nnls_calls)},{"nnls_iterations",qint64(c.nnls_iterations)},
        {"maximum_distortion",c.maximum_distortion},{"best",physical(c.best)},
        {"numerical_protection",QString::fromStdString(c.numerical_protection)},
        {"review_reasons",strings(c.review_reasons)},{"retention_reasons",strings(c.retention_reasons)}};
    if(c.initial_gain)out["initial_gain"]=*c.initial_gain;
    if(c.initial_eta)out["initial_eta"]=*c.initial_eta;
    if(c.eta)out["eta"]=*c.eta;
    if(c.formal_gain)out["formal_gain"]=*c.formal_gain;
    if(c.initial_rank)out["initial_rank"]=qint64(*c.initial_rank);
    if(c.local_rank)out["local_rank"]=qint64(*c.local_rank);
    QJsonArray starts;
    for(const auto &s:c.starts) {
        QJsonObject seed{{"initial_shift",s.initial_shift},{"radius",s.radius},{"initialized",s.initialized},
            {"initial_fit",physical(s.initial_fit)},{"current",physical(s.current)}};
        QJsonArray steps;
        for(const auto &t:s.steps) {
            QJsonObject step{{"anchor",parameters(t.anchor)},{"linear_coefficients",numbers(t.linear_coefficients)},
                {"physical",physical(t.physical)},{"predicted",t.predicted},{"actual",t.actual},
                {"distortion",t.distortion},{"radius",t.radius},{"accepted",t.accepted},
                {"indistinguishable",t.indistinguishable},{"local_boundary",t.local_boundary}};
            if(t.ratio)step["rho"]=*t.ratio;
            steps.append(step);
        }
        seed["steps"]=steps;starts.append(seed);
    }
    out["starts"]=starts;return out;
}
QJsonObject pfsmCandidateRecallSelectionJson(const PfsmCandidateRecallSelection &s) {
    QJsonObject out{{"g_star",s.g_star},{"initial_top20",indices(s.initial_top)},{"review_top20",indices(s.local_top)},
        {"channel_protection",indices(s.channel)},{"review_indices",indices(s.review)},
        {"unresolved_protection",indices(s.protected_indices)},{"proposed_indices",indices(s.proposed)},
        {"retained_indices",indices(s.retained)},{"review_skipped",s.review_skipped},
        {"full_fallback",s.full_fallback},{"fallback_reason",QString::fromStdString(s.fallback_reason)}};
    if (s.limits != PfsmCandidateRecallLimits{}) {
        out["limits"] = pfsmCandidateRecallLimitsJson(s.limits);
        out["initial_top"] = out.take("initial_top20");
        out["local_top"] = out.take("review_top20");
    }
    return out;
}
