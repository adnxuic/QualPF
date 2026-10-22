#include "gui/dialogs/backgrounddialog.h"
#include "gui/dialogs/smoothingdialog.h"
#include "gui/dialogs/peaksearchdialog.h"
#include "gui/dialogs/rangedialog.h"
#include "gui/dialogs/searchoptionsdialog.h"
#include "gui/dialogs/pfsm_options_dialog.h"
#include "gui/dialogs/fwhm_options_dialog.h"
#include "gui/dialogs/initial_peak_options_dialog.h"
#include "gui/dialogs/workflow_settings_dialog.h"
#include "gui/widgets/workflow_dock.h"
#include "gui/mainwindow.h"
#include "infrastructure/database/database_build_provider.h"
#include "infrastructure/network/update_service.h"
#include "infrastructure/platform/platform_services.h"
#include "infrastructure/settings/preference_store.h"
#include "infrastructure/settings/default_settings.h"
#include "infrastructure/settings/search_settings.h"
#include "infrastructure/io/product_files.h"
#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "infrastructure/settings/database_catalog_store.h"
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <cmath>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QKeyEvent>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QTreeWidget>
#include <iostream>
#include <source_location>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::gui;
static void check(bool v,std::source_location at=std::source_location::current()) {
    if(!v)throw std::runtime_error("Workflow GUI line "+std::to_string(at.line()));
}
template<class T> T *widget(QObject &root,const char *name) {
    auto *w=root.findChild<T *>(name);if(!w)throw std::runtime_error(std::string("Missing widget ")+name);return w;
}
static void events() {QEventLoop loop;QTimer::singleShot(0,&loop,&QEventLoop::quit);loop.exec();}
static void waitFor(const std::function<bool()> &ready) {
    QEventLoop loop;QTimer poll,deadline;bool timeout=false;poll.setInterval(10);deadline.setSingleShot(true);
    QObject::connect(&poll,&QTimer::timeout,&loop,[&]{if(ready())loop.quit();});
    QObject::connect(&deadline,&QTimer::timeout,&loop,[&]{timeout=true;loop.quit();});poll.start();deadline.start(10000);loop.exec();check(!timeout&&ready());
}
struct Defaults final:DefaultSettingsStore {
    ProjectSettings value;bool fail=false;
    Result<ProjectSettings> load() const override{return Result<ProjectSettings>{value};}
    Status save(const ProjectSettings &v) override {
        if(fail)return Status{Error{ErrorCode::missing_data,"settings","Injected write failure","io_error"}};
        value=v;return Status{std::monostate{}};
    }
};
struct TimedDefaults final:DefaultSettingsStore {
    DefaultSettingsStore &store;mutable qint64 loadNanoseconds=0;
    explicit TimedDefaults(DefaultSettingsStore &store):store(store) {}
    Result<ProjectSettings> load() const override {
        QElapsedTimer timer;timer.start();auto result=store.load();loadNanoseconds=timer.nsecsElapsed();return result;
    }
    Status save(const ProjectSettings &value) override{return store.save(value);}
};
struct FirstPaint final:QObject {
    QElapsedTimer &timer;qint64 nanoseconds=-1;
    explicit FirstPaint(QElapsedTimer &timer):timer(timer) {}
    bool eventFilter(QObject *,QEvent *event) override {
        if(event->type()==QEvent::Paint && nanoseconds<0)nanoseconds=timer.nsecsElapsed();
        return false;
    }
};
static void measureOpening(WorkflowId flow) {
    infrastructure::QtDefaultSettingsStore persisted;check(bool(persisted.migrate()));
    TimedDefaults defaults(persisted);Workspace workspace(SearchBackend::cpp,defaultAlgorithms(),&defaults);
    infrastructure::QtPreferenceStore preferences;infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider buildProvider;DatabaseBuildUseCases build{buildProvider};
    infrastructure::QtUpdateServiceProvider updates;infrastructure::QtPlatformServices platform;infrastructure::ProductFileProviders files;
    auto factory=std::make_shared<infrastructure::SqlCandidateReaderFactory>();infrastructure::DatabaseCatalogSettings catalogSettings;
    infrastructure::SqlReferenceRepository repository;DatabaseCatalog catalog(workspace.searches(),catalogSettings,repository);
    WorkspaceContext context(workspace,catalog,files.ports,preferences,searchSettings,platform,updates,build,factory);
    context.presentation().configure(PresentationMode::automatic_gui);
    MainWindow window(context);const bool hiddenNative=QGuiApplication::platformName()=="windows";
    if(hiddenNative)window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();if(hiddenNative)check(!window.grab().isNull());events();
    for(int run=0;run<3;++run) {
        QElapsedTimer timer;timer.start();WorkflowSettingsDialog dialog(workspace,&window);
        const auto constructed=timer.nsecsElapsed();FirstPaint painted(timer);dialog.installEventFilter(&painted);
        // Exercise the native Windows style without displaying benchmark windows.
        if(hiddenNative)dialog.setAttribute(Qt::WA_DontShowOnScreen);
        check(dialog.load(flow));const auto loaded=timer.nsecsElapsed();dialog.show();const auto shown=timer.nsecsElapsed();
        if(hiddenNative)check(!dialog.grab().isNull());
        waitFor([&]{return painted.nanoseconds>=0;});
        std::cout<<"settings_open flow="<<int(flow)<<" run="<<run
            <<" construct_us="<<constructed/1000<<" defaults_read_us="<<defaults.loadNanoseconds/1000
            <<" load_us="<<(loaded-constructed)/1000<<" show_us="<<(shown-loaded)/1000
            <<" first_paint_us="<<painted.nanoseconds/1000<<" widgets="<<dialog.findChildren<QWidget *>().size()<<'\n';
        dialog.reject();events();
    }
}
int main(int argc,char **argv) try {
    QApplication app(argc,argv);QTemporaryDir temp;check(temp.isValid());
    if(qEnvironmentVariable("QT_QPA_PLATFORM")=="offscreen") {
        const int font=QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        if(font>=0)app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).front(),9));
    }
    QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temp.path());
    app.setOrganizationName("QualPF-tests");app.setApplicationName("workflow-settings-ui");
    if(app.arguments().contains("--measure-settings")) {
        const int flow=argc>2?QString::fromLocal8Bit(argv[2]).toInt():0;check(flow>=0&&flow<4);
        measureOpening(WorkflowId(flow));return 0;
    }
    const QString capture=argc==2?QString::fromLocal8Bit(argv[1]):QString();if(!capture.isEmpty())check(QDir().mkpath(capture));
    Defaults defaults;Workspace workspace(SearchBackend::cpp,defaultAlgorithms(),&defaults);
    const auto original=workspace.session().snapshot();
    WorkflowSettingsDialog dialog(workspace);
    check(dialog.load(WorkflowId::csm,"Background"));dialog.show();events();
    // Opening one workflow must not construct the other workflows' editors.
    check(!dialog.findChild<QSpinBox *>("0_backgroundCoefficients")&&!dialog.findChild<QLineEdit *>("2_pfsmLower")&&!dialog.findChild<QLineEdit *>("3_peakSlope"));
    check(dialog.selectedWorkflow()==WorkflowId::csm&&dialog.selectedModule()=="Background");
    check(dialog.width()>=650&&!dialog.findChild<QPushButton *>("saveRunWorkflow"));
    auto *categories=widget<QListWidget>(dialog,"workflowCategories");
    categories->setFocus();QKeyEvent down(QEvent::KeyPress,Qt::Key_Down,Qt::NoModifier);QApplication::sendEvent(categories,&down);
    check(dialog.selectedWorkflow()==WorkflowId::pfsm);categories->setCurrentRow(1);
    for(int flow=0;flow<4;++flow) {
        categories->setCurrentRow(flow);auto *tabs=dialog.findChild<QTabWidget *>(QString::number(flow)+"_workflowModules");check(tabs);
        for(int page=0;page<tabs->count();++page) {
            tabs->setCurrentIndex(page);events();auto *button=widget<QPushButton>(dialog,"saveWorkflowSettings");
            check(dialog.rect().contains(button->mapTo(&dialog,button->rect().bottomRight())));
            check(categories->width()>=140&&tabs->currentWidget()->width()>300);
        }
        tabs->setCurrentIndex(0);
    }
    categories->setCurrentRow(2);auto *pfsmModules=widget<QTabWidget>(dialog,"2_workflowModules");
    check(pfsmModules->tabText(0)=="Background" && pfsmModules->tabText(1)=="Initial Peaks" && pfsmModules->tabText(2)=="FWHM");
    pfsmModules->setCurrentIndex(4);check(pfsmModules->tabText(4)=="Profile Expansion");
    auto *pfsmCache=widget<QCheckBox>(dialog,"2_pfsmCache");check(pfsmCache->isChecked());
    pfsmCache->setChecked(false);widget<QPushButton>(dialog,"restoreWorkflowPage")->click();check(pfsmCache->isChecked());
    if(!capture.isEmpty()){events();check(dialog.grab().save(capture+"/setting-pfsm-template-cache.png"));}
    pfsmCache->setChecked(false);
    pfsmModules->setCurrentIndex(1);auto *pfsmInitial=widget<QLineEdit>(dialog,"2_initialSlope");
    auto *pfsmAutomatic=widget<QCheckBox>(dialog,"2_initialAutomatic");auto *pfsmWindow=widget<QSpinBox>(dialog,"2_initialWindow");
    pfsmInitial->setText("0.33");pfsmAutomatic->setChecked(false);check(pfsmWindow->isEnabled());pfsmWindow->setValue(17);
    widget<QSpinBox>(dialog,"2_initialDegree")->setValue(5);
    categories->setCurrentRow(1);check(widget<QLineEdit>(dialog,"1_initialSlope")->text()=="0.1");
    categories->setCurrentRow(2);check(pfsmInitial->text()=="0.33" && pfsmWindow->value()==17);
    widget<QPushButton>(dialog,"restoreWorkflowPage")->click();
    check(pfsmInitial->text()=="0.1" && pfsmAutomatic->isChecked() && !pfsmWindow->isEnabled() && pfsmWindow->value()==11);
    check(widget<QSpinBox>(dialog,"2_initialDegree")->value()==9);
    pfsmInitial->setText("0.33");pfsmAutomatic->setChecked(false);pfsmWindow->setValue(17);widget<QSpinBox>(dialog,"2_initialDegree")->setValue(5);
    if(!capture.isEmpty())check(dialog.grab().save(capture+"/setting-pfsm-initial-peaks.png"));
    categories->setCurrentRow(1);
    auto *automatic=widget<QCheckBox>(dialog,"1_backgroundAutomatic");automatic->setChecked(false);
    auto *coefficient=widget<QSpinBox>(dialog,"1_backgroundCoefficients");check(coefficient->isEnabled());coefficient->setValue(12);
    categories->setCurrentRow(0);widget<QCheckBox>(dialog,"0_backgroundAutomatic")->setChecked(false);widget<QSpinBox>(dialog,"0_backgroundCoefficients")->setValue(8);
    categories->setCurrentRow(1);check(coefficient->value()==12);
    widget<QPushButton>(dialog,"restoreWorkflowPage")->click();check(coefficient->value()==6);
    categories->setCurrentRow(0);check(widget<QSpinBox>(dialog,"0_backgroundCoefficients")->value()==8);
    categories->setCurrentRow(1);automatic->setChecked(false);coefficient->setValue(12);
    if(!capture.isEmpty())check(dialog.grab().save(capture+"/setting-csm-background.png"));
    auto *save=widget<QPushButton>(dialog,"saveWorkflowSettings");
    defaults.fail=true;save->click();check(dialog.isVisible()&&workspace.session().snapshot()==original&&defaults.value.profile(WorkflowId::csm).background.coefficients==6);
    defaults.fail=false;save->click();check(!dialog.isVisible()&&defaults.value.profile(WorkflowId::csm).background.coefficients==12);
    check(defaults.value.profile(WorkflowId::preprocessing).background.coefficients==8&&workspace.session().snapshot()==original);
    check(!defaults.value.profile(WorkflowId::pfsm).pfsm.template_cache && original->projectSettings->profile(WorkflowId::pfsm).pfsm.template_cache);
    const auto &pfsmInitialSaved=defaults.value.profile(WorkflowId::pfsm).initial_peaks.options;
    check(pfsmInitialSaved.slope==.33f && !pfsmInitialSaved.automatic_window && pfsmInitialSaved.half_window==17 && pfsmInitialSaved.polynomial_degree==5);
    for(auto flow:{WorkflowId::preprocessing,WorkflowId::csm,WorkflowId::peak_search})check(defaults.value.profile(flow).initial_peaks.options.slope==.1f);
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::pfsm).initial_peaks.options.slope==.1f);
    Workspace fresh(SearchBackend::cpp,defaultAlgorithms(),&defaults);check(fresh.session().snapshot()->projectSettings->profile(WorkflowId::csm).background.coefficients==12);
    check(fresh.session().snapshot()->projectSettings->profile(WorkflowId::pfsm).initial_peaks.options.slope==.33f);
    check(!fresh.session().snapshot()->projectSettings->profile(WorkflowId::pfsm).pfsm.template_cache);
    WorkflowSettingsDialog restored(fresh);check(restored.load(WorkflowId::pfsm,"Initial Peaks"));
    check(restored.selectedModule()=="Initial Peaks" && widget<QLineEdit>(restored,"2_initialSlope")->text()=="0.33");
    check(widget<QSpinBox>(restored,"2_initialWindow")->value()==17 && widget<QSpinBox>(restored,"2_initialDegree")->value()==5);
    {
        Defaults partial;partial.value=defaults.value;partial.value.profile(WorkflowId::pfsm).pfsm.threads=3;
        partial.value.profile(WorkflowId::peak_search).peaks.slope=.2f;
        Workspace unopened(SearchBackend::cpp,defaultAlgorithms(),&partial);const auto project=unopened.session().snapshot();
        WorkflowSettingsDialog lazy(unopened);check(lazy.load(WorkflowId::csm));lazy.show();events();
        widget<QCheckBox>(lazy,"1_backgroundAutomatic")->setChecked(false);widget<QSpinBox>(lazy,"1_backgroundCoefficients")->setValue(13);
        widget<QPushButton>(lazy,"saveWorkflowSettings")->click();check(!lazy.isVisible());
        check(!lazy.findChild<QSpinBox *>("2_pfsmWorkers") && !lazy.findChild<QLineEdit *>("3_peakSlope"));
        check(partial.value.profile(WorkflowId::pfsm).pfsm.threads==3 && partial.value.profile(WorkflowId::peak_search).peaks.slope==.2f);
        check(unopened.session().snapshot()==project);
        check(lazy.load(WorkflowId::csm));lazy.show();events();
        auto *draft=widget<QLineEdit>(lazy,"1_initialSlope");draft->setText("invalid");
        auto *flows=widget<QListWidget>(lazy,"workflowCategories");flows->setCurrentRow(2);events();
        check(widget<QSpinBox>(lazy,"2_pfsmWorkers")->value()==3);flows->setCurrentRow(1);events();
        check(draft->text()=="invalid");widget<QPushButton>(lazy,"saveWorkflowSettings")->click();
        check(lazy.isVisible() && lazy.selectedWorkflow()==WorkflowId::csm && lazy.selectedModule()=="Initial Peaks");
        check(draft->text()=="invalid" && unopened.session().snapshot()==project);
        lazy.reject();check(partial.value.profile(WorkflowId::csm).initial_peaks.options.slope==.1f);
    }
    // Actual main-window menu routing and dock integration.
    infrastructure::QtPreferenceStore preferences;infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider buildProvider;DatabaseBuildUseCases build{buildProvider};
    infrastructure::QtUpdateServiceProvider updates;infrastructure::QtPlatformServices platform;infrastructure::ProductFileProviders files;
    auto factory=std::make_shared<infrastructure::SqlCandidateReaderFactory>();infrastructure::DatabaseCatalogSettings catalogSettings;
    infrastructure::SqlReferenceRepository repository;DatabaseCatalog catalog(workspace.searches(),catalogSettings,repository);
    WorkspaceContext context(workspace,catalog,files.ports,preferences,searchSettings,platform,updates,build,factory);
    context.presentation().configure(PresentationMode::automatic_gui);
    MainWindow window(context);window.show();events();
    auto *menu=widget<QMenu>(window,"menuSetting");const auto actions=window.menuBar()->actions();
    check(actions.indexOf(menu->menuAction())+1==actions.indexOf(widget<QMenu>(window,"menuHelp")->menuAction()));
    check(menu->actions().size()==4);
    for(int i=0;i<4;++i) {
        menu->actions()[i]->trigger();events();auto *opened=widget<WorkflowSettingsDialog>(window,"workflowDefaultsDialog");
        check(int(opened->selectedWorkflow())==i);opened->reject();events();
    }
    Pattern pattern;pattern.id={1};pattern.source_id="workflow dock";pattern.two_theta={TwoThetaDegrees{20},TwoThetaDegrees{21},TwoThetaDegrees{22}};
    pattern.two_theta.clear();
    for(int i=0;i<=400;++i){const double x=20+.01*i;pattern.two_theta.emplace_back(x);pattern.observed.push_back(10+100*std::exp(-std::pow((x-22.03)/.12,2)));}
    pattern.original_observed=pattern.observed;pattern.processing={false,false,false};pattern.radiation.lines={{WavelengthAngstrom{1.54},1}};
    check(bool(workspace.session().openPattern(workspace.session().snapshot(),pattern)));
    auto source=workspace.session().snapshot();auto next=*source;next.pattern.background=SampledSignal{std::vector<double>(pattern.observed.size(),1),SampleWindow{0}};
    check(bool(workspace.session().commit(source,next)));source=workspace.session().snapshot();next=*source;next.workflow.completed_target=WorkflowStep::search;
    check(bool(workspace.session().commit(source,next)));
    auto *dock=widget<WorkflowDock>(window,"workflowDock");dock->show();events();
    auto *steps=widget<QTreeWidget>(*dock,"workflowSteps");check(steps->topLevelItemCount()==2);steps->setCurrentItem(steps->topLevelItem(0));
    const auto before=workspace.session().snapshot();
    QTimer::singleShot(0,[]{if(auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))box->reject();});
    widget<QPushButton>(*dock,"workflowUndo")->click();check(workspace.session().snapshot()==before);
    if(!capture.isEmpty())check(dock->grab().save(capture+"/workflow-dock.png"));
    QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());check(box);
        for(auto *button:box->buttons())if(box->buttonRole(button)==QMessageBox::AcceptRole){button->click();break;}});
    widget<QPushButton>(*dock,"workflowUndo")->click();check(!workspace.session().snapshot()->pattern.background&&workspace.session().snapshot()->workflow.records.empty());
    // Every project Options entry opens its established dialog, with no settings sidebar.
    window.enableActions(MainWindow::PatternAction);
    int csmOpened=0;
    QTimer::singleShot(0,[&]{
        auto *native=window.findChild<SearchOptionsDialog *>();check(native&&native->isVisible());
        check(!native->findChild<QListWidget *>("workflowCategories"));
        widget<QDoubleSpinBox>(*native,"spinFom")->setValue(.42);
        if(!capture.isEmpty())check(native->grab().save(capture+"/options-csm-native.png"));
        widget<QDialogButtonBox>(*native,"buttonBox")->button(QDialogButtonBox::Ok)->click();++csmOpened;
    });
    widget<QAction>(window,"actionSearch_Match_Options")->trigger();check(csmOpened==1);
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::csm).csm.minimum_fom==.42);
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::csm).csm.delta==.08);
    check(defaults.value.profile(WorkflowId::csm).csm.minimum_fom==.35);
    const auto csmSaved=workspace.session().snapshot();
    QTimer::singleShot(0,[&]{auto *native=window.findChild<SearchOptionsDialog *>();check(native);widget<QDoubleSpinBox>(*native,"spinFom")->setValue(.7);native->reject();});
    widget<QAction>(window,"actionSearch_Match_Options")->trigger();check(workspace.session().snapshot()==csmSaved);
    widget<QAction>(window,"actionPFSMOptions")->trigger();events();
    auto *pfsm=widget<PfsmOptionsDialog>(window,"pfsmOptionsDialog");check(pfsm->isVisible());
    check(widget<QTabWidget>(*pfsm,"pfsmOptionsTabs")->count()==2&&!pfsm->findChild<QListWidget *>());
    auto *lower=widget<QLineEdit>(*pfsm,"lowerShift"),*upper=widget<QLineEdit>(*pfsm,"upperShift");
    lower->setText("1");upper->setText("-1");pfsm->accept();check(pfsm->isVisible());
    lower->setText("-0.25");upper->setText("0.25");
    if(!capture.isEmpty())check(pfsm->grab().save(capture+"/options-pfsm-native.png"));
    pfsm->accept();check(!pfsm->isVisible());
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::pfsm).pfsm.lower_shift==-.25);
    check(defaults.value.profile(WorkflowId::pfsm).pfsm.lower_shift==-.5);
    source=workspace.session().snapshot();next=*source;
    next.projectSettings->profile(WorkflowId::preprocessing).background.method=preprocessing::BackgroundMethod::none;
    next.projectSettings->profile(WorkflowId::peak_search).background.method=preprocessing::BackgroundMethod::none;
    next.projectSettings->profile(WorkflowId::peak_search).fwhm.use_fixed=true;
    next.projectSettings->profile(WorkflowId::peak_search).fwhm.default_fwhm=AngularWidthDegrees{.2};
    check(bool(workspace.session().commit(source,next)));
    const auto nativeRoute=[&](const char *action, QDialog *native) {
        window.enableActions(MainWindow::PatternAction);widget<QAction>(window,action)->trigger();events();
        check(native&&native->isVisible()&&!native->findChild<QListWidget *>("workflowCategories"));
        check(!window.findChild<WorkflowSettingsDialog *>());
        if(!capture.isEmpty())check(native->grab().save(capture+"/"+QString::fromLatin1(action)+"-native.png"));
        check(QMetaObject::invokeMethod(native,"reject"));events();check(!native->isVisible());
    };
    nativeRoute("actionBackgroundOptions",window.findChild<BackgroundDialog *>());
    nativeRoute("actionSmoothing",window.findChild<SmoothingDialog *>());
    nativeRoute("actionPeak_Search_Conditions",window.findChild<PeakSearchDialog *>());
    window.enableActions(MainWindow::PatternAction);widget<QAction>(window,"actionRange")->trigger();events();
    auto *range=window.findChild<RangeDialog *>();check(range&&range->isVisible());static_cast<QDialog *>(range)->reject();events();
    nativeRoute("actionInitialPeakSearchOptions",widget<InitialPeakOptionsDialog>(window,"initialPeakOptionsDialog"));
    window.enableActions(MainWindow::PatternAction);widget<QAction>(window,"actionFwhmEstimationOptions")->trigger();events();
    auto *runDialog=widget<FwhmOptionsDialog>(window,"fwhmOptionsDialog");check(runDialog->isVisible());
    widget<QCheckBox>(*runDialog,"useFixedFwhmCheckBox")->setChecked(true);
    widget<QLineEdit>(*runDialog,"defaultFwhmEdit")->setText("0.3");
    runDialog->accept();
    waitFor([&]{return workspace.session().snapshot()->preprocessing.fwhm_usage.has_value()&&!window.searchTasks()->busy();});
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::preprocessing).fwhm.default_fwhm.value==.3);
    check(workspace.session().snapshot()->preprocessing.fwhm_usage->origin==FwhmOrigin::fixed);
    check(workspace.session().snapshot()->workflow.last_attempt->status==WorkflowStepStatus::completed);
    check(defaults.value.profile(WorkflowId::preprocessing).fwhm.default_fwhm.value==.1);
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::peak_search).fwhm.default_fwhm.value==.2);
    // Initial-peak execution keeps the original Candidates tab in this window.
    window.enableActions(MainWindow::PatternAction);widget<QAction>(window,"actionInitialPeakSearchOptions")->trigger();events();
    auto *initial=widget<InitialPeakOptionsDialog>(window,"initialPeakOptionsDialog");
    auto *slope=widget<QLineEdit>(*initial,"initialPeakSlopeEdit");slope->setText("0.2");
    check(QMetaObject::invokeMethod(slope,"textEdited",Q_ARG(QString,QString("0.2"))));
    initial->accept();waitFor([&]{return !window.searchTasks()->busy();});
    check(initial->isVisible() && widget<QTabWidget>(*initial,"initialPeakOptionsTabs")->currentIndex()==1);
    check(!window.findChild<QDialog *>("initialPeakResults"));
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::preprocessing).initial_peaks.options.slope==.2f);
    check(bool(workspace.session().snapshot()->preprocessing.initial_peak_usage));
    initial->reject();events();
    // Canceling the dependent-result warning leaves the native draft and the
    // editing guard intact; accepting publishes parameters and output together.
    window.enableActions(MainWindow::PatternAction);widget<QAction>(window,"actionBackgroundOptions")->trigger();events();
    auto *background=window.findChild<BackgroundDialog *>();check(background && background->isVisible());
    widget<QComboBox>(*background,"typeComboBox")->setCurrentIndex(1);events();
    const auto editing=workspace.session().snapshot();
    auto *ok=widget<QDialogButtonBox>(*background,"buttonBox")->button(QDialogButtonBox::Ok);
    QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());check(box);box->reject();});
    ok->setFocus();ok->click();events();
    check(background->isVisible() && workspace.session().snapshot()==editing && editing->editing);
    check(!widget<QAction>(window,"actionRange")->isEnabled());
    // Offscreen has no window manager to reactivate the editor after the modal
    // warning. Simulate that part of the user's next explicit mouse click.
    background->activateWindow();events();ok->setFocus();events();check(ok->hasFocus());
    QTimer::singleShot(0,[]{auto *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());check(box);
        for(auto *button:box->buttons())if(box->buttonRole(button)==QMessageBox::AcceptRole){button->click();break;}});
    ok->setFocus();ok->click();events();
    check(!background->isVisible() && !workspace.session().snapshot()->editing);
    check(workspace.session().snapshot()->projectSettings->profile(WorkflowId::preprocessing).background.method==preprocessing::BackgroundMethod::polynomial);
    check(defaults.value.profile(WorkflowId::preprocessing).background.method==preprocessing::BackgroundMethod::chebyshev);
    std::cout<<"Defaults forms, original project dialogs, independent scope and cascade confirmation passed\n";
} catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
