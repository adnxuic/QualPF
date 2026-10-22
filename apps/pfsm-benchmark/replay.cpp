// Validation-only: consumes frozen physical state, never formal ranks or truth.
#include "candidate_recall_diagnostics.h"
#include "pfsm_build_revision.h"
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include "qualpf/numerics/runtime.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <algorithm>
#include <cfenv>
#include <chrono>
#include <cmath>
#include <limits>
#include <csignal>
#include <fstream>
#include <iostream>
#include <map>
#include <omp.h>
#include <set>
#include <sstream>
#include <thread>
using namespace qualpf;
using namespace qualpf::profile;
using namespace qualpf::application;
namespace {
volatile std::sig_atomic_t interrupted=0;
void interrupt(int){interrupted=1;}
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double>(Clock::now()-t).count();}
[[noreturn]] void fail(const std::string &s){throw std::runtime_error(s);}
template<class T>T take(Result<T> result){if(!result)fail(result.error().field+": "+result.error().message);return std::move(result).value();}
QByteArray bytes(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))fail("Cannot read "+path.toStdString());return f.readAll();}
QString digest(const QString &path){QFile f(path);if(!f.open(QIODevice::ReadOnly))fail("Cannot hash "+path.toStdString());QCryptographicHash h(QCryptographicHash::Sha256);if(!h.addData(&f))fail("Hash failed");return QString::fromLatin1(h.result().toHex());}
QJsonObject object(const QString &path){QJsonParseError e;auto d=QJsonDocument::fromJson(bytes(path),&e);if(e.error!=QJsonParseError::NoError || !d.isObject())fail("Invalid JSON input");return d.object();}
QString boundPath(const QJsonValue &value){const auto o=value.toObject();auto p=o["path"].toString();if(p.isEmpty() || digest(p)!=o["sha256"].toString())fail("Frozen input binding mismatch: "+p.toStdString());return p;}
void save(const QString &path,const QJsonObject &o){if(QFile::exists(path))fail("Refusing to overwrite evidence");QSaveFile f(path);if(!f.open(QIODevice::WriteOnly))fail("Cannot save evidence");const auto b=QJsonDocument(o).toJson();if(f.write(b)!=b.size() || !f.commit())fail("Cannot commit evidence");}
QJsonArray indices(const std::vector<std::size_t> &v){QJsonArray a;for(auto i:v)a.append(qint64(i));return a;}
QJsonArray values(const std::vector<double> &v){QJsonArray a;for(auto i:v)a.append(i);return a;}
QJsonObject physical(const PfsmCandidateRecallPhysicalFit &f){return {{"q",f.q},{"gain",f.gain},{"difference_gain",f.difference_gain},{"kkt",f.kkt},{"baseline_fallback",f.baseline_fallback},{"approximated",f.approximated},{"scales",values(f.parameters.scales)},{"shifts",values(f.parameters.shifts)}};}
std::vector<double> numbers(const QJsonValue &v){if(!v.isArray())fail("Missing parameter array");std::vector<double> out;for(const auto &x:v.toArray()){if(!x.isDouble() || !std::isfinite(x.toDouble()))fail("Invalid parameter");out.push_back(x.toDouble());}return out;}
struct Observed {std::shared_ptr<ProfileObservations> data=std::make_shared<ProfileObservations>();std::vector<double> residual;};
Observed observations(const QString &path){
    Observed out;std::istringstream stream(bytes(path).toStdString());std::string line;
    while(std::getline(stream,line)){
        if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);double x,y,b,w,model,r;std::string extra;
        if(!(row>>x>>y>>b>>w>>model>>r) || row>>extra)fail("Invalid frozen observations");
        for(double value:{x,y,b,w,model,r})if(!std::isfinite(value))fail("Nonfinite frozen observations");
        out.data->coordinates.push_back(x);out.data->observed.push_back(y);out.data->background.push_back(b);
        out.data->weights.push_back(w);out.residual.push_back(r);
    }
    return out;
}
QJsonObject recallJson(const PfsmCandidateRecallEvidence &e){
    QJsonObject row{{"original_index",qint64(e.original_index)},{"recalled",e.recalled},{"original_shift",e.original_shift},
        {"residual_shift",e.residual_shift},{"recall_reason",QString::fromStdString(e.recall_reason)},
        {"protection_reason",QString::fromStdString(e.protection_reason)}};
    if(e.original_score)row["original_score"]=*e.original_score;if(e.residual_score)row["residual_score"]=*e.residual_score;
    if(e.original_rank)row["original_rank"]=qint64(*e.original_rank);if(e.residual_rank)row["residual_rank"]=qint64(*e.residual_rank);
    return row;
}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QCommandLineParser cli;cli.addHelpOption();
    for(const auto *key:{"input","output","threads","stage"})cli.addOption(QCommandLineOption(key,key,key));cli.process(app);
    try {
        if(!cli.isSet("input") || !cli.isSet("output"))fail("Requires input and output");
        const auto input=object(cli.value("input"));
        if(input["schema"]!="qualpf.pfsm.fast-replay-input.v1")fail("Unknown replay input schema");
        const auto stage=cli.isSet("stage")?cli.value("stage"):"abc";
        if(stage!="a" && stage!="bc" && stage!="abc")fail("Stage must be a, bc or abc");
        bool ok=true;const int threads=cli.isSet("threads")?cli.value("threads").toInt(&ok):12;
        if(!ok || threads<1)fail("Invalid thread count");
        const auto output=cli.value("output");if(QFile::exists(output) || !QDir().mkpath(output))fail("Output must be a new directory");
        take(numerics::initializeLinearAlgebraCacheSizes({49152,1310720,25165824}));
        std::signal(SIGINT,interrupt);std::signal(SIGTERM,interrupt);std::stop_source stop;
        std::jthread watcher([&](std::stop_token done){while(!done.stop_requested()){if(interrupted)stop.request_stop();std::this_thread::sleep_for(std::chrono::milliseconds(50));}});
        ExecutionContext ctx{stop.get_token(),NumericalPolicy::explicit_float64,{}};
        const auto db=input["database"].toObject();const auto databaseFile=boundPath(db[".sq"]);
        for(const auto *suffix:{".sq.info",".sq.infostat",".sq.search"})boundPath(db[suffix]);
        const auto orderPath=boundPath(input["candidate_order"]);
        std::vector<std::string> order;std::istringstream orderStream(bytes(orderPath).toStdString());std::string id;
        while(std::getline(orderStream,id)){if(!id.empty() && id.back()=='\r')id.pop_back();if(!id.empty())order.push_back(id);}
        if(std::set<std::string>(order.begin(),order.end()).size()!=order.size())fail("Duplicate frozen identity");
        CandidateQuery query;query.route=CandidateQuery::Route::constrained_browse;query.strictPairedReflections=true;query.wavelength=0;
        const auto queryInput=input["query"].toObject();query.elements=queryInput["elements"].toString().toStdString();
        if(!query.elements.empty())query.elementOperator=CandidateQuery::ElementOperator::any;
        const ReferenceDatabase database{databaseFile.chopped(3).toStdString(),1};
        infrastructure::SqlCandidateReaderFactory factory;auto guard=take(factory.protect(database));auto reader=take(factory.open(database,1,ctx));
        auto t=Clock::now();const auto cards=take(reader->read(query,ctx,{}));const double sqlSeconds=elapsed(t);
        if(cards.size()!=order.size())fail("Frozen candidate count disagrees");
        std::map<std::string,std::size_t> identities;
        for(std::size_t i=0;i<cards.size();++i){if(cards[i].id!=order[i] || cards[i].d.size()!=cards[i].intensity.size())fail("Frozen candidate order/reflections disagree");identities.emplace(cards[i].id,i);}
        Radiation radiation;for(const auto &v:input["radiation"].toArray()){auto o=v.toObject();radiation.lines.push_back({WavelengthAngstrom{o["wavelength_angstrom"].toDouble()},o["weight"].toDouble()});}
        const double h=input["width"].toDouble(),lo=input["lower_shift"].toDouble(),hi=input["upper_shift"].toDouble();
        const bool cache=input["template_cache"].toBool();
        std::shared_ptr<const PfsmCandidateRecallData> data;
        std::shared_ptr<const PfsmIntegralData> integralData;
        std::shared_ptr<const ProfileObservations> frozenData;
        QJsonArray reports;
        save(output+"/run-start.json",{{"schema","qualpf.pfsm.fast-replay.v1"},{"code_revision",QUALPF_PFSM_REVISION},
            {"input_sha256",digest(cli.value("input"))},{"executable_sha256",digest(QCoreApplication::applicationFilePath())},
            {"stage",stage},{"formal_fits",0},{"sql_seconds",sqlSeconds},{"threads",threads}});
        std::fenv_t environment;std::fegetenv(&environment);
        const auto workers=[&](std::size_t n,auto work){
            std::vector<std::string> errors(n);
            const int workerThreads=threads;
#pragma omp parallel for schedule(dynamic,1) num_threads(workerThreads)
            for(std::ptrdiff_t slot=0;slot<static_cast<std::ptrdiff_t>(n);++slot){
                std::fenv_t previous;std::fegetenv(&previous);std::fesetenv(&environment);
                try{if(ctx.cancellation.stop_requested())fail("Replay cancelled");work(static_cast<std::size_t>(slot));}
                catch(const std::exception &e){errors[static_cast<std::size_t>(slot)]=e.what();}
                catch(...){errors[static_cast<std::size_t>(slot)]="Unknown replay failure";}
                std::fesetenv(&previous);
            }
            for(const auto &e:errors)if(!e.empty())fail(e);
        };
        for(const auto &roundValue:input["rounds"].toArray()){
            const auto frozen=roundValue.toObject();const int number=frozen["round"].toInt(-1);if(number<0)fail("Invalid round");
            const auto roundStart=Clock::now();std::cout<<"Replay "<<input["case"].toString().toStdString()<<" round "<<number<<std::endl;
            const auto observed=observations(boundPath(frozen["observations"]));
            if(frozenData && (frozenData->coordinates!=observed.data->coordinates || frozenData->observed!=observed.data->observed ||
                frozenData->background!=observed.data->background || frozenData->weights!=observed.data->weights))fail("Frozen observations changed across rounds");
            if(!frozenData)frozenData=observed.data;
            std::string resourceReason;
            if(!data) {
                auto prepared=preparePfsmCandidateRecallData(frozenData,h,lo,hi,ctx);
                if(prepared)data=std::move(prepared).value();
                else if(prepared.error().code==ErrorCode::numerical_failure &&
                    (prepared.error().reason=="candidate_recall_grid_limit" || prepared.error().reason=="candidate_recall_scan_limit"))
                    resourceReason=prepared.error().reason;
                else take(std::move(prepared));
            }
            const auto expansion=[&](std::size_t i){ProfileExpansionInput p;p.fwhm_degrees=h;p.radiation=radiation;
                if(cache)p.interpolation_window={frozenData->coordinates.front()-hi,frozenData->coordinates.back()-lo};
                for(std::size_t j=0;j<cards[i].d.size();++j)p.lines.push_back({j,cards[i].d[j],cards[i].intensity[j]});return p;};
            const auto scales=numbers(frozen["scales"]),shifts=numbers(frozen["shifts"]);const auto ids=frozen["selected_ids"].toArray();
            if(scales.size()!=shifts.size() || scales.size()!=std::size_t(ids.size()))fail("Unaligned frozen parameters");
            std::vector<PfsmCandidateRecallPhysicalPhase> selected;std::set<std::size_t> selectedIndices;
            for(std::size_t j=0;j<scales.size();++j){auto it=identities.find(ids[qsizetype(j)].toString().toStdString());if(it==identities.end() || !selectedIndices.insert(it->second).second)fail("Invalid selected identity");selected.push_back({expansion(it->second),scales[j],shifts[j]});}
            std::vector<std::size_t> pool;for(std::size_t i=0;i<cards.size();++i)if(!selectedIndices.contains(i))pool.push_back(i);
            if(!resourceReason.empty()) {
                PfsmIntegralRecallReport d;d.baseline_shifts=shifts;
                std::vector<ProfileExpansionInput> oldInputs;
                for(const auto &phase:selected){oldInputs.push_back(phase.input);oldInputs.back().interpolation_window.reset();}
                t=Clock::now();
                if(!integralData)integralData=take(preparePfsmIntegralData(frozenData,{},h,d.options,ctx));
                const auto integralRound=take(preparePfsmIntegralRound(integralData,oldInputs,shifts,observed.residual,{},ctx));
                const auto relaxedRound=take(preparePfsmRelaxedRound(integralData,oldInputs,shifts,lo,hi,{},d.relaxed_options,ctx));
                d.shared_seconds=elapsed(t);d.fine_bins=integralData->fine.weights.size();d.coarse_bins=integralData->coarse.weights.size();
                d.fine_denominator=integralData->fine.denominator;d.coarse_baseline_q=integralRound.coarse_baseline_q;
                d.fine_baseline_q=integralRound.fine_baseline_q;d.tau=integralRound.tau;
                d.relaxation_radius=relaxedRound.radius;d.bank_columns=relaxedRound.bank_columns;
                d.peak_buffer_bytes=integralData->buffer_bytes+integralRound.buffer_bytes+relaxedRound.buffer_bytes;
                d.candidates.resize(pool.size());
                t=Clock::now();workers(pool.size(),[&](std::size_t i){
                    auto p=expansion(pool[i]);p.interpolation_window.reset();
                    d.candidates[i]=take(evaluatePfsmIntegralCandidate(integralRound,p,lo,hi,{},ctx));d.candidates[i].original_index=pool[i];
                });d.integral_seconds=elapsed(t);
                t=Clock::now();workers(pool.size(),[&](std::size_t i){
                    auto p=expansion(pool[i]);p.interpolation_window.reset();
                    d.candidates[i].relaxed=take(evaluatePfsmRelaxedCandidate(relaxedRound,p,d.candidates[i],ctx));
                });d.relaxed_seconds=elapsed(t);
                std::size_t workerBytes=0;
                for(auto &e:d.candidates){
                    if(!e.relaxed.uncertainty.empty())e.uncertainty.push_back(e.relaxed.uncertainty);
                    e.nnls_iterations+=e.relaxed.iterations;
                    d.coarse_evaluations+=e.coarse_evaluations;d.fine_evaluations+=e.fine_evaluations;d.relaxed_evaluations+=e.relaxed.evaluations;
                    workerBytes=std::max(workerBytes,e.buffer_bytes+e.relaxed.buffer_bytes);
                }
                d.peak_buffer_bytes+=std::size_t(threads)*workerBytes;
                take(selectPfsmIntegralCandidates(d.candidates,d.options,ctx));
                const auto retained=take(selectPfsmRelaxedCandidates(d.candidates,d.relaxed_options,ctx)).indices;
                QFile evidence(output+QString("/replay-%1.jsonl").arg(number));
                if(!evidence.open(QIODevice::WriteOnly|QIODevice::NewOnly))fail("Cannot create integral replay evidence");
                for(const auto &e:d.candidates){
                    QJsonObject row{{"id",QString::fromStdString(cards[e.original_index].id)},{"original_index",qint64(e.original_index)},
                        {"recalled",e.retained},{"retained",e.retained},{"integral",pfsmIntegralRecallCandidateJson(e)},
                        {"original_shift",e.original.shift},{"residual_shift",e.residual.shift},
                        {"protection_reason",e.uncertainty.empty()?QString():QString::fromStdString(e.uncertainty.front())}};
                    if(e.original.score)row["original_score"]=*e.original.score;if(e.residual.score)row["residual_score"]=*e.residual.score;
                    const auto b=QJsonDocument(row).toJson(QJsonDocument::Compact)+'\n';if(evidence.write(b)!=b.size())fail("Cannot write integral replay evidence");
                }
                evidence.close();
                QJsonObject report{{"round",number},{"backend","bounded_integral_relaxed"},{"switch_reason",QString::fromStdString(resourceReason)},
                    {"integral",pfsmIntegralRecallReportJson(d)},{"formal_fits",0},{"pool",indices(pool)},
                    {"recalled",indices(retained)},{"recalled_indices",indices(retained)},{"retained_indices",indices(retained)},
                    {"full_fallback",false},{"fallback_reason",""},{"fast_seconds",elapsed(roundStart)},
                    {"evidence_sha256",digest(evidence.fileName())}};
                save(output+QString("/round-%1.json").arg(number),report);reports.append(report);continue;
            }
            t=Clock::now();const auto round=take(preparePfsmCandidateRecallPhysicalRound(data,observed.residual,selected,ctx,false));
            const double sharedSeconds=elapsed(t);
            const double expected=frozen["baseline_Q"].toDouble()/data->denominator;
            if(std::abs(round.baseline_q-expected)>128*std::numeric_limits<double>::epsilon()*std::max({1.,expected,round.baseline_q}))fail("Frozen baseline objective disagrees");
            std::vector<PfsmCandidateRecallEvidence> recall(pool.size());
            t=Clock::now();workers(pool.size(),[&](std::size_t i){recall[i].original_index=pool[i];auto result=scorePfsmCandidateRecallCandidate(round.recall,expansion(pool[i]),pool[i],ctx,true);
                if(result)recall[i]=std::move(result).value();else if(result.error().code==ErrorCode::numerical_failure)recall[i].protection_reason=result.error().message;else take(std::move(result));});
            const auto recalled=selectPfsmRecallCandidates(recall);const double recallSeconds=elapsed(t);
            std::vector<std::size_t> recalledSlots;for(std::size_t i=0;i<recall.size();++i)if(recall[i].recalled)recalledSlots.push_back(i);
            QJsonObject report{{"round",number},{"backend","v2"},{"baseline_q",round.baseline_q},{"denominator",data->denominator},
                {"formal_fits",0},{"pool",indices(pool)},{"recalled",indices(recalled)},{"shared_seconds",sharedSeconds},{"recall_seconds",recallSeconds}};
            std::vector<PfsmCandidateRecallBackprojectionResult> a(recalledSlots.size());std::vector<PfsmCandidateRecallEvidence> aRows;
            if(stage!="bc"){
                t=Clock::now();const auto ablation=take(preparePfsmCandidateRecallPhysicalRound(data,observed.residual,selected,ctx,true));
                for(auto slot:recalledSlots)aRows.push_back(recall[slot]);
                workers(recalledSlots.size(),[&](std::size_t i){auto &e=aRows[i];if(!e.protection_reason.empty())return;
                    auto fit=fitPfsmCandidateRecallBackprojection(ablation,expansion(e.original_index),e,ctx);
                    if(fit){a[i]=std::move(fit).value();e.gain=a[i].best.gain;e.local_q=a[i].best.q;}
                    else if(fit.error().code==ErrorCode::numerical_failure)e.protection_reason=fit.error().message;else take(std::move(fit));});
                report["a_top20"]=indices(selectPfsmRecallAblationCandidates(aRows));report["a_seconds"]=elapsed(t);
            }
            std::vector<PfsmCandidateRecallCandidate> bc(recalledSlots.size());
            if(stage!="a"){
                t=Clock::now();workers(recalledSlots.size(),[&](std::size_t i){const auto &e=recall[recalledSlots[i]];bc[i]=take(evaluatePfsmCandidateRecallInitial(round,expansion(e.original_index),e,ctx));});
                auto selection=preparePfsmCandidateRecallReview(bc);report["initial_seconds"]=elapsed(t);t=Clock::now();
                if(!selection.review_skipped)workers(recalledSlots.size(),[&](std::size_t i){auto &e=bc[i];if(!std::binary_search(selection.review.begin(),selection.review.end(),e.recall.original_index))return;
                    const auto p=expansion(e.recall.original_index);e=take(reviewPfsmCandidateRecall(round,p,std::move(e),ctx));});
                finalizePfsmCandidateRecall(bc,selection,pool);report["bc"]=pfsmCandidateRecallSelectionJson(selection);report["review_seconds"]=elapsed(t);
            }
            report["fast_seconds"]=elapsed(roundStart);
            QFile evidence(output+QString("/replay-%1.jsonl").arg(number));if(!evidence.open(QIODevice::WriteOnly|QIODevice::NewOnly))fail("Cannot create replay evidence");
            std::size_t recalledSlot=0;
            for(const auto &e:recall){auto row=recallJson(e);row["id"]=QString::fromStdString(cards[e.original_index].id);
                if(e.recalled){const auto i=recalledSlot++;
                    if(stage!="a")row["bc"]=pfsmCandidateRecallCandidateJson(bc[i]);
                    if(stage!="bc"){
                        QJsonArray trials;for(const auto &v:a[i].trials)trials.append(QJsonObject{{"linear_q",v.linear_q},{"initial_shift",v.candidate_initial_shift},{"coefficients",values(v.coefficients)},{"physical",physical(v.physical)}});
                        QJsonObject physicalRow{{"best",physical(a[i].best)},{"trials",trials},{"nnls_calls",qint64(a[i].nnls_calls)},
                            {"protected",QString::fromStdString(aRows[i].protection_reason)},{"retained",aRows[i].retained}};
                        if(aRows[i].local_rank)physicalRow["rank"]=qint64(*aRows[i].local_rank);row["a"]=physicalRow;
                    }
                }
                const auto b=QJsonDocument(row).toJson(QJsonDocument::Compact)+'\n';if(evidence.write(b)!=b.size())fail("Cannot write replay evidence");
            }
            evidence.close();report["evidence_sha256"]=digest(evidence.fileName());save(output+QString("/round-%1.json").arg(number),report);reports.append(report);
        }
        auto final=object(output+"/run-start.json");final["rounds"]=reports;final["completed"]=true;save(output+"/run.json",final);return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
