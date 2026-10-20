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
