#include "pfsm_sampling.h"
#include "qualpf/profile/pfsm_candidate_recall.h"
#include <iomanip>
#include <iostream>
using namespace qualpf;
using namespace qualpf::profile;
template<class T> T take(Result<T> r) {
    if (!r) throw std::runtime_error(r.error().message);
    return std::move(r).value();
}
void vectorJson(const std::vector<double> &v) {
    std::cout<<'[';for(std::size_t i=0;i<v.size();++i){if(i)std::cout<<',';std::cout<<v[i];}std::cout<<']';
}
void fitJson(const PfsmCandidateRecallPhysicalFit &f) {
    std::cout<<"{\"q\":"<<f.q<<",\"gain\":"<<f.gain<<",\"difference_gain\":"<<f.difference_gain
             <<",\"kkt\":"<<f.kkt<<",\"fallback\":"<<(f.baseline_fallback?"true":"false")<<",\"scales\":";
    vectorJson(f.parameters.scales);std::cout<<",\"shifts\":";vectorJson(f.parameters.shifts);std::cout<<'}';
}
int main(int argc,char **argv) try {
    const std::string mode=argc==2?argv[1]:"";
    const bool anchored=mode=="anchored" || mode=="anchored-cache";
    const bool physical=mode=="backprojection" || anchored;
    if(argc!=2 || (!physical && mode!="derivative"))return 2;
    std::size_t n, m; double h, lower, upper;
    if (!(std::cin>>n>>m>>h>>lower>>upper) || n>100000 || m>32) return 2;
    const ExecutionContext ctx{{},NumericalPolicy::explicit_float64,{}};
    auto o=std::make_shared<ProfileObservations>();std::vector<double> residual;
    for(std::size_t i=0;i<n;++i) {
        double x,y,b,w,r;if(!(std::cin>>x>>y>>b>>w>>r))return 2;
        o->coordinates.push_back(x);o->observed.push_back(y);o->background.push_back(b);o->weights.push_back(w);residual.push_back(r);
    }
    std::vector<PfsmCandidateRecallSelected> selected;std::vector<PfsmCandidateRecallPhysicalPhase> phases;ProfileExpansionInput candidate;double candidateShift=0;
    for(std::size_t j=0;j<=m;++j) {
        double scale=0;if(physical && !(std::cin>>scale))return 2;
        double shift;std::size_t count;if(!(std::cin>>shift>>count)||count>10000)return 2;
        ProfileExpansionInput p;p.fwhm_degrees=h;p.coordinates=ProfileExpansionInput::Coordinates::two_theta_degrees;
        for(std::size_t k=0;k<count;++k){double q,y;if(!(std::cin>>q>>y))return 2;p.lines.push_back({k,q,y});}
        if(mode=="anchored-cache" || mode=="derivative")p.interpolation_window=std::pair{o->coordinates.front()-upper,o->coordinates.back()-lower};
        if(j<m){phases.push_back({p,scale,shift});selected.push_back({std::move(p),shift});}
        else {candidate=std::move(p);candidateShift=shift;}
    }
    if(mode=="derivative") {
        IntermediateLorentzianExpander expander;auto profile=take(expander.prepare(candidate,ctx));
        auto samples=take(evaluateProfilePairedSamples(profile,o->coordinates,candidateShift,ctx));
        std::cout<<std::setprecision(17)<<"{\"intensity\":";vectorJson(samples.intensity);
        std::cout<<",\"derivative\":";vectorJson(samples.shift_derivative);std::cout<<"}\n";return 0;
    }
    auto data=take(preparePfsmCandidateRecallData(o,h,lower,upper,ctx));
    auto round=take(preparePfsmCandidateRecallRound(data,residual,selected,ctx));
    auto score=take(scorePfsmCandidateRecallCandidate(round,candidate,0,ctx,anchored));
    if(anchored) {
        auto pr=take(preparePfsmCandidateRecallPhysicalRound(data,residual,phases,ctx,false));
        auto first=take(evaluatePfsmCandidateRecallInitial(pr,candidate,score,ctx));
        auto result=take(reviewPfsmCandidateRecall(pr,candidate,first,ctx));
        std::cout<<std::setprecision(17)<<"{\"baseline_q\":"<<pr.baseline_q<<",\"calls\":"<<result.nnls_calls
            <<",\"protected\":"<<(result.numerical_protection.empty()?"false":"true")<<",\"seeds\":";
        vectorJson(result.seeds);std::cout<<",\"best\":";fitJson(result.best);
        std::cout<<",\"initial_best\":";fitJson(first.best);std::cout<<",\"starts\":[";
        for(std::size_t j=0;j<result.starts.size();++j) {
            if(j)std::cout<<',';const auto &start=result.starts[j];
            std::cout<<"{\"initial_shift\":"<<start.initial_shift<<",\"initial_fit\":";fitJson(start.initial_fit);
            std::cout<<",\"steps\":[";
            for(std::size_t k=0;k<start.steps.size();++k) {
                if(k)std::cout<<',';const auto &t=start.steps[k];
                std::cout<<"{\"radius\":"<<t.radius<<",\"predicted\":"<<t.predicted<<",\"actual\":"<<t.actual
                    <<",\"distortion\":"<<t.distortion<<",\"accepted\":"<<(t.accepted?"true":"false")<<",\"anchor_scales\":";
                vectorJson(t.anchor.scales);std::cout<<",\"anchor_shifts\":";vectorJson(t.anchor.shifts);
                std::cout<<",\"coefficients\":";vectorJson(t.linear_coefficients);std::cout<<",\"physical\":";
                fitJson(t.physical);std::cout<<'}';
            }
            std::cout<<"]}";
        }
        std::cout<<"]}\n";return 0;
    }
    if(physical) {
        auto pr=take(preparePfsmCandidateRecallPhysicalRound(data,residual,phases,ctx));
        auto result=take(fitPfsmCandidateRecallBackprojection(pr,candidate,score,ctx));
        std::cout<<std::setprecision(17)<<"{\"baseline_q\":"<<pr.baseline_q<<",\"calls\":"<<result.nnls_calls<<",\"best\":";
        fitJson(result.best);std::cout<<",\"trials\":[";
        for(std::size_t i=0;i<result.trials.size();++i) {
            if(i)std::cout<<',';const auto &t=result.trials[i];
            std::cout<<"{\"initial_shift\":"<<t.candidate_initial_shift<<",\"linear_q\":"<<t.linear_q<<",\"coefficients\":";vectorJson(t.coefficients);
            std::cout<<",\"physical\":";fitJson(t.physical);std::cout<<'}';
        }
        std::cout<<"]}\n";return 0;
    }
    return 2; // Historical v1 transport is available from its frozen Git snapshot.
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
