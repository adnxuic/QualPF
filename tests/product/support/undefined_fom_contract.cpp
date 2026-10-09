// Product adapter contract: neither backend may call native undefined arithmetic.
#include "gui/state/search_session.h"
#include "qualpf/domain/radiation.h"
#include <QCoreApplication>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application; using namespace qualpf::gui;
void check(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
CardType card(const char *id,double angle,double intensity) {
    CardType result;
    result.setId(id);
    result.setD({radiation::CU_KA1_WAVE/(2*std::sin(angle*std::numbers::pi/360))},radiation::CU_KA1_WAVE);
    result.setIntensity({intensity});result.setScale(1);
    return result;
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        for (auto backend:{SearchBackend::cpp,SearchBackend::legacy_fallback}) {
            qualpf::application::Workspace sessionOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView session(sessionOwner.session());
            SessionStateView state;
            state.backend=backend;
            state.pattern.id=DatasetId{1};
            state.pattern.two_theta={TwoThetaDegrees{0},TwoThetaDegrees{20},TwoThetaDegrees{40},TwoThetaDegrees{60}};
            state.pattern.observed={0,100,80,0};
            state.pattern.calculation_range=AngularRange{TwoThetaDegrees{0},TwoThetaDegrees{60}};
            state.peaks.tth={20,40};state.peaks.d={4,2};state.peaks.deltaD={.1,.1};
            state.peaks.fwhm={.2,.3};state.peaks.intensity={100,80};state.peaks.intensityOrig={100,80};
            state.peaks.wave=radiation::CU_KA1_WAVE;state.peaks.valid=true;
            check(bool(session.commit(session.snapshot(),state)),"Fixture state rejected");
            const auto before=session.snapshot();
            SearchParameters parameters;
            parameters.delta_two_theta=AngularWidthDegrees{.25};
            parameters.weight_position=parameters.weight_intensity=parameters.weight_phases=.5;
            auto unmatched=card("unmatched",50,10);
            for (const auto &c:{card("out-of-range",100,10),unmatched}) {
                const auto result=scoreCard(*before,c,parameters);
                check(bool(result),"Valid unmatched input rejected");
                const auto &s=result.value();
                check(s.total==0 && s.position==0 && s.intensity==0 && s.scale==1,"Unmatched score not initialized");
            }
            auto zero=card("zero-intensity",20,0);
            const auto invalid=scoreCard(*before,zero,parameters);
            check(!invalid && invalid.error().code==ErrorCode::numerical_failure &&
                invalid.error().reason=="zero_total_intensity","Zero intensity did not fail before fallback");
            auto batch=*before;batch.candidates={unmatched,zero};
            check(!rescoreCards(batch,parameters),"Undefined candidate published a partial result");
            parameters.weight_position=parameters.weight_intensity=parameters.weight_phases=0;
            const auto weights=scoreCard(*before,card("zero-weight",20,10),parameters);
            check(!weights && weights.error().reason=="zero_effective_weight","Zero weights did not fail before fallback");
            check(weights.error().message.find(weights.error().reason)!=std::string::npos,
                "GUI/CLI message lost machine reason");
            check(session.snapshot()==before,"Failed/unmatched score mutated Session");
        }
        std::cout<<"PASS: four FOM contracts under C++ and explicit Legacy; no native dataset initialized\n";
    } catch(const std::exception &e) { std::cerr<<e.what()<<'\n';return 1; }
}
