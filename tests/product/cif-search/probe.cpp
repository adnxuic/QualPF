#include "tests/support/project_peaks.h"
#include "tests/product/support/product_startup.h"
// Test-only observer linked to the frozen product objects. No product math edits.
#define main historical_product_probe_main
#include "product_workflow_probe.cpp"
#undef main
#include <QCryptographicHash>

namespace {
QJsonArray numbers(const QVector<double>& values) {
  QJsonArray result; for (double v:values) result.append(v); return result;
}
QString connection(const QString& suffix) {
  QString found;
  for (const auto &name:QSqlDatabase::connectionNames())
    if (name.endsWith(suffix)) { require(found.isEmpty(),"Ambiguous connection"); found=name; }
  require(!found.isEmpty(),"Missing connection"); return found;
}
QJsonObject scoreJson(const qualpf::ScoreValues& s) {
  return {{"total",s.total},{"position",s.position},{"intensity",s.intensity},{"scale",s.scale}};
}
QJsonObject observe(MainWindow &window) {
  const auto state=AppState::searchSession().snapshot();
  const auto p=currentSearchParameters(AppState::context());
  auto windows=qualpf::gui::peaksWithTolerance(state->peaks,p.delta_two_theta.value);
  require(bool(windows),"Effective windows");
  auto query=buildSearchMatchQuery(AppState::context(), state->peaks);
  query.setCalcFom(false);
  query.setDValues(windows.value().d,windows.value().deltaD);
  query.enableDeleted(SearchOptionsDialog::savedCheckDeleted(AppState::workspace().session().snapshot(), AppState::context().settings()));
  ProductWorkflowProbe::restraints(window,query);
  QVector<CardType> eligible, prefiltered;
  AppState::searches().makeQueryWithoutStrongest(query,eligible);
  require(AppState::searches().searchError().isEmpty(),"Eligibility query");
  AppState::searches().makeQueryStrongest(query,prefiltered);
  require(AppState::searches().searchError().isEmpty(),"Strongest query");
  QSet<QString> eligibleIds,prefilteredIds;
  QJsonArray eligibleOrder,prefilteredOrder;
  for (const auto &c:eligible) {eligibleIds.insert(c.getId());eligibleOrder.append(c.getId());}
  for (const auto &c:prefiltered) {prefilteredIds.insert(c.getId());prefilteredOrder.append(c.getId());}
  QSqlQuery all(QSqlDatabase::database(connection(".sq")));
  require(all.exec("SELECT id,quality,dvalue,intensita FROM id"),"All candidate query");
  QSqlQuery top(QSqlDatabase::database(connection(".sq.search")));
  auto peaks=qualpf::gui::searchPeaks(state->peaks);
  require(bool(peaks),"Search peak projection");
  QJsonArray candidates;
  while(all.next()) {
    const QString id=all.value(0).toString();
    auto parse=[](const QString& s) {QVector<double> a;for(const auto &v:s.split(',',Qt::SkipEmptyParts))a.append(v.toDouble());return a;};
    CardType c;c.setId(id);c.setD(parse(all.value(2).toString()),state->peaks.wave);c.setIntensity(parse(all.value(3).toString()));
    top.prepare("SELECT n,dval FROM top WHERE id=:id");top.bindValue(":id",id);
    require(top.exec()&&top.next(),"Top lookup");
    auto strong=SearchUtil::extractNumbers(top.value(1).toString(),top.value(0).toInt(),3);
    QJsonArray margins;
    for(double d:strong) {
      double best=-std::numeric_limits<double>::infinity();
      for(qsizetype i=0;i<windows.value().d.size();++i)
        best=std::max(best,windows.value().deltaD[i]-std::abs(d-windows.value().d[i]));
      margins.append(best);
    }
    const bool strongPass=SearchUtil::checkStrongValuesWithTolerance(windows.value().d,windows.value().deltaD,strong);
    QJsonObject item{{"id",id},{"quality",all.value(1).toString()},
      {"eligible",eligibleIds.contains(id)},{"strongest_pass",strongPass},{"prefiltered",prefilteredIds.contains(id)},
      {"stored_top",numbers(strong)},{"strongest_margins",margins},
      {"d",numbers(c.getD())},{"intensities",numbers(c.getIntensity())},{"two_theta",numbers(c.getTth())}};
    auto reference=qualpf::gui::searchCard(c);
    auto trace=qualpf::search::score_classic({state->pattern,peaks.value(),reference,p},
        {{},qualpf::NumericalPolicy::legacy_mixed_precision,"phase19c-observer"});
    if(trace) {
      const auto &t=trace.value();
      item["score"]=scoreJson(t.score);
      item["phase_contribution"]=t.sqrt_sample;
      item["weighted_phase_contribution"]=p.weight_phases*t.sqrt_sample;
      item["associated_peaks"]=int(t.associated_peaks);
      item["expected_peaks_per_phase"]=int(t.expected_peaks_per_phase);
      item["disposition"]=int(t.disposition);
      item["threshold_margin"]=t.score.total-p.minimum_score;
      item["threshold_pass"]=t.score.total>p.minimum_score;
      auto actual=qualpf::gui::scoreCard(*state,c,p);
      require(bool(actual),"Product score failed for defined trace");
      item["product_score"]=scoreJson(actual.value());
    } else {
      item["score_error"]=QString::fromStdString(trace.error().reason);
      item["threshold_pass"]=false;
    }
    candidates.append(item);
  }
  auto ranked=state->candidates;
  std::stable_sort(ranked.begin(),ranked.end(),[](const auto &a,const auto &b){return a.getFom()>b.getFom();});
  QJsonArray ties;
  for(qsizetype i=1;i<ranked.size();++i)
    if(ranked[i-1].getFom()==ranked[i].getFom())ties.append(QJsonArray{ranked[i-1].getId(),ranked[i].getId()});
  return {{"peaks",qualpf::gui::encode_project_peaks(state->peaks)},
      {"delta",p.delta_two_theta.value},{"minimum_fom",p.minimum_score},{"maximum_entries",int(p.maximum_entries)},
      {"weights",QJsonArray{p.weight_position,p.weight_intensity,p.weight_phases}},
      {"effective_d",numbers(windows.value().d)},{"effective_delta_d",numbers(windows.value().deltaD)},
      {"eligible_order",eligibleOrder},{"prefiltered_order",prefilteredOrder},{"all_cards",candidates},
      {"candidate_order",scores(state->candidates)},{"complete_ranking",scores(ranked)},{"ties",ties}};
}
}

