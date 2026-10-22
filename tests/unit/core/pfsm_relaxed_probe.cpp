// Text-protocol probe over the production Core implementation; no duplicate search.
#include "qualpf/profile/pfsm_relaxed_screen.h"
#include <algorithm>
#include <cmath>
#include <atomic>
#include <cfenv>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <thread>
using namespace qualpf;
using namespace qualpf::profile;
namespace {
struct Failure { Error error; };
template<class T> T take(Result<T> r) { if (!r) throw Failure{r.error()}; return std::move(r).value(); }
void checkpoint(const ExecutionContext &context) { if (context.cancellation.stop_requested()) throw Failure{{ErrorCode::cancelled,"probe","Canceled"}}; }
std::atomic_bool interrupted=false;
static_assert(std::atomic_bool::is_always_lock_free);
void interrupt(int) { interrupted.store(true,std::memory_order_relaxed); }
using Evidence=PfsmRelaxedEvidence;
constexpr std::size_t cacheLimit=32,maxEvaluations=205;
struct Card { std::string id; std::size_t index; double fine,original; ProfileExpansionInput input; };
void numbers(const std::vector<double> &v) {
    std::cout << '[';
    for (std::size_t j=0; j<v.size(); ++j) { if (j) std::cout << ','; std::cout << v[j]; }
    std::cout << ']';
}
}
int main(int argc,char **argv) try {
    std::signal(SIGINT,interrupt); std::signal(SIGTERM,interrupt);
    if (argc>1 && std::string(argv[1])=="--cancel") interrupted=1;
    std::stop_source stop;
    if (interrupted) stop.request_stop();
    const ExecutionContext context{stop.get_token(),NumericalPolicy::explicit_float64,{}};
    std::size_t n,m,count,threads,radiations; double h,lo,hi;
    if (!(std::cin>>n>>m>>count>>threads>>h>>lo>>hi>>radiations) || n<2 || n>100000 || m>64 || !count || count>100000 || radiations>16 ||
        !threads || threads>64 || !std::isfinite(lo) || !std::isfinite(hi) || lo>=hi) return 2;
    Radiation radiation;
    for (std::size_t j=0;j<radiations;++j) {
        double wave,weight; if (!(std::cin>>wave>>weight)) return 2;
        radiation.lines.push_back({WavelengthAngstrom{wave},weight});
    }
    auto observations=std::make_shared<ProfileObservations>();
    for (std::size_t i=0;i<n;++i) {
        double x,y,w; if (!(std::cin>>x>>y>>w)) return 2;
        observations->coordinates.push_back(x); observations->observed.push_back(y);
        observations->background.push_back(0); observations->weights.push_back(w);
    }
    const auto lines=[&] {
        std::size_t count; if (!(std::cin>>count) || count>20000) throw std::runtime_error("Invalid line count");
        std::vector<ProfileLine> refs;
        for (std::size_t j=0;j<count;++j) {
            double q,s; if (!(std::cin>>q>>s)) throw std::runtime_error("Incomplete reference"); refs.push_back({j,q,s});
        }
        ProfileExpansionInput input;
        input.coordinates=radiations ? ProfileExpansionInput::Coordinates::d_spacing_angstrom : ProfileExpansionInput::Coordinates::two_theta_degrees;
        input.lines=std::move(refs); input.radiation=radiation; input.fwhm_degrees=h;
        return input;
    };
    std::vector<ProfileExpansionInput> selected;
    std::vector<double> old;
    for (std::size_t j=0;j<m;++j) {
        double shift; if (!(std::cin>>shift) || shift<lo || shift>hi) return 2;
        old.push_back(shift); selected.push_back(lines());
    }
    std::vector<Card> cards(count);
    for (auto &card:cards) {
        if (!(std::cin>>card.index>>card.id>>card.fine>>card.original) || card.id.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_.-")!=std::string::npos ||
            !std::isfinite(card.fine) || !std::isfinite(card.original) || card.fine<lo || card.fine>hi || card.original<lo || card.original>hi) return 2;
        card.input=lines();
    }
    const auto data=take(preparePfsmIntegralData(observations,{},h,{},context));
    const double radius=std::min(1.5*h,.5*(hi-lo));
    const auto relaxed=take(preparePfsmRelaxedRound(data,selected,old,lo,hi,{}, {},context));
    const auto &banks=relaxed.banks;
    const auto bankBytes=relaxed.buffer_bytes,bankCount=relaxed.bank_columns;
    std::vector<Evidence> evidence(count);
    std::atomic_size_t completed=0;
    std::atomic_bool allocationFailed=false;
    std::fenv_t environment; std::fegetenv(&environment);
    std::vector<std::jthread> workers;
    for (std::size_t worker=0;worker<threads;++worker) workers.emplace_back([&,worker] {
        std::fenv_t previous; std::fegetenv(&previous); std::fesetenv(&environment);
        for (std::size_t i=worker;i<count;i+=threads) {
            if (stop.stop_requested() || interrupted) { stop.request_stop(); break; }
            try {
                PfsmScreenEvidence seed; seed.fine_shift=cards[i].fine;
                seed.original.shift=cards[i].original; seed.integral_complete=true;
                evidence[i]=take(evaluatePfsmRelaxedCandidate(relaxed,cards[i].input,seed,context));
            } catch (const std::bad_alloc &) { allocationFailed=true; stop.request_stop(); }
            catch (const Failure &e) {
                if (e.error.code==ErrorCode::cancelled) stop.request_stop();
                else evidence[i].uncertainty=e.error.field+": "+e.error.message;
            } catch (const std::exception &e) { evidence[i].uncertainty=e.what(); }
            const auto done=++completed;
            if (done%100==0 || done==count) std::cerr << "completed " << done << '/' << count << '\n';
        }
        std::fesetenv(&previous);
    });
    for (auto &worker:workers) worker.join();
    if (allocationFailed) { std::cerr << "Allocation failure; no selection published\n"; return 1; }
    if (stop.stop_requested()) return 130;
    std::cout << std::setprecision(17);
    const auto bins=data->fine.weights.size(),columns=m+1;
    const auto workerBound=sizeof(double)*(cacheLimit*bins+20*bins*columns+20*columns*columns+64*columns+18*bins)+65536;
    std::cout << "{\"schema\":\"qualpf.pfsm.relaxed-probe.v1\",\"fine_bins\":" << data->fine.weights.size()
        << ",\"fine_denominator\":" << data->fine.denominator << ",\"radius\":" << radius
        << ",\"old_bank_columns\":" << bankCount << ",\"old_bank_capacity_bytes\":" << bankBytes
        << ",\"shared_integral_capacity_bytes\":" << data->buffer_bytes
        << ",\"worker_capacity_allowance_bytes\":" << workerBound
        << ",\"memory_qualification\":\"Visible capacity plus conservative Eigen/scratch allowance, not measured process peak\""
        << ",\"nnls_limit\":" << maxEvaluations << "}\n";
    for (std::size_t i=0;i<count;++i) {
        const auto &e=evidence[i];
        std::cout << "{\"id\":" << std::quoted(cards[i].id) << ",\"original_index\":" << cards[i].index
            << ",\"normalized_gain\":" << e.gain << ",\"fixed_q\":" << e.fixed_q
            << ",\"null_q\":" << e.null.q << ",\"best_q\":" << e.best.q
            << ",\"evaluations\":" << e.evaluations << ",\"nnls_iterations\":" << e.iterations
            << ",\"maximum_kkt\":" << e.kkt << ",\"candidate_cache_columns\":" << e.cache_columns
            << ",\"candidate_cache_capacity_bytes\":" << e.cache_bytes << ",\"uncertainty\":" << std::quoted(e.uncertainty)
            << ",\"complete\":" << (e.complete ? "true" : "false")
            << ",\"released_old_indices\":[";
        for (std::size_t j=0;j<e.released.size();++j) { if (j) std::cout << ','; std::cout << e.released[j]; }
        std::cout << "],\"best_shifts\":"; numbers(e.best.shifts);
        std::cout << ",\"best_scales\":"; numbers(e.best.scales);
        std::cout << ",\"null_shifts\":"; numbers(e.null.shifts);
        std::cout << "}\n";
    }
    return 0;
} catch (const Failure &e) {
    std::cerr << e.error.field << ": " << e.error.message << '\n'; return e.error.code==ErrorCode::cancelled ? 130 : 1;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
