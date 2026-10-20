#include "pfsm_sampling.h"
#include "qualpf/profile/pfsm_candidate_recall.h"
#include <numeric>
#include <algorithm>
#include <cmath>
#include <limits>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::profile;
namespace {
void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) throw std::runtime_error("Fast PFSM contract at " + std::to_string(at.line()));
}
void near(double a, double b, double tol = 2e-11) { check(std::abs(a-b) <= tol * std::max(1., std::abs(b))); }
template<class T> T take(Result<T> r) {
    if (!r) throw std::runtime_error(r.error().message);
    return std::move(r).value();
}
double phi(double x, double h) { return std::pow(1+4*(std::pow(2.,2./3)-1)*(x/h)*(x/h),-1.5); }
ExecutionContext context() { return {{},NumericalPolicy::explicit_float64,{}}; }
ProfileExpansionInput reference(std::vector<ProfileLine> lines, double h = 4) {
    ProfileExpansionInput p; p.coordinates=ProfileExpansionInput::Coordinates::two_theta_degrees;
    p.fwhm_degrees=h; p.lines=std::move(lines); return p;
}
}
void candidateRecallContracts() {
    auto o=std::make_shared<ProfileObservations>();
    o->coordinates={0, .17, 1.9, 30, 30.5, 32, 90.31, 179.7};
    o->observed={1, -4, 3, 8, 6, 5, -1, 2};
    o->background={0, 1, 1, 2, 1, 1, 0, 0};
    o->weights={1, 2, 0, .25, 3, .5, 1.5, 2};
    const auto d=take(preparePfsmCandidateRecallData(o,4,-.5,.5,context()));
    double wsum=0, vsum=0;
    for(std::size_t i=0;i<o->weights.size();++i) {
        wsum+=o->weights[i]; vsum+=o->weights[i]*(o->observed[i]-o->background[i]);
    }
    double dw=0,dv=0; for(double w:d->weights)dw+=w;for(double v:d->weighted_signal)dv+=v;
    near(dw,wsum);near(dv,vsum);
    // Direct finite-distance correlation: endpoints expose circular wraparound;
    // unequal weights and negative signal expose sqrt(w) and clipping mutants.
    for(std::size_t j=0;j<d->weights.size();++j) {
        double f=0,g=0;
        for(std::size_t i=0;i<d->weights.size();++i) {
            const double p=phi((double(i)-double(j))*d->step,4);
            f+=d->weighted_signal[i]*p; g+=d->weights[i]*p*p;
        }
        near(d->original_response[j],f);near(d->energy_response[j],g);
    }
    std::vector<double> residual;
    for(std::size_t i=0;i<o->observed.size();++i) residual.push_back(o->observed[i]-o->background[i]);
    const auto round=take(preparePfsmCandidateRecallRound(d,residual,{},context()));
    const auto p=reference({{0,30,1},{1,30,2},{2,31,.4}});
    const auto merged=reference({{0,30,3},{2,31,.4}});
    auto a=take(scorePfsmCandidateRecallCandidate(round,p,0,context()));
    auto b=take(scorePfsmCandidateRecallCandidate(round,merged,1,context()));
    near(*a.original_score,*b.original_score); near(*a.residual_score,*b.residual_score);
    const auto zero=scorePfsmCandidateRecallCandidate(round,reference({{0,30,0}}),2,context());
    check(!zero && zero.error().code==ErrorCode::numerical_failure);
    auto overflow=std::make_shared<PfsmCandidateRecallData>(*d);
    std::fill(overflow->original_response.begin(),overflow->original_response.end(),-std::numeric_limits<double>::max()/2);
    auto overflowRound=round; overflowRound.data=overflow;
    const auto invalidCorrelation=scorePfsmCandidateRecallCandidate(overflowRound,p,3,context());
    check(!invalidCorrelation && invalidCorrelation.error().code==ErrorCode::numerical_failure);
    const auto native=take(preparePfsmCandidateRecallPhysicalRound(d,residual,{},context(),false));
    const auto local=take(evaluatePfsmCandidateRecallInitial(native,p,a,context()));
    check(local.best.gain>0 && local.best.q<=native.baseline_q);
    // Two identical columns must describe the summed full template, not diagonal energy.
    auto q=std::make_shared<ProfileObservations>();
    for(int i=0;i<=32;++i) {
        const double x=22+i*.5;
        q->coordinates.push_back(x);q->observed.push_back(3*phi(x-30,4));
        q->background.push_back(0);q->weights.push_back(1);
    }
    auto exact=take(preparePfsmCandidateRecallData(q,4,0,0,context()));
    auto er=take(preparePfsmCandidateRecallRound(exact,q->observed,{},context()));
    auto ep=reference({{0,30,1},{1,30,1}});
    auto ee=take(scorePfsmCandidateRecallCandidate(er,ep,0,context()));
    auto physical=take(preparePfsmCandidateRecallPhysicalRound(exact,q->observed,{},context(),false));
    auto ef=take(evaluatePfsmCandidateRecallInitial(physical,ep,ee,context()));
    near(ef.best.q,0);near(ef.best.gain,physical.baseline_q);
    // A fully negative target cannot gain from a nonnegative template.
    for(auto &y:q->observed)y=-y;
    exact=take(preparePfsmCandidateRecallData(q,4,0,0,context()));
    er=take(preparePfsmCandidateRecallRound(exact,q->observed,{},context()));
    ee=take(scorePfsmCandidateRecallCandidate(er,ep,0,context()));
    near(*ee.original_score,0);near(*ee.residual_score,0);
    physical=take(preparePfsmCandidateRecallPhysicalRound(exact,q->observed,{},context(),false));
    near(take(evaluatePfsmCandidateRecallInitial(physical,ep,ee,context())).best.gain,0);
    // Fixed budgets, duplicate-free alternating channels, stable original order,
    // cutoff ties and unconditionally protected failures.
    std::vector<PfsmCandidateRecallEvidence> rows(600);
    for(std::size_t i=0;i<rows.size();++i) {
        rows[i].original_index=i;rows[i].original_score=double(600-i);
        rows[i].residual_score=double(600-i);rows[i].gain=double(600-i);
    }
    check(selectPfsmRecallCandidates(rows).size()==500);
    check(rows[0].original_rank==1 && rows[0].residual_rank==1 && rows.back().original_rank==600);
    check(rows[0].recall_reason=="original_channel" && rows[1].recall_reason=="residual_channel");
    rows[500].original_score=rows[499].original_score;
    rows[500].residual_score=rows[499].residual_score;
    check(selectPfsmRecallCandidates(rows).size()==501 && rows[500].recall_reason=="residual_cutoff_tie");
    rows[500].original_score=rows[500].residual_score=100.;
    check(selectPfsmRecallCandidates(rows).size()==500);
    check(selectPfsmRecallAblationCandidates(rows).size()==20);
    check(rows[19].local_rank==20 && rows[19].retention_reason=="local_rank_budget");
    rows[20].gain=rows[19].gain;
    check(selectPfsmRecallAblationCandidates(rows).size()==21);
    check(rows[20].retention_reason=="local_cutoff_tie");
    rows.back().protection_reason="fixture";
    check(selectPfsmRecallCandidates(rows).size()==501);
    check(selectPfsmRecallAblationCandidates(rows).size()==22);
    check(rows.back().recall_reason=="numerical_protection" && rows.back().retention_reason=="numerical_protection");
    check(!preparePfsmCandidateRecallData(o,1e-8,-.5,.5,context()));
    check(take(preparePfsmCandidateRecallData(o,4,0,4095,context()))->scan.size()==4096);
    check(!preparePfsmCandidateRecallData(o,4,0,4096,context()));
    std::stop_source stop;stop.request_stop();auto cancelled=context();cancelled.cancellation=stop.get_token();
    auto failure=preparePfsmCandidateRecallData(o,4,-.5,.5,cancelled);
    check(!failure && failure.error().code==ErrorCode::cancelled);
}

