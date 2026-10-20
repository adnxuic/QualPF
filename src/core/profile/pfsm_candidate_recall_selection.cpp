#include "qualpf/profile/pfsm_candidate_recall.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace qualpf::profile {
namespace {
using Rows = std::vector<PfsmCandidateRecallCandidate>;
using Indices = std::vector<std::size_t>;
bool tied(double a, double b) {
    return std::abs(a-b)<=128*std::numeric_limits<double>::epsilon()*std::max({1.,std::abs(a),std::abs(b)});
}
void ordered(Indices &v) {std::sort(v.begin(),v.end());v.erase(std::unique(v.begin(),v.end()),v.end());}
bool contains(const Indices &v,std::size_t i) {return std::binary_search(v.begin(),v.end(),i);}
template<class Score> Indices rank(const Rows &rows,Score score) {
    Indices out;
    for(std::size_t i=0;i<rows.size();++i) {
        auto v=score(rows[i]);if(v && std::isfinite(*v))out.push_back(i);
    }
    std::sort(out.begin(),out.end(),[&](auto a,auto b) {
        const double x=*score(rows[a]),y=*score(rows[b]);
        return x!=y?x>y:rows[a].recall.original_index<rows[b].recall.original_index;
    });
    return out;
}
template<class Score> Indices top(const Rows &rows,const Indices &ranked,std::size_t budget,Score score) {
    Indices out;const auto n=std::min(budget,ranked.size());
    for(std::size_t i=0;i<ranked.size();++i)
        if(i<n || (n && tied(*score(rows[ranked[i]]),*score(rows[ranked[n-1]]))))
            out.push_back(rows[ranked[i]].recall.original_index);
    ordered(out);return out;
}
std::optional<double> initial(const PfsmCandidateRecallCandidate &r) {return r.initial_gain;}
std::optional<double> gain(const PfsmCandidateRecallCandidate &r) {
    return r.initialized?std::optional<double>{r.best.gain}:std::nullopt;
}
std::vector<std::string> risks(const PfsmCandidateRecallCandidate &r,double g) {
    bool distortion=false,boundary=false;
    for(const auto &seed:r.starts) if(!seed.steps.empty()) {
        // Last valid step of each seed, not the historical maximum.
        const auto &step=seed.steps.back();
        distortion |= step.distortion>PfsmCandidateRecallPolicy::distortion*std::max(g,std::abs(r.best.gain));
        boundary |= step.local_boundary && step.actual>PfsmCandidateRecallPolicy::boundary*g;
    }
    std::vector<std::string> out;
    if(distortion)out.push_back("model_distortion");
    if(boundary)out.push_back("local_boundary");
    if(r.eta && *r.eta<PfsmCandidateRecallPolicy::correlation)out.push_back("high_correlation");
    if(!r.numerical_protection.empty())out.push_back("numerical_protection");
    return out;
}
}
Status validatePfsmCandidateRecallLimits(const PfsmCandidateRecallLimits &limits) {
    const auto invalid = [](const char *field, const char *message) {
        return Status{Error{ErrorCode::invalid_input, std::string("pfsm.candidate_recall.") + field, message}};
    };
    if (limits.recall_candidates < 1)
        return invalid("recallCandidates", "Recall candidates must be a positive integer.");
    if (limits.local_shortlist < 1 || limits.local_shortlist > limits.recall_candidates)
        return invalid("localShortlist", "Local shortlist must be positive and no greater than recall candidates.");
    if (limits.protected_per_channel < 1 || limits.protected_per_channel > limits.local_shortlist)
        return invalid("protectedPerChannel", "Channel protection must be positive and no greater than the local shortlist.");
    if (limits.review_limit < limits.local_shortlist)
        return invalid("reviewLimit", "Review threshold must be at least the local shortlist.");
    if (limits.formal_limit < limits.local_shortlist)
        return invalid("formalLimit", "Formal-set threshold must be at least the local shortlist.");
    return Status{std::monostate{}};
}
PfsmCandidateRecallSelection preparePfsmCandidateRecallReview(Rows &rows, const PfsmCandidateRecallLimits &limits) {
    PfsmCandidateRecallSelection out;
    out.limits = limits;
    const auto ranked=rank(rows,initial);
    if(ranked.size()>=static_cast<std::size_t>(limits.local_shortlist))
        out.g_star=std::max(*rows[ranked[limits.local_shortlist-1]].initial_gain,PfsmCandidateRecallPolicy::gain_floor);
    out.initial_top=top(rows,ranked,limits.local_shortlist,initial);
    for(auto &r:rows) {r.initial_rank.reset();r.local_rank.reset();r.review_reasons.clear();r.retention_reasons.clear();}
    for(std::size_t i=0;i<ranked.size();++i)rows[ranked[i]].initial_rank=i+1;
    const auto a=[](const auto &r){return r.recall.original_score;};
    const auto b=[](const auto &r){return r.recall.residual_score;};
    out.channel=top(rows,rank(rows,a),limits.protected_per_channel,a);
    const auto residual=top(rows,rank(rows,b),limits.protected_per_channel,b);
    out.channel.insert(out.channel.end(),residual.begin(),residual.end());ordered(out.channel);
    for(auto &r:rows) {
        const auto i=r.recall.original_index;
        if(contains(out.initial_top,i))r.review_reasons.push_back(limits.local_shortlist == 20 ? "initial_top20" : "initial_top_k");
        if(contains(out.channel,i))r.review_reasons.push_back("channel_protection");
        const auto risk=risks(r,out.g_star);
        r.review_reasons.insert(r.review_reasons.end(),risk.begin(),risk.end());
        if(!r.review_reasons.empty())out.review.push_back(i);
    }
    ordered(out.review);
    if(out.review.size()>static_cast<std::size_t>(limits.review_limit)) {
        out.review_skipped=out.full_fallback=true;
        out.fallback_reason=limits.review_limit == 80 ? "review_set_over_80" : "review_set_over_limit";
    }
    return out;
}
void finalizePfsmCandidateRecall(Rows &rows,PfsmCandidateRecallSelection &out,std::span<const std::size_t> pool) {
    const auto &limits = out.limits;
    const auto ranked=rank(rows,gain);
    out.local_top=top(rows,ranked,limits.local_shortlist,gain);
    for(auto &r:rows) {r.local_rank.reset();r.retention_reasons.clear();}
    for(std::size_t i=0;i<ranked.size();++i)rows[ranked[i]].local_rank=i+1;
    out.protected_indices.clear();out.proposed.clear();
    for(auto &r:rows) {
        const auto i=r.recall.original_index;
        if(contains(out.local_top,i))r.retention_reasons.push_back(limits.local_shortlist == 20 ? "local_top20" : "local_top_k");
        if(contains(out.channel,i))r.retention_reasons.push_back("channel_protection");
        const auto risk=risks(r,out.g_star);
        r.retention_reasons.insert(r.retention_reasons.end(),risk.begin(),risk.end());
        if(!risk.empty())out.protected_indices.push_back(i);
        if(!r.retention_reasons.empty())out.proposed.push_back(i);
    }
    ordered(out.proposed);ordered(out.protected_indices);
    if(!out.full_fallback && out.proposed.size()>static_cast<std::size_t>(limits.formal_limit)) {
        out.full_fallback=true;
        out.fallback_reason=limits.formal_limit == 80 ? "formal_set_over_80" : "formal_set_over_limit";
    }
    out.retained=out.proposed;
    if(out.full_fallback) {
        out.retained.assign(pool.begin(),pool.end());ordered(out.retained);
        for(auto &r:rows)r.retention_reasons.push_back("full_fallback");
    }
}
bool pfsmCandidateRecallUnderestimated(const PfsmCandidateRecallCandidate &r,double gStar,double formalGain) {
    return std::isfinite(formalGain) && formalGain-r.best.gain>
        PfsmCandidateRecallPolicy::underestimation*std::max(formalGain,gStar);
}
} // namespace qualpf::profile
