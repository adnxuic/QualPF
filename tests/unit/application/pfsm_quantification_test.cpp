#include "qualpf/application/pfsm.h"
#include "qualpf/application/presentation/quantification.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/search_coordinator.h"
#include "qualpf/domain/radiation.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) throw std::runtime_error("PFSM quantification at line " + std::to_string(at.line()));
}
template<class T> static T take(Result<T> r) {
    if (!r) throw std::runtime_error(r.error().field+": "+r.error().message);
    return std::move(r).value();
}
static double il(double x, double center) {
    return std::pow(1+4*(std::pow(2.,2./3)-1)*std::pow((x-center)/.2,2),-1.5);
}
struct Reader final : CandidateReader {
    int calls = 0;
    Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &, SearchProgress) override {
        ++calls;
        std::vector<ProductCard> cards;
        for (int i=0;i<2;++i) {
            ProductCard c; c.id = i ? "B" : "A"; c.chemicalName = i ? "Phase B" : "Phase A";
            c.rir = i ? "4" : "2"; c.referenceRir = i ? 4 : 2;
            for (double angle : i ? std::vector<double>{33,34} : std::vector<double>{30,31})
                c.d.push_back(radiation::CU_KA1_WAVE/(2*std::sin(angle*std::numbers::pi/360)));
            c.intensity = i ? std::vector<double>{1000,250} : std::vector<double>{100,50};
            cards.push_back(std::move(c));
        }
        return Result<std::vector<ProductCard>>{std::move(cards)};
    }
};
static ProductState initial() {
    ProductState s; s.algorithms = take(resolveAlgorithms(AlgorithmSpec{{},SearchMethod::pfsm}));
    auto &p = s.pattern; p.id={44}; p.source_id="Independent 25/75 mixture";
    p.radiation.kind=RadiationKind::x_ray;
    p.radiation.lines={{WavelengthAngstrom{radiation::CU_KA1_WAVE},1}};
    p.processing={false,false,false};
    for (int i=0;i<=800;++i) {
        double x=28+i*.01; p.two_theta.emplace_back(x);
        p.observed.push_back(7+5*(100*il(x,30)+50*il(x,31))+3*(1000*il(x,33)+250*il(x,34)));
    }
    p.calculation_range=AngularRange{p.two_theta.front(),p.two_theta.back()};
    return s;
}
static void execute(Workspace &w, Reader &r, SearchInput in) {
    auto task=take(w.searches().begin({"fixture",3}));
    in.source=task.source; in.method=SearchMethod::pfsm; in.taskId=task.id; in.databaseVersion=3;
    auto result=SearchEngine{}.run(in,r,task.context);
    take(w.searches().complete(task.id,std::move(result)));
}
static bool reason(const QuantificationResult &q,const char *code) {
    return std::any_of(q.issues.begin(),q.issues.end(),[&](const auto &i){return i.reason==code;});
}
int main() try {
    Workspace w; check(bool(w.session().commit(w.session().snapshot(),initial())));
    Reader reader;
    auto request=std::make_shared<PfsmRequest>(); request->threads=1;
    request->lower_shift=0; request->upper_shift=0; request->observations.emplace();
    auto &o=*request->observations; o.signal_source="Independent analytic mixture";
    o.intensity_units="analytic units"; o.unclipped_signal=true; o.equal_weights=true;
    o.noise_source="No variance; independently prescribed analytic observations";
    o.background=std::vector<double>(801,7);
    FwhmUsage width; width.width=AngularWidthDegrees{.2}; width.origin=FwhmOrigin::fixed;
    width.current=true; width.algorithm={"qualpf.fwhm.fixed",1}; request->supplied_width=width;
    SearchParameters acceptanceParameters;
    SearchInput in; in.pfsm=request; execute(w,reader,in);
    check(prepareQuantification(w.session().snapshot()).availability==QuantificationAvailability::empty);
    for (const char *id : {"A","B"}) {
        auto intent=take(phaseAcceptanceInput(w.session().snapshot(),id,acceptanceParameters,false)); check(bool(intent));
        execute(w,reader,*intent);
    }
    const auto full=w.session().snapshot();
    auto q=prepareQuantification(full); check(q.available() && q.source==full && q.generation==3);
    check(std::abs(*q.phases[0].weight_percent-25)<1e-9 && std::abs(*q.phases[1].weight_percent-75)<1e-9);
    check(q.phases[0].reference_maximum==100 && q.phases[1].reference_maximum==1000);
    check(!full->phases[0].fomCalculated && reader.calls==1); // No Classical-score dependency or reread.
    const auto generation=full->pfsm->generation;
    PfsmRirRevision edit{6,"Independent calibrated RIR; Cu Kalpha"};
    check(bool(w.revisePfsmRir(full,"B",edit)));
    const auto revised=w.session().snapshot(); q=prepareQuantification(revised);
    check(q.available() && q.phases[1].rir_revised && q.phases[1].original_rir=="4");
    check(std::abs(*q.phases[0].weight_percent-100./3)<1e-9);
    check(revised->pfsm->generation==generation && revised->pfsm->universe==full->pfsm->universe &&
          revised->pfsm->baseline.parameters.scales==full->pfsm->baseline.parameters.scales && reader.calls==1);
    check(!w.revisePfsmRir(full,"B",edit) && w.session().snapshot()==revised);
    check(bool(w.revisePfsmRir(revised,"B",edit)) && w.session().snapshot()==revised); // No-op.
    check(!w.revisePfsmRir(revised,"B",PfsmRirRevision{0,"invalid"}));
    check(!w.revisePfsmRir(revised,"B",PfsmRirRevision{2," \t"}));
    check(!w.revisePfsmRir(revised,"missing",edit));
    auto active=take(w.searches().begin({"fixture",3}));
    check(!w.revisePfsmRir(revised,"B",{}) && w.session().snapshot()==revised);
    w.searches().cancel();
    check(!w.searches().complete(active.id,Result<PreparedSearchResult>{Error{ErrorCode::cancelled,"test","cancel"}}));
    auto remove=take(phaseRemovalInput(revised,"B")); execute(w,reader,remove);
    auto accept=take(phaseAcceptanceInput(w.session().snapshot(),"B",acceptanceParameters,false)); check(bool(accept)); execute(w,reader,*accept);
    check(prepareQuantification(w.session().snapshot()).phases[1].rir_revised && reader.calls==1);
    check(bool(w.revisePfsmRir(w.session().snapshot(),"B",{})));
    q=prepareQuantification(w.session().snapshot()); check(q.available() && !q.phases[1].rir_revised);
    const auto invalid=[&](const auto &mutate) {
        auto s=*full; auto a=std::make_shared<PfsmAnalysis>(*s.pfsm); mutate(s,*a); s.pfsm=a;
        return prepareQuantification(std::make_shared<const ProductState>(std::move(s)));
    };
    auto missing=invalid([](auto &,auto &a){auto l=std::make_shared<PfsmQuantificationLibrary>(*a.quantification_library);l->references["B"].rir.reset();l->references["B"].original_rir.clear();a.quantification_library=l;});
    check(!missing.available() && reason(missing,"missing_rir") && !missing.phases[0].weight_percent && missing.issues[0].phase_id=="B");
    check(reason(invalid([](auto &,auto &a){a.baseline.diagnostics.rank_deficient=true;}),"rank_deficient"));
    check(reason(invalid([](auto &,auto &a){auto p=std::make_shared<PfsmProblem>(*a.problem);p->radiation.lines[0].wavelength=WavelengthAngstrom{.7107};a.problem=p;}),"unsupported_rir_radiation"));
    check(reason(invalid([](auto &,auto &a){a.baseline.diagnostics.kkt_checked=false;}),"unverified_joint_fit"));
    auto nonfinite=invalid([](auto &,auto &a){a.baseline.parameters.scales[1]=std::numeric_limits<double>::infinity();});
    check(!nonfinite.available() && reason(nonfinite,"invalid_scale") && nonfinite.issues[0].phase_id=="B");
    // Prescribe exact binary coefficients for this zero-scale unit scenario;
    // solver-returned single-phase normalization may legitimately be 100+1 ULP.
    auto zero=invalid([](auto &s,auto &a){
        a.baseline.parameters.scales={0,3};s.phases[0].scale=0;s.phases[1].scale=3;
    });
    check(zero.available() && zero.phases[0].weight_percent==0 && zero.phases[1].weight_percent==100);
    w.session().beginPreprocessingEdit(); check(!w.revisePfsmRir(w.session().snapshot(),"A",edit));
    w.session().cancelPreprocessingEdit(w.session().snapshot());
    w.searches().close(); check(!w.revisePfsmRir(w.session().snapshot(),"A",edit));
    const auto ended=take(prepareSearchMethod(full,SearchMethod::classic_sm,true)); check(!ended.pfsm);
    std::cout<<"PFSM/RIR source, analytic mixture and revision contracts passed\n";
} catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