void candidateRecallBackprojectionContracts() {
    auto o=std::make_shared<ProfileObservations>();
    const auto old=reference({{0,30,1}}),candidate=reference({{0,32,1}});
    std::vector<double> residual;
    for(int i=0;i<81;++i) {
        const double x=20+i*.25;
        o->coordinates.push_back(x);o->weights.push_back(i%7?(.2+i%5):0);
        o->background.push_back(2);
        const double y=2*phi(x-30,4)+.7*phi(x-32-.2,4);
        o->observed.push_back(y+2);residual.push_back(y-2*phi(x-30,4));
    }
    const auto data=take(preparePfsmCandidateRecallData(o,4,-1,1,context()));
    const std::vector<PfsmCandidateRecallPhysicalPhase> selected{{old,2,0}};
    const auto round=take(preparePfsmCandidateRecallPhysicalRound(data,residual,selected,context()));
    auto score=take(scorePfsmCandidateRecallCandidate(round.recall,candidate,0,context()));
    const auto result=take(fitPfsmCandidateRecallBackprojection(round,candidate,score,context()));
    check(result.best.gain>0 && !result.best.baseline_fallback);
    check(result.nnls_calls==2*result.trials.size());
    for(const auto &trial:result.trials) {
        const auto &f=trial.physical;
        near(f.gain,round.baseline_q-f.q);
        check(f.parameters.scales.size()==2 && f.parameters.shifts.size()==2);
        for(double shift:f.parameters.shifts)check(shift>=-1 && shift<=1);
        for(double scale:f.parameters.scales)check(scale>=0);
    }
    auto wrong=selected;wrong[0].scale=3;
    check(!preparePfsmCandidateRecallPhysicalRound(data,residual,wrong,context()));
    std::stop_source stop;stop.request_stop();auto cancelled=context();cancelled.cancellation=stop.get_token();
    auto failure=fitPfsmCandidateRecallBackprojection(round,candidate,score,cancelled);
    check(!failure && failure.error().code==ErrorCode::cancelled);
    // A negative target has a real zero-template feasible solution, not a clipped score.
    for(auto &y:o->observed)y=-1;
    const auto negative=take(preparePfsmCandidateRecallData(o,4,-1,1,context()));
    std::vector<double> net(o->observed.size(),-3);
    const auto nr=take(preparePfsmCandidateRecallPhysicalRound(negative,net,{},context()));
    score=take(scorePfsmCandidateRecallCandidate(nr.recall,candidate,0,context()));
    const auto nf=take(fitPfsmCandidateRecallBackprojection(nr,candidate,score,context()));
    check(nf.best.baseline_fallback && nf.best.gain==0 && nf.best.parameters.scales.back()==0);
}