int main(int argc,char **argv) {
  QApplication app(argc,argv);
  QTemporaryDir settings;
  QCoreApplication::setApplicationName("Phase19CQualification");
  QSettings::setDefaultFormat(QSettings::IniFormat);
  QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
  try {
    require(argc==5,"Usage: observer database project tables output");
    require(AppState::db().openDatabases(QString::fromLocal8Bit(argv[1])),"Database open");
    const auto tablePath=QString::fromLocal8Bit(argv[3]);
    MainWindow::setPathDataFiles(tablePath);
    MainWindow window(AppState::context());
    qualpf::application::StartupOptions options{0,1,0,-1};int error=0;
    const auto tables=QFile::encodeName(QDir(tablePath).absolutePath()+"/");
    qualpf::legacy::initialize_product_presentation_for_probe(&options,tables.constData(),tables.size(),&error);
    require(error==0,"Runtime initialization");
    ProductWorkflowProbe::load(window,QString::fromLocal8Bit(argv[2]));
    auto *results=window.findChild<DbResultsWidget *>();
    require(results!=nullptr,"Results widget");
    QJsonArray stages;
    for(int stage=0;stage<3;++stage) {
      if(AppState::peaks().tth.isEmpty()) {
        stages.append(QJsonObject{{"stage",stage},{"terminal","empty residual"}});break;
      }
      ProductWorkflowProbe::search(window);
      require(AppState::searches().searchError().isEmpty(),"Product search");
      auto observed=observe(window);observed["stage"]=stage;
      if(stage<2 && !AppState::searchSession().snapshot()->candidates.isEmpty()) {
        auto ranked=AppState::searchSession().snapshot()->candidates;
        std::stable_sort(ranked.begin(),ranked.end(),[](const auto &a,const auto &b){return a.getFom()>b.getFom();});
        auto chosen=std::find_if(ranked.begin(),ranked.end(),[](const auto &candidate){
          for(const auto &phase:AppState::searchSession().snapshot()->phases)if(phase.getId()==candidate.getId())return false;
          return true;
        });
        if(chosen!=ranked.end()) {
          observed["acceptance_policy"]="Highest total score, stable source-order ties, not already accepted";
          observed["selected"]=scores({*chosen});
          observed["subtraction_input"]=QJsonObject{{"d",numbers(chosen->getD())},{"intensity",numbers(chosen->getIntensity())},{"scale",chosen->getScale()}};
          const auto before=AppState::searchSession().snapshot();
          results->selectCard(chosen->getId());results->acceptSelectedCards(); awaitSearch(window);
          const auto after=AppState::searchSession().snapshot();
          require(after->phases.size()==before->phases.size()+1,"Acceptance transaction");
          observed["accepted_phases"]=scores(after->phases);
          observed["residual_after_acceptance"]=qualpf::gui::encode_project_peaks(after->peaks);
          observed["rescore_after_acceptance"]=scores(after->candidates);
        } else observed["acceptance_unavailable"]="All candidates already accepted";
      } else if(stage<2) observed["acceptance_unavailable"]="No candidate";
      stages.append(observed);
    }
    write96(QString::fromLocal8Bit(argv[4]),{{"stages",stages},{"approved",false},{"default_cutover",false}});
    AppState::db().closeDatabeses();
    return 0;
  }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
