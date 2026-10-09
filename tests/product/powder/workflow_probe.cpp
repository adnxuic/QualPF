#include "infrastructure/io/powder_source_reader.h"
#include "gui/import/powder_cif_import_service.h"
#include "infrastructure/serialization/powder_cif_provenance.h"
#include "gui/search/searchmatch.h"
#include "tests/support/workspace_fixture.h"
#include "gui/state/preprocessing_edit.h"
#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "observation.h"
#include <QApplication>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>

int main(int argc,char **argv) {
    QApplication app(argc,argv);
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    using namespace qualpf::io::powder_cif;
    try {
        phase25b::check(argc==4,"Input output database prefix");
        const QString output=QString::fromLocal8Bit(argv[2]);
        QTemporaryDir settings(QFileInfo(output).absolutePath()+"/settings-XXXXXX");
        phase25b::check(settings.isValid(),"Private settings");
        QCoreApplication::setOrganizationName("Phase25B");QCoreApplication::setApplicationName("ImportedWorkflow");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        auto &session=AppState::searchSession();phase25b::seed(session);
        qualpf::infrastructure::PowderSourceReader reader;
        PowderCifImportService service(session,reader);
        auto draft=service.prepare(QString::fromLocal8Bit(argv[1]));
        phase25b::check(std::holds_alternative<PowderCifImportDraft>(draft),"Production source preparation");
        auto complete=service.complete(std::get<PowderCifImportDraft>(draft));
        phase25b::check(std::holds_alternative<PreparedPowderCifImport>(complete),"Production canonical conversion");
        phase25b::check(bool(service.commit(std::get<PreparedPowderCifImport>(complete))),"Production import commit");
        const auto imported=session.snapshot();const auto immutable=phase25b::pattern(imported->pattern);
        phase25b::check(imported->pattern.observed.size()>100 && imported->candidates.empty(),"Defined large imported fixture/reset");
        QJsonArray steps;
        int commits=0;QObject::connect(&session,&SessionView::changed,[&] { ++commits; });
        auto apply=[&](const char *name,const PreprocessingRequest &request) {
            const auto before=session.snapshot();const int oldCommits=commits;
            PreprocessingEdit edit(session);
            auto preview=edit.preview(request);
            if (!preview) throw std::runtime_error(std::string(name)+": "+preview.error().reason+" "+preview.error().message);
            phase25b::check(session.snapshot()==before,"Private preprocessing preview");
            phase25b::check(bool(edit.commit()) && commits==oldCommits+1 &&
                session.snapshot()->revision==before->revision+1,"One preprocessing publication");
            phase25b::check(bool(validateSearchState(*session.snapshot())),"Canonical post-processing state");
            phase25b::check(session.snapshot()->powderCifProvenance==imported->powderCifProvenance &&
                session.snapshot()->projectMetadata["powder_cif_provenance"]==imported->projectMetadata["powder_cif_provenance"],"Immutable import provenance survives processing");
            steps.append(QJsonObject{{"operation",name},{"revision",double(session.snapshot()->revision)},
                {"samples",double(session.snapshot()->pattern.observed.size())},{"peaks",session.snapshot()->peaks.tth.size()}});
        };
        PreprocessingRequest request;
        request.wavelength=imported->pattern.radiation.lines.front().wavelength;
        request.tolerance=AngularWidthDegrees{.15};
        request.background=preprocessing::BackgroundOptions{};
        request.background->method=preprocessing::BackgroundMethod::spline;
        apply("background",request);
        phase25b::check(session.snapshot()->pattern.background.has_value(),"Aligned background retained");
        request.background.reset();request.smoothing=preprocessing::SmoothingOptions{};
        apply("smoothing",request);
        request.smoothing.reset();request.peaks=preprocessing::PeakOptions{};request.peaks->search_range=-1;
        apply("peaks",request);
        const auto searched=session.snapshot();
        phase25b::check(searched->peaks.valid && !searched->peaks.tth.empty() && bool(searchPeaks(searched->peaks)),"Valid regenerated Search input");
        phase25b::check(AppState::db().openDatabases(QString::fromLocal8Bit(argv[3])),"Reference database available");
        auto builder=buildSearchMatchQuery(AppState::context(), AppState::peaks());QVector<CardType> cards;
        const auto searchInput=session.snapshot();
        phase25b::check(searchInput->searchOptions.size()>0 &&
            phase25b::pattern(searchInput->pattern)==phase25b::pattern(searched->pattern) &&
            searchInput->peaks.tth==searched->peaks.tth,"Existing Search settings snapshot preserves measured input");
        AppState::searches().makeQueryStrongest(builder,cards);
        phase25b::check(AppState::searches().searchError().isEmpty() && session.snapshot()==searchInput,"Search reads owning immutable snapshot");
        QJsonArray scores;
        for (const auto &card : cards) {
            phase25b::check(std::isfinite(card.getFom()) && std::isfinite(card.getFomPeakPos()) &&
                std::isfinite(card.getFomIntensity()) && std::isfinite(card.getScale()),"Finite candidates");
            scores.append(QJsonObject{{"id",card.getId()},{"fom",card.getFom()},{"scale",card.getScale()}});
        }
        session.replaceCandidates(cards);
        const auto candidates=session.snapshot();
        phase25b::check(candidates->candidates.size()==cards.size() && candidates->phases.empty(),"Explicit candidate publication/no stale phases");
        request.peaks.reset();const auto &p=candidates->pattern;
        request.crop_range=AngularRange{p.two_theta[20],p.two_theta[p.two_theta.size()-21]};
        apply("crop",request);
        phase25b::check(session.snapshot()->candidates.empty() && session.snapshot()->phases.empty(),"Crop invalidates candidate authority");
        request.crop_range.reset();request.strip_kalpha2=true;
        apply("kalpha2-cpp",request);
        phase25b::check(session.snapshot()->pattern.processing.kalpha2_subtracted==true && session.snapshot()->peaks.tth.empty(),"C++ stripping invalidates derived peaks");
        phase25b::check(phase25b::pattern(imported->pattern)==immutable,"Old imported handle unchanged");
        AppState::db().closeDatabeses();
        phase25b::save(output,QJsonObject{{"status","PASS"},{"steps",steps},{"candidates",scores},
            {"imported_pattern",immutable},{"final_pattern",phase25b::pattern(session.snapshot()->pattern)},
            {"source_provenance",encodePowderCifProvenance(*session.snapshot()->powderCifProvenance)},
            {"expected_numeric_from_cpp",false},{"coverage","ownership/workflow properties; numerical authority remains original Phase9/10/12"}});
        return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n';return 1; }
}
