#include "pfsm_sampling.h"
#include "qualpf/profile/pfsm_candidate_recall.h"
#include "pfsm_internal.h"

namespace qualpf::profile {
using detail::Matrix;
using detail::Vector;
struct PfsmCandidateRecallPhysicalProblem {
    Vector z, sqrt_weights, residual, baseline_model;
    std::vector<double> coordinates;
    std::vector<PfsmCandidateRecallPhysicalPhase> selected;
    std::vector<ProfileTemplate> profiles;
    Matrix baseline_columns, baseline_derivatives, orthogonal_basis;
    double denominator = 0;
    bool anchored = false;
};
Result<PfsmCandidateRecallBackprojectionResult> backprojectionProposals(const PfsmCandidateRecallRound &,
    std::span<const PfsmCandidateRecallSelected>, const ProfileExpansionInput &, const PfsmCandidateRecallEvidence &,
    const ExecutionContext &);
namespace {
template<class T> T take(Result<T> r) {
    if(!r) throw detail::Failure{r.error()};
    return std::move(r).value();
}
[[noreturn]] void numerical(const char *message) {
    throw detail::Failure{Error{ErrorCode::numerical_failure,"pfsm.fast.v2",message}};
}
double threshold(double a,double b,double c) {
    return 128*std::numeric_limits<double>::epsilon()*std::max({1.,a,b,c});
}
double stableGain(const Vector &r,const Vector &change) {
    double sum=0, correction=0;
    for(Eigen::Index i=0;i<r.size();++i) {
        const double term=change[i]*(2*r[i]-change[i]);
        const double adjusted=term-correction;
        const double next=sum+adjusted;
        correction=(next-sum)-adjusted; sum=next;
    }
    if(!std::isfinite(sum)) numerical("Nonfinite physical gain");
    return sum;
}
Vector physicalColumn(const PfsmCandidateRecallPhysicalProblem &p,const ProfileTemplate &profile,
                      double shift,const ExecutionContext &ctx) {
    auto samples=take(p.anchored ? evaluateProfilePairedSamples(profile,p.coordinates,shift,ctx)
                                 : profile->evaluate(p.coordinates,shift,false,ctx));
    if(samples.intensity.size()!=p.coordinates.size()) numerical("Unaligned physical template");
    Vector result=detail::vectorOf(samples.intensity).cwiseProduct(p.sqrt_weights);
    if(!result.allFinite()) numerical("Nonfinite physical template");
    return result;
}
PfsmCandidateRecallPhysicalFit fallback(const PfsmCandidateRecallPhysicalRound &round,double candidateShift) {
    PfsmCandidateRecallPhysicalFit out;out.q=round.baseline_q;
    for(const auto &phase:round.physical->selected) {
        out.parameters.scales.push_back(phase.scale);out.parameters.shifts.push_back(phase.shift);
    }
    out.parameters.scales.push_back(0);out.parameters.shifts.push_back(candidateShift);
    return out;
}
PfsmCandidateRecallPhysicalFit fitPhysical(const PfsmCandidateRecallPhysicalRound &round,
    const std::vector<ProfileTemplate> &profiles,const std::vector<double> &shifts,
    std::size_t &calls,std::size_t &iterations,detail::NnlsWorkspace &workspace,const ExecutionContext &ctx) {
    detail::checkpoint(ctx);const auto &p=*round.physical;
    Matrix columns(p.z.size(),static_cast<Eigen::Index>(profiles.size()));
    for(std::size_t j=0;j<profiles.size();++j) {
        if(j<p.selected.size() && shifts[j]==p.selected[j].shift)
            columns.col(static_cast<Eigen::Index>(j))=p.baseline_columns.col(static_cast<Eigen::Index>(j));
        else columns.col(static_cast<Eigen::Index>(j))=physicalColumn(p,profiles[j],shifts[j],ctx);
    }
    workspace.resize(columns.rows(),columns.cols());
    ++calls;auto solved=detail::nnls(columns,p.z,0,workspace,ctx);iterations+=solved.iterations;
    PfsmCandidateRecallPhysicalFit out;out.parameters.scales=detail::valuesOf(solved.scales);out.parameters.shifts=shifts;
    out.q=solved.residual.squaredNorm();out.kkt=solved.kkt;out.baseline_fallback=false;
    out.approximated=std::any_of(profiles.begin(),profiles.end(),[](const auto &v){return v->usesApproximation();});
    const Vector model=columns*solved.scales;
    out.gain=stableGain(p.residual,model-p.baseline_model);
    out.difference_gain=round.baseline_q-out.q;
    if(!std::isfinite(out.q) || std::abs(out.gain-out.difference_gain)>threshold(round.baseline_q,out.q,0))
        numerical("Physical gain identity failed");
    return out;
}
} // namespace
Result<PfsmCandidateRecallPhysicalRound> preparePfsmCandidateRecallPhysicalRound(
    std::shared_ptr<const PfsmCandidateRecallData> data,std::span<const double> residual,
    std::span<const PfsmCandidateRecallPhysicalPhase> selected,const ExecutionContext &ctx,bool backprojectionDictionary) try {
    detail::checkpoint(ctx);
    if(!data || !data->observations || residual.size()!=data->observations->coordinates.size())
        detail::fail("pfsm.fast.v2","Invalid physical round");
    auto p=std::make_shared<PfsmCandidateRecallPhysicalProblem>();p->denominator=data->denominator;
    p->selected.assign(selected.begin(),selected.end());
    std::vector<PfsmCandidateRecallSelected> legacy;
    IntermediateLorentzianExpander expander;
    for(const auto &phase:selected) {
        if(!std::isfinite(phase.scale) || phase.scale<0 || !std::isfinite(phase.shift) ||
           phase.shift<data->lower_shift || phase.shift>data->upper_shift)
            detail::fail("pfsm.fast.v2","Invalid physical baseline parameters");
        legacy.push_back({phase.input,phase.shift});
        p->profiles.push_back(take(expander.prepare(phase.input,ctx)));
    }
    const auto &o=*data->observations;
    std::vector<double> z,sw,rr;
    const double norm=std::sqrt(p->denominator);
    for(std::size_t i=0;i<residual.size();++i) if(o.weights[i]>0) {
        if(!std::isfinite(residual[i])) detail::fail("pfsm.fast.v2","Invalid physical residual");
        p->coordinates.push_back(o.coordinates[i]);sw.push_back(std::sqrt(o.weights[i])/norm);
        z.push_back(sw.back()*(o.observed[i]-o.background[i]));rr.push_back(sw.back()*residual[i]);
    }
    p->z=detail::vectorOf(z);p->sqrt_weights=detail::vectorOf(sw);p->residual=detail::vectorOf(rr);
    p->baseline_model=p->z-p->residual;
    p->baseline_columns.resize(p->z.size(),static_cast<Eigen::Index>(selected.size()));
    Vector explicitModel=Vector::Zero(p->z.size());
    for(std::size_t j=0;j<selected.size();++j) {
        auto samples=take(p->profiles[j]->evaluateExact(p->coordinates,selected[j].shift,false,ctx));
        p->baseline_columns.col(static_cast<Eigen::Index>(j))=detail::vectorOf(samples.intensity).cwiseProduct(p->sqrt_weights);
        explicitModel+=selected[j].scale*p->baseline_columns.col(static_cast<Eigen::Index>(j));
    }
    if(!p->z.allFinite() || !p->residual.allFinite() || !explicitModel.allFinite() ||
       (explicitModel-p->baseline_model).norm()>threshold(1.,explicitModel.norm(),p->baseline_model.norm()))
        numerical("Physical baseline parameters and residual disagree");
    PfsmCandidateRecallPhysicalRound out;
    if(!backprojectionDictionary) {
        p->anchored=true;
        p->baseline_derivatives.resize(p->z.size(),static_cast<Eigen::Index>(selected.size()));
        std::vector<Vector> nonzero;
        for(std::size_t j=0;j<selected.size();++j) {
            auto samples=take(evaluateProfilePairedSamples(p->profiles[j],p->coordinates,selected[j].shift,ctx));
            p->baseline_columns.col(static_cast<Eigen::Index>(j))=
                detail::vectorOf(samples.intensity).cwiseProduct(p->sqrt_weights);
            p->baseline_derivatives.col(static_cast<Eigen::Index>(j))=
                detail::vectorOf(samples.shift_derivative).cwiseProduct(p->sqrt_weights)*data->width;
            const double norm=p->baseline_columns.col(static_cast<Eigen::Index>(j)).stableNorm();
            if(norm>0)nonzero.push_back(p->baseline_columns.col(static_cast<Eigen::Index>(j))/norm);
        }
        p->orthogonal_basis.resize(p->z.size(),0);
        if(!nonzero.empty()) {
            Matrix normalized(p->z.size(),static_cast<Eigen::Index>(nonzero.size()));
            for(std::size_t j=0;j<nonzero.size();++j)normalized.col(static_cast<Eigen::Index>(j))=nonzero[j];
            Eigen::JacobiSVD<Matrix> svd(normalized,Eigen::ComputeThinU);
            svd.setThreshold(std::numeric_limits<double>::epsilon()*double(std::max(normalized.rows(),normalized.cols())));
            p->orthogonal_basis=svd.matrixU().leftCols(svd.rank());
        }
    }
    out.recall=take(preparePfsmCandidateRecallRound(std::move(data),residual,legacy,ctx,backprojectionDictionary));
    out.baseline_q=p->residual.squaredNorm();out.physical=std::move(p);
    return Result<PfsmCandidateRecallPhysicalRound>{std::move(out)};
} catch(const detail::Failure &f) {return Result<PfsmCandidateRecallPhysicalRound>{f.error};}

Result<PfsmCandidateRecallBackprojectionResult> fitPfsmCandidateRecallBackprojection(const PfsmCandidateRecallPhysicalRound &round,
    const ProfileExpansionInput &input,const PfsmCandidateRecallEvidence &e,const ExecutionContext &ctx) try {
    if(!round.physical) detail::fail("pfsm.fast.v2","Missing physical baseline");
    std::vector<PfsmCandidateRecallSelected> legacy;
    for(const auto &phase:round.physical->selected) legacy.push_back({phase.input,phase.shift});
    auto out=take(backprojectionProposals(round.recall,legacy,input,e,ctx));
    auto profiles=round.physical->profiles;
    IntermediateLorentzianExpander expander;profiles.push_back(take(expander.prepare(input,ctx)));
    const double initial=(e.recall_reason=="residual_channel" || e.recall_reason=="residual_cutoff_tie")
        ?e.residual_shift:e.original_shift;
    out.best=fallback(round,initial);
    detail::NnlsWorkspace workspace;
    for(auto &trial:out.trials) {
        detail::checkpoint(ctx);std::vector<double> shifts;std::size_t offset=0;
        for(std::size_t j=0;j<trial.column_shifts.size();++j) {
            const auto &group=trial.column_shifts[j];
            double largest=0;for(std::size_t k=0;k<group.size();++k)largest=std::max(largest,trial.coefficients[offset+k]);
            double total=0,position=0;
            if(largest>0) for(std::size_t k=0;k<group.size();++k) {
                const double coefficient=trial.coefficients[offset+k]/largest;
                total+=coefficient;position+=coefficient*group[k];
            }
            const double unchanged=j<legacy.size()?legacy[j].shift:trial.candidate_initial_shift;
            shifts.push_back(largest>0?std::clamp(position/total,group.front(),group.back()):unchanged);
            offset+=group.size();
        }
        trial.physical=fitPhysical(round,profiles,shifts,out.nnls_calls,out.nnls_iterations,workspace,ctx);
        if(trial.physical.gain>out.best.gain+threshold(round.baseline_q,trial.physical.q,out.best.q))
            out.best=trial.physical;
    }
    return Result<PfsmCandidateRecallBackprojectionResult>{std::move(out)};
} catch(const detail::Failure &f) {return Result<PfsmCandidateRecallBackprojectionResult>{f.error};}

namespace {
struct LocalWorkspace { detail::NnlsWorkspace physical, linear; };
std::vector<ProfileTemplate> candidateProfiles(const PfsmCandidateRecallPhysicalRound &round,
    const ProfileExpansionInput &input,const ExecutionContext &ctx) {
    if(!round.physical || !round.recall.data)detail::fail("pfsm.fast.v2","Missing anchored round");
    if(!round.physical->anchored)detail::fail("pfsm.fast.v2","Anchored correction requires a v2 round");
    if(input.fwhm_degrees!=round.recall.data->width)detail::fail("pfsm.fast.v2","Anchored width mismatch");
    auto profiles=round.physical->profiles;
    IntermediateLorentzianExpander expander;profiles.push_back(take(expander.prepare(input,ctx)));
    return profiles;
}
Matrix physicalColumns(const PfsmCandidateRecallPhysicalRound &round,const std::vector<ProfileTemplate> &profiles,
                       const std::vector<double> &shifts,const ExecutionContext &ctx) {
    const auto &p=*round.physical;Matrix a(p.z.size(),static_cast<Eigen::Index>(profiles.size()));
    for(std::size_t j=0;j<profiles.size();++j) {
        if(j<p.selected.size() && shifts[j]==p.selected[j].shift)
            a.col(static_cast<Eigen::Index>(j))=p.baseline_columns.col(static_cast<Eigen::Index>(j));
        else a.col(static_cast<Eigen::Index>(j))=physicalColumn(p,profiles[j],shifts[j],ctx);
    }
    return a;
}
double correlationRisk(const PfsmCandidateRecallPhysicalRound &round,const ProfileTemplate &candidate,
                       double shift,const ExecutionContext &ctx) {
    const auto &p=*round.physical;
    const Vector a=physicalColumn(p,candidate,shift,ctx);
    const double norm=a.stableNorm();
    if(!(norm>0) || !std::isfinite(norm))numerical("Zero or nonfinite physical candidate");
    const Vector unit=a/norm;
    const Vector residual=unit-p.orthogonal_basis*(p.orthogonal_basis.transpose()*unit);
    const double eta=residual.squaredNorm();
    if(!std::isfinite(eta))numerical("Nonfinite physical correlation risk");
    return eta;
}
void consider(const PfsmCandidateRecallPhysicalRound &round,PfsmCandidateRecallCandidate &candidate,
              const PfsmCandidateRecallPhysicalFit &fit) {
    if(fit.gain>candidate.best.gain+threshold(round.baseline_q,fit.q,candidate.best.q))candidate.best=fit;
}
void initializeSeed(const PfsmCandidateRecallPhysicalRound &round,const std::vector<ProfileTemplate> &profiles,
                    PfsmCandidateRecallCandidate &candidate,PfsmCandidateRecallSeed &seed,LocalWorkspace &workspace,const ExecutionContext &ctx) {
    auto shifts=fallback(round,seed.initial_shift).parameters.shifts;
    if(candidate.nnls_calls>=PfsmCandidateRecallPolicy::total_calls)numerical("Fast NNLS call budget exhausted");
    seed.current=fitPhysical(round,profiles,shifts,candidate.nnls_calls,candidate.nnls_iterations,workspace.physical,ctx);
    if(seed.current.q>round.baseline_q+threshold(round.baseline_q,seed.current.q,0))
        numerical("Fixed-shift NNLS lost the feasible baseline");
    seed.initial_fit=seed.current;seed.initialized=true;consider(round,candidate,seed.current);
}
void anchoredStep(const PfsmCandidateRecallPhysicalRound &round,const std::vector<ProfileTemplate> &profiles,
                  PfsmCandidateRecallCandidate &candidate,PfsmCandidateRecallSeed &seed,LocalWorkspace &workspace,const ExecutionContext &ctx) {
    detail::checkpoint(ctx);
    const auto &p=*round.physical;const auto &data=*round.recall.data;
    const std::size_t m=profiles.size();
    if(!seed.initialized || seed.steps.size()>=PfsmCandidateRecallPolicy::steps ||
       candidate.nnls_calls+2>PfsmCandidateRecallPolicy::total_calls)numerical("Fast local step budget exhausted");
    const auto &shifts=seed.current.parameters.shifts;
    Matrix a=physicalColumns(round,profiles,shifts,ctx);
    Matrix derivative(a.rows(),a.cols());
    for(std::size_t j=0;j<m;++j) {
        if(j<p.selected.size() && shifts[j]==p.selected[j].shift && p.baseline_derivatives.cols()==a.cols()-1)
            derivative.col(static_cast<Eigen::Index>(j))=p.baseline_derivatives.col(static_cast<Eigen::Index>(j));
        else {
            auto samples=take(evaluateProfilePairedSamples(profiles[j],p.coordinates,shifts[j],ctx));
            a.col(static_cast<Eigen::Index>(j))=detail::vectorOf(samples.intensity).cwiseProduct(p.sqrt_weights);
            derivative.col(static_cast<Eigen::Index>(j))=detail::vectorOf(samples.shift_derivative).cwiseProduct(p.sqrt_weights)*data.width;
        }
    }
    if(!derivative.allFinite())numerical("Nonfinite anchored derivative");
    std::vector<std::pair<double,double>> bounds;std::vector<Vector> dictionary;
    for(std::size_t j=0;j<m;++j) {
        const double lower=std::max(-seed.radius,(data.lower_shift-shifts[j])/data.width);
        const double upper=std::min(seed.radius,(data.upper_shift-shifts[j])/data.width);
        if(lower>upper || lower>0 || upper<0)numerical("Current shift outside local feasible interval");
        bounds.emplace_back(lower,upper);
        dictionary.push_back(a.col(static_cast<Eigen::Index>(j))+lower*derivative.col(static_cast<Eigen::Index>(j)));
        if(lower!=upper)dictionary.push_back(a.col(static_cast<Eigen::Index>(j))+upper*derivative.col(static_cast<Eigen::Index>(j)));
    }
    Matrix linear(a.rows(),static_cast<Eigen::Index>(dictionary.size()));
    for(std::size_t j=0;j<dictionary.size();++j)linear.col(static_cast<Eigen::Index>(j))=dictionary[j];
    workspace.linear.resize(linear.rows(),linear.cols());
    ++candidate.nnls_calls;
    const auto solved=detail::nnls(linear,p.z,0,workspace.linear,ctx);candidate.nnls_iterations+=solved.iterations;
    const Vector model=a*detail::vectorOf(seed.current.parameters.scales);
    const Vector residual=p.z-model;
    PfsmCandidateRecallStep step;step.radius=seed.radius;step.anchor=seed.current.parameters;
    step.linear_coefficients=detail::valuesOf(solved.scales);
    step.predicted=stableGain(residual,linear*solved.scales-model);
    std::vector<double> trialShifts=shifts;std::vector<double> localSteps(m);
    std::size_t offset=0;
    for(std::size_t j=0;j<m;++j) {
        const auto [lower,upper]=bounds[j];
        if(lower==upper) {localSteps[j]=lower;++offset;}
        else {
            const double u=solved.scales[static_cast<Eigen::Index>(offset++)];
            const double v=solved.scales[static_cast<Eigen::Index>(offset++)];
            const double largest=std::max(u,v);
            localSteps[j]=largest>0?(lower*(u/largest)+upper*(v/largest))/((u/largest)+(v/largest)):0;
        }
        trialShifts[j]=std::clamp(shifts[j]+data.width*localSteps[j],data.lower_shift,data.upper_shift);
    }
    step.physical=fitPhysical(round,profiles,trialShifts,candidate.nnls_calls,candidate.nnls_iterations,workspace.physical,ctx);
    const Matrix trialColumns=physicalColumns(round,profiles,trialShifts,ctx);
    const Vector trialModel=trialColumns*detail::vectorOf(step.physical.parameters.scales);
    step.actual=stableGain(residual,trialModel-model);
    step.distortion=std::abs(step.predicted-step.actual);
    if(step.predicted>0)step.ratio=step.actual/step.predicted;
    const double tolerance=threshold(round.baseline_q,seed.current.q,step.physical.q);
    if(std::abs(step.actual-(seed.current.q-step.physical.q))>tolerance)
        numerical("Physical trial gain identity failed");
    step.indistinguishable=std::abs(step.actual)<=tolerance;
    step.accepted=step.actual>tolerance && step.ratio && *step.ratio>=PfsmCandidateRecallPolicy::acceptance;
    const double amplitudeTolerance=threshold(1.,trialModel.norm(),0);
    const double displacementTolerance=128*std::numeric_limits<double>::epsilon();
    for(std::size_t j=0;j<m;++j) {
        const auto [lower,upper]=bounds[j];
        const bool edge=std::abs(localSteps[j]-lower)<=displacementTolerance || std::abs(localSteps[j]-upper)<=displacementTolerance;
        const bool physicalEdge=std::abs(trialShifts[j]-data.lower_shift)<=data.width*displacementTolerance ||
                                std::abs(trialShifts[j]-data.upper_shift)<=data.width*displacementTolerance;
        if(lower!=upper && edge && !physicalEdge &&
           step.physical.parameters.scales[j]*trialColumns.col(static_cast<Eigen::Index>(j)).stableNorm()>amplitudeTolerance)
            step.local_boundary=true;
    }
    candidate.maximum_distortion=std::max(candidate.maximum_distortion,step.distortion);
    consider(round,candidate,step.physical);
    if(step.accepted)seed.current=step.physical;else seed.radius*=.5;
    seed.steps.push_back(std::move(step));
}
} // namespace

Result<PfsmCandidateRecallCandidate> evaluatePfsmCandidateRecallInitial(const PfsmCandidateRecallPhysicalRound &round,
    const ProfileExpansionInput &input,const PfsmCandidateRecallEvidence &recall,const ExecutionContext &ctx) {
    PfsmCandidateRecallCandidate out;out.recall=recall;out.numerical_protection=recall.protection_reason;
    try {
        detail::checkpoint(ctx);
        if(!round.physical || !round.recall.data)detail::fail("pfsm.fast.v2","Missing anchored baseline");
        const bool residual=recall.recall_reason=="residual_channel" || recall.recall_reason=="residual_cutoff_tie";
        out.seeds=residual?recall.residual_seeds:recall.original_seeds;
        if(out.seeds.empty())out.seeds.push_back(residual?recall.residual_shift:recall.original_shift);
        if(out.seeds.size()>PfsmCandidateRecallPolicy::seeds)detail::fail("pfsm.fast.v2","Too many frozen seeds");
        out.best=fallback(round,out.seeds.front());
        if(!out.numerical_protection.empty())return Result<PfsmCandidateRecallCandidate>{std::move(out)};
        for(std::size_t i=0;i<out.seeds.size();++i) {
            const auto &d=*round.recall.data;
            if(!std::isfinite(out.seeds[i]) || out.seeds[i]<d.lower_shift || out.seeds[i]>d.upper_shift)
                detail::fail("pfsm.fast.v2","Invalid frozen seed");
            for(std::size_t j=0;j<i;++j)if(std::abs(out.seeds[i]-out.seeds[j])<d.width/4)
                detail::fail("pfsm.fast.v2","Unseparated frozen seeds");
        }
        auto profiles=candidateProfiles(round,input,ctx);
        LocalWorkspace workspace;
        out.starts.push_back({});out.starts.back().initial_shift=out.seeds.front();
        out.initialized=true;
        initializeSeed(round,profiles,out,out.starts.back(),workspace,ctx);
        anchoredStep(round,profiles,out,out.starts.back(),workspace,ctx);
        out.initial_gain=out.best.gain;
        out.initial_eta=out.eta=correlationRisk(round,profiles.back(),out.best.parameters.shifts.back(),ctx);
    } catch(const detail::Failure &f) {
        if(f.error.code!=ErrorCode::numerical_failure)return Result<PfsmCandidateRecallCandidate>{f.error};
        out.numerical_protection=f.error.message;
    }
    return Result<PfsmCandidateRecallCandidate>{std::move(out)};
}
Result<PfsmCandidateRecallCandidate> reviewPfsmCandidateRecall(const PfsmCandidateRecallPhysicalRound &round,
    const ProfileExpansionInput &input,PfsmCandidateRecallCandidate out,const ExecutionContext &ctx) {
    try {
        detail::checkpoint(ctx);
        if(out.reviewed)detail::fail("pfsm.fast.v2","Repeated candidate review");
        out.reviewed=true;
        if(!out.numerical_protection.empty())return Result<PfsmCandidateRecallCandidate>{std::move(out)};
        if(!out.initialized || out.starts.size()!=1 || out.starts.front().steps.size()!=1)
            detail::fail("pfsm.fast.v2","Review requires one completed initial step");
        auto profiles=candidateProfiles(round,input,ctx);
        LocalWorkspace workspace;
        anchoredStep(round,profiles,out,out.starts.front(),workspace,ctx);
        for(std::size_t i=1;i<out.seeds.size();++i) {
            out.starts.push_back({});auto &seed=out.starts.back();seed.initial_shift=out.seeds[i];
            initializeSeed(round,profiles,out,seed,workspace,ctx);
            anchoredStep(round,profiles,out,seed,workspace,ctx);anchoredStep(round,profiles,out,seed,workspace,ctx);
        }
        out.eta=correlationRisk(round,profiles.back(),out.best.parameters.shifts.back(),ctx);
    } catch(const detail::Failure &f) {
        if(f.error.code!=ErrorCode::numerical_failure)return Result<PfsmCandidateRecallCandidate>{f.error};
        out.numerical_protection=f.error.message;
    }
    return Result<PfsmCandidateRecallCandidate>{std::move(out)};
}
} // namespace qualpf::profile
