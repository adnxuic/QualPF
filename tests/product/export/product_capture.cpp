#include "infrastructure/serialization/metadata_values.h"
#include "tests/support/project_peaks.h"
// Real GUI/Application exporters only. No replacement formatter in this runner.
#include "qualpf/application/startup/application_initialization_service.h"
#include "tests/support/workspace_fixture.h"
#include "gui/state/preprocessing_edit.h"
#include "qualpf/domain/radiation.h"
#include "gui/mainwindow.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/export/pattern_export_service.h"
#include "infrastructure/io/project_peaks.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QLineEdit>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <QLocale>
#include <clocale>
#include <iostream>
#include <stdexcept>

class ProductWorkflowProbe {
public:
    static void save(MainWindow& w,const QString& p) { w.saveProject(p); }
    static void load(MainWindow& w,const QString& p) { w.loadProject(p); }
};
namespace {
QJsonObject native;
int dataCalls=0,xyCalls=0,cifCalls=0;
QString nativePath;
void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
QJsonArray values(const std::vector<double>& v) { QJsonArray a;for(auto x:v)a.append(x);return a; }
template<class T> QString bits(const T* p,std::size_t n) {
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(p),int(n*sizeof(T))).toHex());
}
QJsonObject doubles(const std::vector<double>& v) { return {{"values",values(v)},{"binary64_le",bits(v.data(),v.size())}}; }
QJsonObject canonical() {
    const auto s=AppState::searchSession().snapshot();const auto& p=s->pattern;
    std::vector<double> x;for(auto t:p.two_theta)x.push_back(t.value);
    QJsonArray radiation;for(auto l:p.radiation.lines)radiation.append(QJsonObject{{"wavelength",l.wavelength.value},{"weight",l.relative_weight}});
    QJsonObject result{{"x",doubles(x)},{"observed",doubles(p.observed)},{"source_id",QString::fromStdString(p.source_id)},
      {"radiation",radiation},{"metadata",qualpf::infrastructure::jsonValue(s->projectMetadata)},{"editing",s->editing},
      {"peaks",qualpf::gui::encode_project_peaks(s->peaks)}};
    if(p.background) result["background"]=QJsonObject{{"values",doubles(p.background->values)},
      {"first",p.background->window ? int(p.background->window->first) : -1}};
    if(p.original_observed)result["original_observed"]=doubles(*p.original_observed);
    if(p.calculated)result["calculated"]=doubles(p.calculated->values);
    if(p.calculation_range)result["calculation_range"]=QJsonArray{p.calculation_range->minimum.value,p.calculation_range->maximum.value};
    return result;
}
void write(const QString& path,const QByteArray& bytes) {
    QFile file(path);require(!file.exists() && file.open(QIODevice::WriteOnly),"New evidence path required");
    require(file.write(bytes)==bytes.size(),"Evidence write failed");
}
}
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    try {
        require(argc==3,"Usage: native_capture output-directory repository-root");
        const QString out=QString::fromLocal8Bit(argv[1]),root=QString::fromLocal8Bit(argv[2]);
        QDir().mkpath(out);QTemporaryDir settings(out+"/settings-XXXXXX");
        QCoreApplication::setOrganizationName("Phase24B");QCoreApplication::setApplicationName("ProductExportReplay");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        qualpf::application::ApplicationInitializationService{}.initialize(AppState::presentation(),qualpf::application::PresentationMode::automatic_gui);
        MainWindow::setPathDataFiles(root+"/share/qualx/files");MainWindow window(AppState::context());
        QJsonArray cases;
        auto capture=[&](const QString& name,const QString& format,bool back=false,bool direct=false) {
            const auto before=AppState::searchSession().snapshot();const auto snapshot=canonical();
            native={};nativePath.clear();dataCalls=xyCalls=cifCalls=0;
            const auto path=out+"/"+name+"."+format;
            bool result=false,visited=false,pathMatched=false;
            if(direct)result=format=="cif" ? bool(qualpf::gui::exporting::PatternExportService(AppState::searchSession().snapshot(), AppState::context().files().exports).save(path, qualpf::exporting::PatternFormat::powder_cif)) : bool(qualpf::gui::exporting::PatternExportService(AppState::searchSession().snapshot(), AppState::context().files().exports).save(path, back ? qualpf::exporting::PatternFormat::xy_background : qualpf::exporting::PatternFormat::xy_observed));
            else {
                QTimer driver;
                QObject::connect(&driver,&QTimer::timeout,[&] {
                    if(auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                        visited=true;driver.stop();dialog->setOption(QFileDialog::DontConfirmOverwrite);
                        // Same bounded file-model settling driver as Phase19D.
                        dialog->setDirectory(QFileInfo(path).absolutePath());
                        QTimer::singleShot(200, dialog, [dialog, format, path, &pathMatched] {
                            dialog->selectNameFilter(format=="cif" ? "CIF files (*.cif)" : "XY files (*.xy)");
                            dialog->selectFile(QFileInfo(path).fileName());
                            if (auto* edit=dialog->findChild<QLineEdit*>("fileNameEdit"))
                                edit->setText(QFileInfo(path).fileName());
                            QTimer::singleShot(50, dialog, [dialog, path, &pathMatched] {
                                pathMatched=!dialog->selectedFiles().isEmpty() &&
                                    QFileInfo(dialog->selectedFiles().first()).absoluteFilePath()==QFileInfo(path).absoluteFilePath();
                                if(pathMatched) QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);
                                else dialog->reject();
                            });
                        });
                    }
                });driver.start(10);
                require(QMetaObject::invokeMethod(&window,back ? "onActionBackgroundExportTriggered" : "onActionExportDiffractionPattern",Qt::DirectConnection),"Real product slot missing");
                require(visited,"Real SaveDialog not exercised");require(pathMatched,"Settled SaveDialog output path differs");result=QFileInfo::exists(path);
            }
            require(AppState::searchSession().snapshot()==before,"Export changed canonical Session");
            QJsonObject record{{"name",name},{"format",format},{"background_only",back},{"direct_application",direct},
              {"success",result},{"canonical",snapshot},{"native",native},{"native_filename",nativePath},
              {"output_filename",path},{"locale",QString::fromLatin1(std::setlocale(LC_NUMERIC,nullptr))},
              {"qt_locale",QLocale().name()},{"working_directory",QDir::currentPath()},
              {"set_diffraction_data",dataCalls},{"export_preprocessing_xy",xyCalls},{"esportanew",cifCalls}};
            write(out+"/"+name+".json",QJsonDocument(record).toJson());cases.append(QJsonObject{{"name",name},{"success",result}});
        };
        auto both=[&](const QString& name) { capture(name+"-xy","xy");capture(name+"-cif","cif"); };
        const auto examples=root+"/share/qualx/examples/";
        for(const auto& sample:QStringList{"example1","CPD2","MIXT_5"}) {
            require(window.loadDiffractionPatterns({examples+sample+".dat"},false),"Real example pattern open failed");both(sample);
        }
        require(window.loadDiffractionPatterns({examples+"example1.dat"},false),"Example reload failed");
        using namespace qualpf;using namespace qualpf::application; using namespace qualpf::gui;
        auto& session=AppState::searchSession();
        PreprocessingRequest request;request.wavelength=session.snapshot()->pattern.radiation.lines.front().wavelength;
        request.background=preprocessing::BackgroundOptions{};
        { PreprocessingEdit edit(session);require(bool(edit.preview(request)) && bool(edit.commit()),"Real background commit failed"); }
        both("background");capture("background-only","xy",true);
        capture("background-cif-rejected","cif",true);
        request=PreprocessingRequest();request.wavelength=session.snapshot()->pattern.radiation.lines.front().wavelength;
        request.smoothing=preprocessing::SmoothingOptions{};
        { PreprocessingEdit edit(session);require(bool(edit.preview(request)) && bool(edit.commit()),"Real smoothing commit failed"); }
        both("smoothed");
        request=PreprocessingRequest();request.wavelength=session.snapshot()->pattern.radiation.lines.front().wavelength;
        request.tolerance=AngularWidthDegrees{.15};request.manual=ManualPeakChange{};
        request.manual->two_theta=session.snapshot()->pattern.two_theta[200].value;request.manual->intensity=37.25;request.manual->fwhm=.12;
        { PreprocessingEdit edit(session);require(bool(edit.preview(request)) && bool(edit.commit()),"Real manual peak commit failed"); }
        both("peaks");
        for(int axis=0;axis<4;++axis) {
            auto options=window.xpdViewer()->pSettings();options.setAbscissa(static_cast<xpdutils::xAbscissaType>(axis));window.xpdViewer()->setPlotSettings(options);
            both("view-axis-"+QString::number(axis));
        }
        ProductWorkflowProbe::save(window,out+"/reopen.qxp");ProductWorkflowProbe::load(window,out+"/reopen.qxp");both("project-reopen");
        auto source=session.snapshot();auto next=*source;next.pattern.calculation_range=AngularRange{next.pattern.two_theta[100],next.pattern.two_theta[300]};
        require(bool(session.commit(source,next)),"Working range fixture failed");both("working-range");
        source=session.snapshot();next=*source;next.pattern.background->values=std::vector<double>(next.pattern.background->values.begin()+100,next.pattern.background->values.begin()+300);next.pattern.background->window=SampleWindow{100};
        require(bool(session.commit(source,next)),"Partial background fixture failed");capture("partial-background","xy",true);capture("partial-cif-rejected","cif");
        require(window.loadDiffractionPatterns({examples+"example1.dat"},false),"Crop reload failed");
        request=PreprocessingRequest();request.wavelength=session.snapshot()->pattern.radiation.lines.front().wavelength;
        request.crop_range=AngularRange{session.snapshot()->pattern.two_theta[100],session.snapshot()->pattern.two_theta[300]};
        { PreprocessingEdit edit(session);require(bool(edit.preview(request)) && bool(edit.commit()),"Real C++ crop commit failed"); }
        both("cropped");
        write(out+"/实验 short input.xy","10 -1\n11 0\n12 1\n13 0.000000001\n14 1000000000\n");
        require(window.loadDiffractionPatterns({out+"/实验 short input.xy"},false),"Real signed/zero input failed");both("number-edges");
        write(out+"/two.xy","10 1\n20 2\n");require(window.loadDiffractionPatterns({out+"/two.xy"},false),"Two point product input failed");both("two-point");
        capture(QString(90,'f')+" spaces 实验","xy");
        source=session.snapshot();next=*source;next.pattern.two_theta={TwoThetaDegrees{10.0625},TwoThetaDegrees{20.1875}};
        next.pattern.observed={-0.0,1.0000001192092896};next.pattern.original_observed=next.pattern.observed;
        next.pattern.calculation_range=AngularRange{next.pattern.two_theta.front(),next.pattern.two_theta.back()};
        next.pattern.corrected_two_theta=next.pattern.two_theta;
        require(bool(session.commit(source,next)),"Exact precision boundary fixture failed");both("precision-boundaries");
        write(out+"/one.xy","10 1\n");require(!window.loadDiffractionPatterns({out+"/one.xy"},false),"One point product unexpectedly accepted");
        for(const auto& preset:QStringList{"custom","neutron","electron","missing-metadata","two-waves"}) {
            source=session.snapshot();next=*source;
            next.pattern.radiation.lines={{WavelengthAngstrom{preset=="custom" ? .123456789012345 : radiation::default_wavelength_angstrom},1.0}};
            next.projectMetadata["radtype"]=preset=="neutron" ? 1 : preset=="electron" ? 2 : 0;
            if(preset=="missing-metadata")next.projectMetadata={};
            if(preset=="two-waves")next.pattern.radiation.lines.push_back({WavelengthAngstrom{radiation::CU_KA2_WAVE},.5});
            require(bool(session.commit(source,next)),"Radiation fixture failed");both(preset);
        }
        capture("application-direct","cif",false,true);
        session.beginLegacyEdit("phase24-preview");capture("editing-cif-rejected","cif",false,true);session.endLegacyEdit("phase24-preview",false);
        session.reset();capture("empty-cif-rejected","cif",false,true);capture("empty-xy-rejected","xy",false,true);
        write(out+"/index.json",QJsonDocument(QJsonObject{{"status","PASS"},{"cases",cases},{"formatter_implementation_present",true}}).toJson());
        std::cout<<"PASS real export oracle cases "<<cases.size()<<'\n';return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