void candidateRecallAnchoredContracts() {
    const auto ctx=context();IntermediateLorentzianExpander expander;
    auto input=reference({{0,30,1},{1,32,.3}});
    input.interpolation_window=std::pair{18.,44.};
    auto cached=take(expander.prepare(input,ctx));
    std::vector<double> x;for(int i=0;i<125;++i)x.push_back(20+i*.17);
    for(double shift:{-1.,0.,.23,1.}) {
        const auto sampled=take(evaluateProfilePairedSamples(cached,x,shift,ctx));
        const auto ordinary=take(cached->evaluate(x,shift,false,ctx));
        for(std::size_t i=0;i<x.size();++i) {
            double value=0,derivative=0;
            for(const auto &line:input.lines) {
                const double distance=x[i]-line.position-shift;
                const double a=4*(std::pow(2.,2./3)-1)/16;
                value+=line.intensity*std::pow(1+a*distance*distance,-1.5);
                derivative+=line.intensity*3*a*distance*std::pow(1+a*distance*distance,-2.5);
            }
            near(sampled.intensity[i],value);near(sampled.intensity[i],ordinary.intensity[i]);
            near(sampled.shift_derivative[i],derivative,2e-10);
        }
    }
    auto o=std::make_shared<ProfileObservations>();o->coordinates=x;
    for(std::size_t i=0;i<x.size();++i){o->observed.push_back(2*phi(x[i]-30-.3,4)+.6*phi(x[i]-32-.3,4));o->background.push_back(0);o->weights.push_back(i%7?1.:0.);}
    auto data=take(preparePfsmCandidateRecallData(o,4,-1,1,ctx));
    auto round=take(preparePfsmCandidateRecallPhysicalRound(data,o->observed,{},ctx,false));
    auto a=take(scorePfsmCandidateRecallCandidate(round.recall,input,0,ctx));
    auto b=take(scorePfsmCandidateRecallCandidate(round.recall,input,0,ctx,true));
    check(a.original_score==b.original_score && a.residual_score==b.residual_score && a.original_shift==b.original_shift);
    check(!b.original_seeds.empty() && b.original_seeds.size()<=3);
    for(std::size_t i=0;i<b.original_seeds.size();++i)for(std::size_t j=0;j<i;++j)
        check(std::abs(b.original_seeds[i]-b.original_seeds[j])>=1);
    auto initial=take(evaluatePfsmCandidateRecallInitial(round,input,b,ctx));
    check(initial.initialized && initial.nnls_calls==3 && initial.numerical_protection.empty());
    auto reviewed=take(reviewPfsmCandidateRecall(round,input,initial,ctx));
    check(reviewed.reviewed && reviewed.nnls_calls<=15 && reviewed.numerical_protection.empty());
    check(reviewed.best.gain>=initial.best.gain && reviewed.best.q<=initial.best.q);
    for(const auto &start:reviewed.starts) {
        check(start.steps.size()==2 && !start.initial_fit.baseline_fallback);
        near(start.initial_fit.gain,round.baseline_q-start.initial_fit.q);
        check(start.initial_fit.parameters.shifts.back()==start.initial_shift);
        check(start.steps[1].radius==(start.steps[0].accepted?.125:.0625));
        for(const auto &step:start.steps) {
            check(step.physical.parameters.shifts.back()>=-1 && step.physical.parameters.shifts.back()<=1);
            if(step.accepted)check(step.actual>0 && step.ratio && *step.ratio>=.1);
        }
    }
    std::stop_source stop;stop.request_stop();auto cancelled=ctx;cancelled.cancellation=stop.get_token();
    auto failure=reviewPfsmCandidateRecall(round,input,initial,cancelled);
    check(!failure && failure.error().code==ErrorCode::cancelled);
    check(!reviewPfsmCandidateRecall(round,input,reviewed,ctx));
    auto zero=reference({{0,30,0}});auto protectedZero=take(evaluatePfsmCandidateRecallInitial(round,zero,b,ctx));
    check(!protectedZero.numerical_protection.empty() && protectedZero.best.baseline_fallback);
}

