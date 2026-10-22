#include "qualpf/profile/pfsm_relaxed_screen.h"
#include "pfsm_internal.h"
#include <array>
#include <deque>
#include <numeric>
namespace qualpf::profile {
namespace {
using namespace detail;
template<class T> T take(Result<T> r) { if (!r) throw Failure{r.error()}; return std::move(r).value(); }
constexpr std::size_t oldLimit=2, stages=4, cacheLimit=32, maxEvaluations=205;
constexpr double tie=128*std::numeric_limits<double>::epsilon();
using Bank=PfsmRelaxedBank;
using Point=PfsmRelaxedPoint;
struct Card { double fine, original; std::vector<ProfileLine> lines; };
std::size_t nearest(const Bank &bank,double x) {
    const auto it=std::lower_bound(bank.shifts.begin(),bank.shifts.end(),x);
    if (it==bank.shifts.begin()) return 0;
    if (it==bank.shifts.end()) return bank.shifts.size()-1;
    const auto j=std::size_t(it-bank.shifts.begin());
    return x-bank.shifts[j-1]<=bank.shifts[j]-x ? j-1 : j;
}
struct Worker {
    const PfsmIntegralData &data;
    const std::vector<Bank> &banks;
    const Card &card;
    const ExecutionContext &context;
    const ProfileFitOptions &fitOptions;
    PfsmRelaxedEvidence result;
    Matrix a;
    Vector z;
    NnlsWorkspace fullWorkspace, nullWorkspace;
    std::deque<std::pair<double,std::vector<double>>> cache;
    bool contribution = false;
    Worker(const PfsmIntegralData &d, const std::vector<Bank> &b, const Card &v,
           const ExecutionContext &c, const ProfileFitOptions &options)
        : data(d), banks(b), card(v), context(c), fitOptions(options),
          a(Eigen::Index(d.fine.net.size()),Eigen::Index(b.size()+1)), z(a.rows()) {
        for (Eigen::Index i=0; i<a.rows(); ++i)
            z[i]=std::sqrt(d.fine.weights[std::size_t(i)]/d.fine.denominator)*d.fine.net[std::size_t(i)];
        fullWorkspace.resize(a.rows(),a.cols()); nullWorkspace.resize(a.rows(),a.cols()-1);
    }
    void check() { checkpoint(context); }
    const std::vector<double> &candidate(double shift) {
        check();
        for (const auto &entry:cache) if (entry.first==shift) return entry.second;
        if (cache.size()==cacheLimit) cache.pop_front();
        auto column=take(integratePfsmScreenTemplate(data,card.lines,shift,context)).fine;
        contribution=contribution || std::any_of(column.begin(),column.end(),[](double v){return v>0;});
        cache.emplace_back(shift,std::move(column));
        result.cache_columns=std::max(result.cache_columns,cache.size());
        std::size_t bytes=0; for (const auto &entry:cache) bytes+=entry.second.capacity()*sizeof(double);
        result.cache_bytes=std::max(result.cache_bytes,bytes);
        return cache.back().second;
    }
    Point solve(std::vector<double> shifts, bool withCandidate) {
        check();
        if (++result.evaluations>maxEvaluations) fail("diagnostic.budget","Bounded NNLS count exceeded");
        const auto m=banks.size()+std::size_t(withCandidate);
        for (std::size_t j=0;j<m;++j) {
            const std::vector<double> *col;
            if (j<banks.size()) {
                const auto index=nearest(banks[j],shifts[j]); shifts[j]=banks[j].shifts[index];
                col=&banks[j].columns[index];
            } else col=&candidate(shifts[j]);
            for (Eigen::Index i=0;i<a.rows();++i)
                a(i,Eigen::Index(j))=(*col)[std::size_t(i)]*std::sqrt(data.fine.weights[std::size_t(i)])/std::sqrt(data.fine.denominator);
        }
        auto &workspace=withCandidate ? fullWorkspace : nullWorkspace;
        std::fill(workspace.valid.begin(),workspace.valid.end(),false);
        const auto fit=nnls(a.leftCols(Eigen::Index(m)),z,fitOptions.maximum_nnls_iterations,workspace,context);
        result.iterations+=fit.iterations; result.kkt=std::max(result.kkt,fit.kkt);
        const double q=fit.residual.squaredNorm()*data.fine.denominator;
        if (!std::isfinite(q)) fail("diagnostic.objective","Nonfinite relaxed objective");
        return {q,std::move(shifts),valuesOf(fit.scales)};
    }
    bool better(const Point &x,const Point &y) const {
        const double difference=(x.q-y.q)/data.fine.denominator;
        return difference < -tie || (std::abs(difference)<=tie && x.shifts<y.shifts);
    }
    Point search(std::vector<double> shifts, bool withCandidate, double quantum, double lo, double hi) {
        Point best=solve(std::move(shifts),withCandidate);
        const std::array<int,stages> steps{64,16,4,1};
        auto free=result.released;
        if (withCandidate) free.push_back(banks.size());
        for (int step:steps) for (auto j:free) {
            const auto center=best.shifts;
            Point local=best;
            std::vector<double> tried;
            for (int offset:{-2,-1,0,1,2}) {
                auto trial=center;
                trial[j]=std::clamp(center[j]+double(step*offset)*quantum,
                    j<banks.size() ? banks[j].shifts.front() : lo,
                    j<banks.size() ? banks[j].shifts.back() : hi);
                if (j<banks.size()) trial[j]=banks[j].shifts[nearest(banks[j],trial[j])];
                if (std::find(tried.begin(),tried.end(),trial[j])!=tried.end()) continue;
                tried.push_back(trial[j]);
                if (trial[j]==center[j]) continue;
                auto value=solve(std::move(trial),withCandidate);
                if (better(value,local)) local=std::move(value);
            }
            best=std::move(local);
        }
        return best;
    }
    PfsmRelaxedEvidence run(const std::vector<double> &old,double radius,double lo,double hi) {
        auto location=old; location.push_back(card.fine);
        result.fixed_q=solve(location,true).q; // Whole-pool v1 fine-location replay.
        std::vector<std::pair<double,std::size_t>> correlations;
        const auto &p=candidate(card.fine);
        for (std::size_t j=0;j<banks.size();++j) {
            const auto &v=banks[j].columns[nearest(banks[j],old[j])];
            double pp=0,vv=0,pv=0;
            for (std::size_t i=0;i<p.size();++i) {
                pp+=data.fine.weights[i]*p[i]*p[i]; vv+=data.fine.weights[i]*v[i]*v[i];
                pv+=data.fine.weights[i]*p[i]*v[i];
            }
            correlations.emplace_back(pp>0 && vv>0 ? pv/std::sqrt(pp*vv) : 0,j);
        }
        std::stable_sort(correlations.begin(),correlations.end(),[](const auto &x,const auto &y){return x.first>y.first;});
        for (std::size_t j=0;j<std::min(oldLimit,correlations.size());++j) result.released.push_back(correlations[j].second);
        std::sort(result.released.begin(),result.released.end());
        const double quantum=radius/128;
        auto base=search(old,false,quantum,lo,hi);
        std::vector<double> seeds{card.fine,card.original};
        std::sort(seeds.begin(),seeds.end()); seeds.erase(std::unique(seeds.begin(),seeds.end()),seeds.end());
        bool first=true;
        for (double seed:seeds) {
            auto start=base.shifts; start.push_back(seed);
            auto fit=search(std::move(start),true,quantum,lo,hi);
            if (first || better(fit,result.best)) result.best=std::move(fit);
            first=false;
        }
        // Matched null control gets the same released old coordinates, including
        // a second bounded search from the candidate's old shifts. This avoids
        // crediting an obviously improvable null model to the candidate.
        auto nullStart=result.best.shifts; nullStart.pop_back();
        auto null=search(std::move(nullStart),false,quantum,lo,hi);
        result.null=better(null,base) ? std::move(null) : std::move(base);
        result.gain=std::max(0.,result.null.q-result.best.q)/data.fine.denominator;
        if (!contribution) result.uncertainty="no_candidate_contribution";
        result.complete=true;
        return result;
    }
};

}
Status validatePfsmRelaxedOptions(const PfsmRelaxedOptions &options) {
    if (options!=PfsmRelaxedOptions{}) return Status{Error{ErrorCode::invalid_input,"pfsm.relaxed.options","Expected frozen bounded-old-shift v1"}};
    return Status{std::monostate{}};
}
Result<PfsmRelaxedRound> preparePfsmRelaxedRound(std::shared_ptr<const PfsmIntegralData> data,
    std::span<const ProfileExpansionInput> selected,std::span<const double> shifts,double lower,double upper,
    const ProfileFitOptions &fit,const PfsmRelaxedOptions &options,const ExecutionContext &context) try {
    checkpoint(context); take(validatePfsmRelaxedOptions(options));
    if (!data || selected.size()!=shifts.size() || !std::isfinite(lower) || !std::isfinite(upper) || lower>upper)
        fail("pfsm.relaxed.round","Invalid bounded relaxation input");
    PfsmRelaxedRound round;
    round.data=std::move(data); round.fit=fit; round.options=options;
    round.lower_shift=lower; round.upper_shift=upper;
    round.radius=std::min(1.5*round.data->width,.5*(upper-lower));
    round.old_shifts.assign(shifts.begin(),shifts.end()); round.banks.resize(selected.size());
    for (std::size_t j=0;j<selected.size();++j) {
        if (!std::isfinite(shifts[j]) || shifts[j]<lower || shifts[j]>upper) fail("pfsm.relaxed.shifts","Old shift outside bounds");
        const auto lines=take(expandProfileLines(selected[j],context));
        auto &bank=round.banks[j];
        for (int tick=-128;tick<=128;++tick) bank.shifts.push_back(std::clamp(shifts[j]+double(tick)*round.radius/128,lower,upper));
        std::sort(bank.shifts.begin(),bank.shifts.end()); bank.shifts.erase(std::unique(bank.shifts.begin(),bank.shifts.end()),bank.shifts.end());
        for (double shift:bank.shifts) {
            checkpoint(context); bank.columns.push_back(take(integratePfsmScreenTemplate(*round.data,lines,shift,context)).fine);
            round.buffer_bytes+=bank.columns.back().capacity()*sizeof(double); ++round.bank_columns;
        }
        round.buffer_bytes+=bank.shifts.capacity()*sizeof(double)+bank.columns.capacity()*sizeof(std::vector<double>);
    }
    round.buffer_bytes+=round.banks.capacity()*sizeof(PfsmRelaxedBank)+round.old_shifts.capacity()*sizeof(double);
    return Result<PfsmRelaxedRound>{std::move(round)};
} catch (const Failure &e) { return Result<PfsmRelaxedRound>{e.error}; }
Result<PfsmRelaxedEvidence> evaluatePfsmRelaxedCandidate(const PfsmRelaxedRound &round,
    const ProfileExpansionInput &input,const PfsmScreenEvidence &seed,const ExecutionContext &context) try {
    checkpoint(context);
    if (!seed.integral_complete) {
        PfsmRelaxedEvidence unavailable; unavailable.uncertainty="integral_seed_unavailable";
        return Result<PfsmRelaxedEvidence>{std::move(unavailable)};
    }
    if (!round.data || !std::isfinite(seed.fine_shift) || !std::isfinite(seed.original.shift) ||
        seed.fine_shift<round.lower_shift || seed.fine_shift>round.upper_shift)
        fail("pfsm.relaxed.seed","Invalid spectral relaxation starts");
    Card card{seed.fine_shift,std::clamp(seed.original.shift,round.lower_shift,round.upper_shift),take(expandProfileLines(input,context))};
    Worker work(*round.data,round.banks,card,context,round.fit);
    try { work.result=work.run(round.old_shifts,round.radius,round.lower_shift,round.upper_shift); }
    catch (const Failure &e) {
        if (e.error.code==ErrorCode::cancelled || e.error.code==ErrorCode::invalid_input) throw;
        work.result.uncertainty=e.error.field+": "+e.error.message;
    }
    const auto n=round.data->fine.net.size(),m=round.banks.size()+1;
    work.result.buffer_bytes=sizeof(double)*(cacheLimit*n+20*n*m+20*m*m+64*m+18*n)+65536+card.lines.capacity()*sizeof(ProfileLine);
    return Result<PfsmRelaxedEvidence>{std::move(work.result)};
} catch (const Failure &e) { return Result<PfsmRelaxedEvidence>{e.error}; }
Result<PfsmScreenSelection> selectPfsmRelaxedCandidates(std::span<PfsmScreenEvidence> rows,
    const PfsmRelaxedOptions &options,const ExecutionContext &context) try {
    checkpoint(context); take(validatePfsmRelaxedOptions(options));
    std::vector<std::size_t> order;
    for (std::size_t i=0;i<rows.size();++i) {
        auto &e=rows[i].relaxed; e.selected=false;
        if (!e.uncertainty.empty()) rows[i].retained=true;
        if (!e.complete && e.uncertainty.empty()) fail("pfsm.relaxed.selection","Unaccounted relaxation evaluation");
        if (e.complete && e.uncertainty.empty()) {
            if (!std::isfinite(e.gain) || e.gain<0) fail("pfsm.relaxed.selection","Invalid relaxed score");
            order.push_back(i);
        }
    }
    std::sort(order.begin(),order.end(),[&](auto a,auto b) {
        return rows[a].relaxed.gain!=rows[b].relaxed.gain ? rows[a].relaxed.gain>rows[b].relaxed.gain : rows[a].original_index<rows[b].original_index;
    });
    if (!order.empty()) {
        const double cutoff=rows[order[std::min(options.budget,order.size())-1]].relaxed.gain;
        for (auto i:order) if (rows[i].relaxed.gain>=cutoff-tie) rows[i].relaxed.selected=rows[i].retained=true;
    }
    PfsmScreenSelection selection;
    for (const auto &e:rows) if (e.retained) selection.indices.push_back(e.original_index);
    std::sort(selection.indices.begin(),selection.indices.end());
    return Result<PfsmScreenSelection>{std::move(selection)};
} catch (const Failure &e) { return Result<PfsmScreenSelection>{e.error}; }
} // namespace qualpf::profile