void candidateRecallProtectionContracts() {
    // Independent ordered scores exercise configurable counts, not fitted-product goldens.
    const PfsmCandidateRecallLimits custom{5,3,1,4,4};
    check(bool(validatePfsmCandidateRecallLimits(custom)));
    for (auto bad : {PfsmCandidateRecallLimits{0,3,1,4,4}, {-1,3,1,4,4}, {2,3,1,4,4},
                     {5,0,1,4,4}, {5,3,0,4,4}, {5,3,4,4,4}, {5,3,1,2,4}, {5,3,1,4,2}})
        check(!validatePfsmCandidateRecallLimits(bad));
    std::vector<PfsmCandidateRecallEvidence> scores(10);
    std::vector<PfsmCandidateRecallCandidate> small(10);
    std::vector<std::size_t> smallPool(10);std::iota(smallPool.begin(),smallPool.end(),0);
    for (std::size_t i=0;i<10;++i) {
        scores[i].original_index=i;scores[i].original_score=100.-i;scores[i].residual_score=i+1.;
        small[i].recall=scores[i];small[i].recall.recalled=true;
        small[i].initialized=true;small[i].initial_gain=small[i].best.gain=100.-i;small[i].eta=.5;
    }
    check(selectPfsmRecallCandidates(scores,custom)==std::vector<std::size_t>({0,1,2,8,9}));
    scores[5].protection_reason="independent protection";
    scores[3].original_score=98.;
    check(selectPfsmRecallCandidates(scores,custom)==std::vector<std::size_t>({0,1,2,3,5,8,9}));
    auto local=preparePfsmCandidateRecallReview(small,custom);
    check(local.limits==custom && local.g_star==98. && local.review==std::vector<std::size_t>({0,1,2,9}));
    check(!local.review_skipped); // Exactly the threshold does not trigger full fallback.
    finalizePfsmCandidateRecall(small,local,smallPool);
    check(!local.full_fallback && local.retained==std::vector<std::size_t>({0,1,2,9}));
    check(small[1].retention_reasons==std::vector<std::string>{"local_top_k"});
    small[8].eta=0;
    finalizePfsmCandidateRecall(small,local,smallPool);
    check(local.proposed.size()==5 && local.retained==smallPool && local.fallback_reason=="formal_set_over_limit");
    local=preparePfsmCandidateRecallReview(small,custom);
    check(local.review_skipped && local.fallback_reason=="review_set_over_limit");
    finalizePfsmCandidateRecall(small,local,smallPool);check(local.retained==smallPool);
    small[8].eta=.5;small[3].initial_gain=small[3].best.gain=98.;
    local=preparePfsmCandidateRecallReview(small,{5,3,1,10,10});
    finalizePfsmCandidateRecall(small,local,smallPool);
    check(local.local_top==std::vector<std::size_t>({0,1,2,3}) && local.retained.size()==5);
    const int largest=std::numeric_limits<int>::max();
    const PfsmCandidateRecallLimits huge{largest,largest,largest,largest,largest};
    check(bool(validatePfsmCandidateRecallLimits(huge)));
    local=preparePfsmCandidateRecallReview(small,huge);finalizePfsmCandidateRecall(small,local,smallPool);
    check(local.retained==smallPool && selectPfsmRecallCandidates(scores,huge)==smallPool);

    std::vector<PfsmCandidateRecallCandidate> rows(100);
    std::vector<std::size_t> pool(600);std::iota(pool.begin(),pool.end(),0);
    for(std::size_t i=0;i<rows.size();++i) {
        auto &r=rows[i];r.recall.original_index=i;r.recall.recalled=true;
        r.recall.original_score=double(100-i);r.recall.residual_score=double(i+1);
        r.initialized=true;r.initial_gain=r.best.gain=double(100-i);r.eta=.5;
    }
    auto selection=preparePfsmCandidateRecallReview(rows);
    check(selection.g_star==81 && selection.review.size()==25 && !selection.review_skipped);
    // A formerly hidden candidate is retained by channel evidence without changing the local rank.
    finalizePfsmCandidateRecall(rows,selection,pool);
    check(selection.local_top.size()==20 && selection.retained.size()==25 && rows[99].local_rank==100);
    check(rows[99].retention_reasons==std::vector<std::string>{"channel_protection"});
    // The current valid states replace stale risk; a numeric failure remains protected.
    rows[40].maximum_distortion=1e8;rows[40].starts.resize(1);rows[40].starts[0].steps.resize(2);
    rows[40].starts[0].steps[0].distortion=1e8;rows[40].starts[0].steps[1].distortion=0;
    rows[41].starts.resize(1);rows[41].starts[0].steps.resize(1);
    rows[41].starts[0].steps[0].local_boundary=true;rows[41].starts[0].steps[0].actual=5;
    rows[42].eta=1e-4;rows[43].numerical_protection="fixture failure";
    selection=preparePfsmCandidateRecallReview(rows);finalizePfsmCandidateRecall(rows,selection,pool);
    check(rows[40].retention_reasons.empty());check(!rows[41].retention_reasons.empty());
    check(!rows[42].retention_reasons.empty() && !rows[43].retention_reasons.empty());
    // More than 80 risks cannot be truncated, and must not be counted as local repair.
    for(auto &r:rows)r.eta=0;
    selection=preparePfsmCandidateRecallReview(rows);finalizePfsmCandidateRecall(rows,selection,pool);
    check(selection.review_skipped && selection.review.size()==100 && selection.retained==pool);
    check(selection.local_top.size()==20 && selection.fallback_reason=="review_set_over_80");
    for(auto &r:rows){r.eta=.5;r.starts.clear();r.numerical_protection.clear();}
    for(std::size_t i=0;i<75;++i)rows[i].eta=0;
    selection=preparePfsmCandidateRecallReview(rows);check(selection.review.size()==80 && !selection.full_fallback);
    // New unresolved risks after review independently exercise the formal-set limit.
    rows[75].eta=0;finalizePfsmCandidateRecall(rows,selection,pool);
    check(selection.proposed.size()==81 && selection.full_fallback && selection.fallback_reason=="formal_set_over_80");
    // Cutoff ties are preserved; storage order cannot choose winners.
    for(auto &r:rows)r.eta=.5;
    rows[20].initial_gain=rows[20].best.gain=81;
    selection=preparePfsmCandidateRecallReview(rows);finalizePfsmCandidateRecall(rows,selection,pool);
    check(selection.local_top.size()==21);
    const auto expected=selection.retained;
    std::reverse(rows.begin(),rows.end());selection=preparePfsmCandidateRecallReview(rows);finalizePfsmCandidateRecall(rows,selection,pool);
    check(selection.retained==expected);
    PfsmCandidateRecallCandidate r;r.best.gain=74;
    check(pfsmCandidateRecallUnderestimated(r,1,100));r.best.gain=75;check(!pfsmCandidateRecallUnderestimated(r,1,100));
}
