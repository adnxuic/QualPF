#include "gui/dialogs/workflow_settings_dialog.h"
#include "qualpf/application/search_coordinator.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>
namespace qualpf::gui {
using namespace application;
namespace {
std::size_t index(WorkflowId id) {return static_cast<std::size_t>(id);}
QString prefix(WorkflowId id) {return QString::number(int(id))+"_";}
QString numberText(double value,bool singlePrecision) {
    if(value==0||(std::abs(value)>=.001&&std::abs(value)<100000)) for(int decimals=0;decimals<=16;++decimals) {
        const auto text=QString::number(value,'f',decimals);const auto parsed=QLocale::c().toDouble(text);
        if(singlePrecision?float(parsed)==float(value):parsed==value)return text;
    }
    for(int precision=1;precision<=17;++precision) {
        const auto text=QString::number(value,'g',precision);const auto parsed=QLocale::c().toDouble(text);
        if(singlePrecision?float(parsed)==float(value):parsed==value)return text;
    }
    return QString::number(value,'g',17);
}
QStringList modules(WorkflowId id) {
    switch(id) {
    // U+2082 can trigger a costly cold fallback in Windows UI fonts. Keep the
    // system font and use the equivalent ordinary digit in this label.
    case WorkflowId::preprocessing: return {"Range","Background","Initial Peaks","FWHM","Smoothing","Kα2"};
    case WorkflowId::csm: return {"Background","Initial Peaks","FWHM","Peak Extraction","Candidate Screening","Matching","FOM","Residual","Results"};
    case WorkflowId::pfsm: return {"Background","Initial Peaks","FWHM","Candidate Recall","Profile Expansion","Profile Fitting","FOM","Joint Baseline","Results"};
    case WorkflowId::peak_search: return {"Background","Initial Peaks","FWHM","Peak Extraction","Peak Selection"};
    }
    return {};
}
}
WorkflowSettingsDialog::WorkflowSettingsDialog(Workspace &workspace,QWidget *parent)
    :QDialog(parent),workspace_(workspace) {
    setObjectName("workflowDefaultsDialog");
    setWindowModality(Qt::WindowModal);resize(980,680);setMinimumSize(650,430);
    auto *layout=new QVBoxLayout(this);auto *body=new QHBoxLayout;
    flows_=new QListWidget(this);flows_->setObjectName("workflowCategories");flows_->setFixedWidth(150);
    stack_=new QStackedWidget(this);body->addWidget(flows_);body->addWidget(stack_,1);layout->addLayout(body,1);
    for(auto id:workflows) {
        flows_->addItem(QString::fromUtf8(workflowName(id)));
        auto *tabs=new QTabWidget(stack_);tabs->setObjectName(prefix(id)+"workflowModules");
        tabs->setUsesScrollButtons(true);tabs_[index(id)]=tabs;stack_->addWidget(tabs);
    }
    status_=new QLabel(this);status_->setWordWrap(true);status_->setObjectName("workflowSettingsStatus");layout->addWidget(status_);
    auto *buttons=new QDialogButtonBox(this);buttons->setObjectName("workflowSettingsButtons");
    auto *restore=buttons->addButton(tr("Restore Page Defaults"),QDialogButtonBox::ResetRole);
    restore->setObjectName("restoreWorkflowPage");connect(restore,&QPushButton::clicked,this,&WorkflowSettingsDialog::restorePage);
    buttons->addButton(QDialogButtonBox::Cancel);
    auto *saveButton=buttons->addButton(tr("Save Defaults"),QDialogButtonBox::ActionRole);
    saveButton->setObjectName("saveWorkflowSettings");saveButton->setDefault(true);connect(saveButton,&QPushButton::clicked,this,[this]{save();});
    connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject);layout->addWidget(buttons);
    connect(flows_,&QListWidget::currentRowChanged,this,[this](int row){
        if(row<0)return;
        ensureFlow(WorkflowId(row));stack_->setCurrentIndex(row);
    });
}
WorkflowSettingsDialog::Field &WorkflowSettingsDialog::addField(WorkflowId id,int page,const QString &label,const QString &key,QWidget *control) {
    auto &p=pages_[index(id)][page];auto *container=new QWidget(p.widget);auto *layout=new QVBoxLayout(container);
    layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);layout->addWidget(control);
    auto *error=new QLabel(container);error->setWordWrap(true);error->setObjectName(prefix(id)+key+"Error");error->hide();layout->addWidget(error);
    control->setObjectName(prefix(id)+key);p.form->addRow(label,container);
    fields_.push_back({id,page,control,error,{},{},{}});return fields_.back();
}
QCheckBox *WorkflowSettingsDialog::boolean(WorkflowId id,int page,const QString &label,const QString &key,
    std::function<bool(const Profile &)> get,std::function<void(Profile &,bool)> set) {
    auto *w=new QCheckBox;auto &f=addField(id,page,label,key,w);
    f.read=[this,id,w,set]{set(draft_.profile(id),w->isChecked());};
    f.write=[this,id,w,get]{w->setChecked(get(draft_.profile(id)));};return w;
}
QSpinBox *WorkflowSettingsDialog::integer(WorkflowId id,int page,const QString &label,const QString &key,int low,int high,
    std::function<int(const Profile &)> get,std::function<void(Profile &,int)> set) {
    auto *w=new QSpinBox;w->setRange(low,high);w->setKeyboardTracking(false);auto &f=addField(id,page,label,key,w);
    f.read=[this,id,w,set]{w->interpretText();set(draft_.profile(id),w->value());};
    f.write=[this,id,w,get]{w->setValue(get(draft_.profile(id)));};
    f.check=[w]{return w->hasAcceptableInput()?QString():tr("Enter an integer within the allowed range.");};return w;
}
QLineEdit *WorkflowSettingsDialog::number(WorkflowId id,int page,const QString &label,const QString &key,double low,double high,
    std::function<double(const Profile &)> get,std::function<void(Profile &,double)> set) {
    auto *w=new QLineEdit;auto *validator=new QDoubleValidator(low,high,16,w);validator->setLocale(QLocale::c());w->setValidator(validator);
    auto &f=addField(id,page,label,key,w);
    f.read=[this,id,w,set]{set(draft_.profile(id),QLocale::c().toDouble(w->text()));};
    const bool singlePrecision=key=="initialSlope"||key=="peakSlope"||key=="selectionThreshold";
    f.write=[this,id,w,get,singlePrecision]{w->setText(numberText(get(draft_.profile(id)),singlePrecision));};
    f.check=[w,low,high]{bool ok=false;const auto value=QLocale::c().toDouble(w->text(),&ok);
        return ok&&std::isfinite(value)&&value>=low&&value<=high?QString():tr("Enter a finite number from %1 to %2.").arg(low).arg(high);};return w;
}
QComboBox *WorkflowSettingsDialog::choice(WorkflowId id,int page,const QString &label,const QString &key,QStringList labels,
    std::function<int(const Profile &)> get,std::function<void(Profile &,int)> set) {
    auto *w=new QComboBox;w->addItems(labels);auto &f=addField(id,page,label,key,w);
    f.read=[this,id,w,set]{set(draft_.profile(id),w->currentIndex());};
    f.write=[this,id,w,get]{w->setCurrentIndex(get(draft_.profile(id)));};return w;
}
void WorkflowSettingsDialog::note(WorkflowId id,int page,const QString &text) {
    auto &p=pages_[index(id)][page];auto *label=new QLabel(text,p.widget);label->setWordWrap(true);p.form->addRow(label);
}
void WorkflowSettingsDialog::ensureFlow(WorkflowId id) {
    if(!pages_[index(id)].empty())return;
    // Only initialize new editors; revisiting a workflow must preserve its draft,
    // including invalid text that has not been collected into the typed values.
    buildFlow(id);refresh(id);
}
void WorkflowSettingsDialog::buildFlow(WorkflowId id) {
    auto *tabs=tabs_[index(id)];
    for(const auto &name:modules(id)) {
        auto *scroll=new QScrollArea(tabs);scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
        auto *content=new QWidget(scroll);auto *layout=new QVBoxLayout(content);auto *group=new QGroupBox(name,content);
        auto *form=new QFormLayout(group);form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        layout->addWidget(group);auto *error=new QLabel(content);error->setWordWrap(true);error->hide();layout->addWidget(error);layout->addStretch();
        scroll->setWidget(content);tabs->addTab(scroll,name);pages_[index(id)].push_back({name,content,form,error});
    }
    const auto page=[&](const char *name){return modules(id).indexOf(QString::fromUtf8(name));};
    int p=page("Background");
    auto *backgroundMethod=choice(id,p,tr("Method"),"backgroundMethod",{"Chebyshev","Polynomial","Cosine","Spline","Bezier","Bruckner","None"},
        [](const Profile &v){return int(v.background.method);},[](Profile &v,int x){v.background.method=preprocessing::BackgroundMethod(x);});
    auto *automatic=boolean(id,p,tr("Automatic coefficients"),"backgroundAutomatic",[](const Profile &v){return v.background.automatic;},[](Profile &v,bool x){v.background.automatic=x;});
    auto *coefficients=integer(id,p,tr("Coefficients"),"backgroundCoefficients",0,36,[](const Profile &v){return v.background.coefficients;},[](Profile &v,int x){v.background.coefficients=x;});
    connect(automatic,&QCheckBox::toggled,coefficients,[coefficients](bool value){coefficients->setEnabled(!value);});
    auto *iterations=integer(id,p,tr("Bruckner iterations"),"backgroundIterations",0,100000,[](const Profile &v){return v.background.iterations;},[](Profile &v,int x){v.background.iterations=x;});
    auto *backgroundWindow=integer(id,p,tr("Bruckner half-window (samples)"),"backgroundWindow",0,100000,[](const Profile &v){return v.background.half_window;},[](Profile &v,int x){v.background.half_window=x;});
    auto *backgroundForm=pages_[index(id)][p].form;
    connect(backgroundMethod,&QComboBox::currentIndexChanged,this,[backgroundForm,iterations,backgroundWindow](int method){
        backgroundForm->setRowVisible(iterations->parentWidget(),method==5);
        backgroundForm->setRowVisible(backgroundWindow->parentWidget(),method==5);
    });
    note(id,p,tr("Automatic preparation uses the full current pattern. Existing compatible manual background takes precedence."));
    p=page("Initial Peaks");
    if(p>=0) {
        number(id,p,tr("Derivative threshold"),"initialSlope",0,200,[](const Profile &v){return v.initial_peaks.options.slope;},[](Profile &v,double x){v.initial_peaks.options.slope=float(x);});
        auto *autoWindow=boolean(id,p,tr("Automatic window"),"initialAutomatic",[](const Profile &v){return v.initial_peaks.options.automatic_window;},[](Profile &v,bool x){v.initial_peaks.options.automatic_window=x;});
        auto *window=integer(id,p,tr("Half-window (samples)"),"initialWindow",2,100,[](const Profile &v){return v.initial_peaks.options.half_window;},[](Profile &v,int x){v.initial_peaks.options.half_window=x;});
        connect(autoWindow,&QCheckBox::toggled,window,[window](bool x){window->setEnabled(!x);});
        integer(id,p,tr("SG polynomial degree"),"initialDegree",3,20,[](const Profile &v){return v.initial_peaks.options.polynomial_degree;},[](Profile &v,int x){v.initial_peaks.options.polynomial_degree=x;});
        note(id,p,id==WorkflowId::pfsm?
            tr("These SG initial-peak parameters are saved independently for PFSM. Automatic geometric FWHM detects its own candidate peaks and does not use these parameters."):
            tr("Full-pattern SG derivative candidates. Fixed FWHM needs no candidates; geometric FWHM detects its own candidates without this SG module."));
    }
    p=page("FWHM");
    if(id==WorkflowId::pfsm) note(id,p,tr("Automatic PFSM width uses geometric estimation with internal candidate detection; no separate SG initial search or formal peak extraction is inserted."));
    else choice(id,p,tr("Automatic method"),"fwhmMethod",{"Gaussian","Geometric"},[](const Profile &v){return int(v.fwhm.method);},[](Profile &v,int x){v.fwhm.method=FwhmMethod(x);});
    auto *fixed=boolean(id,p,tr("Use fixed width"),"fwhmFixed",[](const Profile &v){return v.fwhm.use_fixed;},[](Profile &v,bool x){v.fwhm.use_fixed=x;});
    auto *width=number(id,p,tr("Fixed FWHM (° 2θ)"),"fwhmWidth",1e-12,180,[](const Profile &v){return v.fwhm.default_fwhm.value;},[](Profile &v,double x){v.fwhm.default_fwhm=AngularWidthDegrees{x};});
    connect(fixed,&QCheckBox::toggled,width,&QWidget::setEnabled);
    integer(id,p,tr("Minimum samples per FWHM (Q)"),"fwhmSampling",2,8,[](const Profile &v){return v.fwhm.minimum_sampling_density;},[](Profile &v,int x){v.fwhm.minimum_sampling_density=x;});
    p=page("Peak Extraction");
    if(p>=0) {
        number(id,p,tr("Derivative threshold"),"peakSlope",0,1e6,[](const Profile &v){return v.peaks.slope;},[](Profile &v,double x){v.peaks.slope=float(x);});
        auto *autoWindow=boolean(id,p,tr("Automatic window"),"peakAutomatic",[](const Profile &v){return v.peaks.automatic_window;},[](Profile &v,bool x){v.peaks.automatic_window=x;});
        auto *window=integer(id,p,tr("Half-window (samples)"),"peakWindow",1,100,[](const Profile &v){return v.peaks.half_window;},[](Profile &v,int x){v.peaks.half_window=x;});
        connect(autoWindow,&QCheckBox::toggled,window,[window](bool x){window->setEnabled(!x);});
        integer(id,p,tr("SG polynomial degree"),"peakDegree",3,20,[](const Profile &v){return v.peaks.polynomial_degree;},[](Profile &v,int x){v.peaks.polynomial_degree=x;});
        note(id,p,tr("The fitting/search width is calibrated from physical FWHM. It is a result, not a saved fixed width."));
        if(id==WorkflowId::peak_search) p=page("Peak Selection");
        number(id,p,tr("Minimum angle (° 2θ)"),"selectionMinimum",0,180,[](const Profile &v){return v.selection.minimum;},[](Profile &v,double x){v.selection.minimum=x;});
        number(id,p,tr("Maximum angle (° 2θ)"),"selectionMaximum",0,180,[](const Profile &v){return v.selection.maximum;},[](Profile &v,double x){v.selection.maximum=x;});
        number(id,p,tr("Minimum relative height (%)"),"selectionThreshold",0,100,[](const Profile &v){return v.selection.threshold_percent;},[](Profile &v,double x){v.selection.threshold_percent=float(x);});
        integer(id,p,tr("Maximum peaks (−1: all)"),"selectionCount",-1,100000,[](const Profile &v){return v.selection.maximum_peaks;},[](Profile &v,int x){v.selection.maximum_peaks=x;});
        boolean(id,p,tr("Retain peaks outside this range"),"selectionAppend",[](const Profile &v){return v.selection.append;},[](Profile &v,bool x){v.selection.append=x;});
    }
    if(id==WorkflowId::preprocessing) {
        p=page("Range");
        auto *enabled=boolean(id,p,tr("Use saved range for explicit Range command"),"rangeEnabled",[](const Profile &v){return bool(v.range);},[](Profile &v,bool x){if(x&&!v.range)v.range=AngularRange{TwoThetaDegrees{0},TwoThetaDegrees{180}};if(!x)v.range.reset();});
        auto *lo=number(id,p,tr("Minimum angle (° 2θ)"),"rangeMinimum",0,180,[](const Profile &v){return v.range?v.range->minimum.value:0;},[](Profile &v,double x){if(v.range)v.range->minimum=TwoThetaDegrees{x};});
        auto *hi=number(id,p,tr("Maximum angle (° 2θ)"),"rangeMaximum",0,180,[](const Profile &v){return v.range?v.range->maximum.value:180;},[](Profile &v,double x){if(v.range)v.range->maximum=TwoThetaDegrees{x};});
        connect(enabled,&QCheckBox::toggled,lo,&QWidget::setEnabled);connect(enabled,&QCheckBox::toggled,hi,&QWidget::setEnabled);
        note(id,p,tr("Range is executed explicitly. Other workflows never crop the pattern automatically."));
        p=page("Smoothing");
        choice(id,p,tr("Method"),"smoothingMethod",{"Savitzky–Golay","Averaging"},[](const Profile &v){return int(v.smoothing.method);},[](Profile &v,int x){v.smoothing.method=preprocessing::SmoothingMethod(x);});
        integer(id,p,tr("Half-window (samples)"),"smoothingWindow",1,100000,[](const Profile &v){return v.smoothing.half_window;},[](Profile &v,int x){v.smoothing.half_window=x;});
        integer(id,p,tr("Polynomial degree"),"smoothingDegree",0,20,[](const Profile &v){return v.smoothing.polynomial_degree;},[](Profile &v,int x){v.smoothing.polynomial_degree=x;});
        note(id,p,tr("Smoothing is executed explicitly and changes the working observations."));
        note(id,page("Kα2"),tr("Kα2 stripping is an explicit operation with fixed algorithm rules and no adjustable parameters. The source radiation and stripping history must be known."));
    }
    if(id==WorkflowId::csm) {
        p=page("Candidate Screening");
        boolean(id,p,tr("Use strongest-reflection screening"),"csmStrongest",[](const Profile &v){return v.csm.strongest;},[](Profile &v,bool x){v.csm.strongest=x;});
        boolean(id,p,tr("Exclude deleted entries"),"csmDeleted",[](const Profile &v){return v.csm.exclude_deleted;},[](Profile &v,bool x){v.csm.exclude_deleted=x;});
        integer(id,p,tr("Maximum entries"),"csmEntries",1,std::numeric_limits<int>::max(),[](const Profile &v){return v.csm.maximum_entries;},[](Profile &v,int x){v.csm.maximum_entries=x;});
        p=page("Matching");auto *automaticDelta=boolean(id,p,tr("Automatic tolerance"),"csmAutomaticDelta",[](const Profile &v){return v.csm.automatic_delta;},[](Profile &v,bool x){v.csm.automatic_delta=x;});
        auto *delta=number(id,p,tr("Tolerance (° 2θ)"),"csmDelta",0,180,[](const Profile &v){return v.csm.delta;},[](Profile &v,double x){v.csm.delta=x;});
        connect(automaticDelta,&QCheckBox::toggled,delta,[delta](bool x){delta->setEnabled(!x);});
        p=page("FOM");
        number(id,p,tr("Position weight"),"csmPosition",0,1,[](const Profile &v){return v.csm.weight_position;},[](Profile &v,double x){v.csm.weight_position=x;});
        number(id,p,tr("Intensity weight"),"csmIntensity",0,1,[](const Profile &v){return v.csm.weight_intensity;},[](Profile &v,double x){v.csm.weight_intensity=x;});
        number(id,p,tr("Phase weight"),"csmPhases",0,1,[](const Profile &v){return v.csm.weight_phases;},[](Profile &v,double x){v.csm.weight_phases=x;});
        number(id,p,tr("Minimum FOM"),"csmMinimumFom",0,1,[](const Profile &v){return v.csm.minimum_fom;},[](Profile &v,double x){v.csm.minimum_fom=x;});
        boolean(id,page("Residual"),tr("Subtract accepted phase residual"),"csmResidual",[](const Profile &v){return v.csm.residual;},[](Profile &v,bool x){v.csm.residual=x;});
        note(id,page("Results"),tr("Candidates are ranked by FOM. Accepting/removing phases updates residual and quantitative results through checked project transactions. Reference RIR and existing normalization rules are retained."));
    }
    if(id==WorkflowId::pfsm) {
        p=page("Candidate Recall");
        auto *enabled=boolean(id,p,tr("Enable candidate recall"),"recallEnabled",[](const Profile &v){return v.pfsm.candidate_recall_enabled;},[](Profile &v,bool x){v.pfsm.candidate_recall_enabled=x;});
        auto *resource = choice(id,p,tr("On resource limit"),"recallResourceFallback",{"Bounded integral recall","Full-pool formal fitting"},
            [](const Profile &v){return int(v.pfsm.candidate_recall_resource_fallback);},
            [](Profile &v,int x){v.pfsm.candidate_recall_resource_fallback=PfsmResourceFallback(x);});
        connect(enabled,&QCheckBox::toggled,resource,&QWidget::setEnabled);
        note(id,p,tr("Candidate counts below apply to standard v2 recall. Bounded integral recall uses fixed budgets with cutoff ties and numerical protection."));
        const std::array<std::pair<const char *,int profile::PfsmCandidateRecallLimits::*>,5> limits{{
            {"Recall candidates",&profile::PfsmCandidateRecallLimits::recall_candidates},{"Local shortlist",&profile::PfsmCandidateRecallLimits::local_shortlist},
            {"Protected per channel",&profile::PfsmCandidateRecallLimits::protected_per_channel},{"Review limit",&profile::PfsmCandidateRecallLimits::review_limit},
            {"Formal limit",&profile::PfsmCandidateRecallLimits::formal_limit}}};
        int n=0;for(auto [label,member]:limits) {
            auto *count=integer(id,p,tr(label),"recallCount"+QString::number(n++),1,std::numeric_limits<int>::max(),
                [member](const Profile &v){return v.pfsm.candidate_recall_limits.*member;},[member](Profile &v,int x){v.pfsm.candidate_recall_limits.*member=x;});
            connect(enabled,&QCheckBox::toggled,count,&QWidget::setEnabled);
        }
        p=page("Profile Expansion");
        boolean(id,p,tr("Cache profile templates"),"pfsmCache",[](const Profile &v){return v.pfsm.template_cache;},[](Profile &v,bool x){v.pfsm.template_cache=x;});
        note(id,p,tr("Enabled (default): Uses interpolated templates during fitting iterations to reduce repeated "
                     "profile calculations and can speed up searches. Preparing and storing the cache takes extra "
                     "time and memory, so small searches may not benefit. Final fits are always verified by direct "
                     "profile evaluation."));
        note(id,p,tr("Disabled: Evaluates profiles directly throughout fitting, avoiding interpolation and its cache "
                     "preparation and memory costs. Repeated profile calculations can make searches slower."));
        note(id,p,tr("Uses the existing radiation components, measured FWHM and fixed profile expansion model."));
        p=page("Profile Fitting");
        number(id,p,tr("Lower shift (° 2θ)"),"pfsmLower",-180,180,[](const Profile &v){return v.pfsm.lower_shift;},[](Profile &v,double x){v.pfsm.lower_shift=x;});
        number(id,p,tr("Upper shift (° 2θ)"),"pfsmUpper",-180,180,[](const Profile &v){return v.pfsm.upper_shift;},[](Profile &v,double x){v.pfsm.upper_shift=x;});
        choice(id,p,tr("Weights"),"pfsmWeights",{"Automatic from attested noise","Equal"},[](const Profile &v){return int(v.pfsm.weighting);},[](Profile &v,int x){v.pfsm.weighting=PfsmSettings::Weighting(x);});
        integer(id,p,tr("Workers (0: automatic)"),"pfsmWorkers",0,std::numeric_limits<int>::max(),[](const Profile &v){return v.pfsm.threads;},[](Profile &v,int x){v.pfsm.threads=x;});
        note(id,page("FOM"),tr("Existing ΔRwp and relative-FOM definitions are fixed. Scores share the frozen analysis baseline."));
        note(id,page("Joint Baseline"),tr("Accepting or removing a phase refits the joint baseline using the existing nonnegative-scale solver and fixed verification policy. No additional solver controls are exposed."));
        p=page("Results");number(id,p,tr("Minimum relative FOM (display)"),"pfsmMinimum",0,1,[](const Profile &v){return v.pfsm.minimum_relative_fom;},[](Profile &v,double x){v.pfsm.minimum_relative_fom=x;});
        note(id,p,tr("This filter changes the displayed candidates only. Quantification retains the analysis normalization and explicitly recorded RIR revisions."));
    }
}
bool WorkflowSettingsDialog::load(WorkflowId flow,const QString &module) {
    auto loaded=workspace_.session().projectDefaults();
    if(!loaded) {QMessageBox::warning(parentWidget(),tr("Settings unavailable"),QString::fromStdString(loaded.error().message));return false;}
    draft_=std::move(loaded).value();refresh();ensureFlow(flow);flows_->setCurrentRow(int(flow));
    if(!module.isEmpty()) {const int at=modules(flow).indexOf(module);if(at>=0)tabs_[index(flow)]->setCurrentIndex(at);}
    setWindowTitle(tr("Setting — Defaults for New Projects"));
    status_->setText(tr("These defaults apply to new projects. The current project is unchanged."));
    return true;
}
WorkflowId WorkflowSettingsDialog::selectedWorkflow() const {return WorkflowId(std::max(0,flows_->currentRow()));}
QString WorkflowSettingsDialog::selectedModule() const {const auto id=selectedWorkflow();return tabs_[index(id)]->tabText(tabs_[index(id)]->currentIndex());}
void WorkflowSettingsDialog::refresh() {
    for(auto id:workflows)refresh(id);
}
void WorkflowSettingsDialog::refresh(WorkflowId id) {
    for(auto &f:fields_) if(f.flow==id) {f.write();f.error->hide();}
    // A checkbox initialized to its existing value emits no toggled signal.
    for(auto &f:fields_) if(f.flow==id) if(auto *box=qobject_cast<QCheckBox *>(f.control)) emit box->toggled(box->isChecked());
    for(auto &f:fields_) if(f.flow==id) if(auto *box=qobject_cast<QComboBox *>(f.control)) emit box->currentIndexChanged(box->currentIndex());
    for(auto &page:pages_[index(id)]) page.error->hide();
}
void WorkflowSettingsDialog::copyPage(Profile &to,const Profile &from,const QString &page) const {
    if(page=="Background") to.background=from.background;
    else if(page=="Initial Peaks") to.initial_peaks=from.initial_peaks;
    else if(page=="FWHM") to.fwhm=from.fwhm;
    else if(page=="Range") to.range=from.range;
    else if(page=="Smoothing") to.smoothing=from.smoothing;
    else if(page=="Peak Extraction") {to.peaks=from.peaks;if(selectedWorkflow()==WorkflowId::csm)to.selection=from.selection;}
    else if(page=="Peak Selection") to.selection=from.selection;
    else if(page=="Candidate Screening") {to.csm.strongest=from.csm.strongest;to.csm.exclude_deleted=from.csm.exclude_deleted;to.csm.maximum_entries=from.csm.maximum_entries;}
    else if(page=="Matching") {to.csm.automatic_delta=from.csm.automatic_delta;to.csm.delta=from.csm.delta;}
    else if(page=="FOM") {to.csm.minimum_fom=from.csm.minimum_fom;to.csm.weight_position=from.csm.weight_position;to.csm.weight_intensity=from.csm.weight_intensity;to.csm.weight_phases=from.csm.weight_phases;}
    else if(page=="Residual") to.csm.residual=from.csm.residual;
    else if(page=="Candidate Recall") {to.pfsm.candidate_recall_enabled=from.pfsm.candidate_recall_enabled;to.pfsm.candidate_recall_limits=from.pfsm.candidate_recall_limits;to.pfsm.candidate_recall_resource_fallback=from.pfsm.candidate_recall_resource_fallback;}
    else if(page=="Profile Expansion") to.pfsm.template_cache=from.pfsm.template_cache;
    else if(page=="Profile Fitting") {to.pfsm.lower_shift=from.pfsm.lower_shift;to.pfsm.upper_shift=from.pfsm.upper_shift;to.pfsm.weighting=from.pfsm.weighting;to.pfsm.threads=from.pfsm.threads;}
    else if(page=="Results") to.pfsm.minimum_relative_fom=from.pfsm.minimum_relative_fom;
}
bool WorkflowSettingsDialog::collect() {
    for(auto &f:fields_) {
        f.error->hide();const QString error=f.check?f.check():QString();
        if(!error.isEmpty()) {flows_->setCurrentRow(int(f.flow));tabs_[index(f.flow)]->setCurrentIndex(f.page);f.error->setText(error);f.error->show();f.control->setFocus();return false;}
        f.read();
    }
    for(auto flow:workflows) {
        ProjectSettings isolated;isolated.profile(flow)=draft_.profile(flow);
        const auto valid=validateProjectSettings(isolated);
        if(!valid) {
            const auto field=valid.error().field;QString module="Background",key;
            if(field.starts_with("pfsm.candidate_recall")) {
                module="Candidate Recall";key="recallCount1";
                const std::array<const char *,5> names{"recall_candidates","local_shortlist","protected_per_channel","review_limit","formal_limit"};
                for(int i=0;i<int(names.size());++i) if(field.ends_with(names[i]))key="recallCount"+QString::number(i);
            }
            else if(field.starts_with("pfsm.")) {module="Profile Fitting";key="pfsmLower";}
            else if(field.starts_with("fwhm")) {module="FWHM";key="fwhmWidth";}
            else if(field.starts_with("peaks.initial")) {module="Initial Peaks";key="initialSlope";}
            else if(field=="settings.range") {module="Range";key="rangeMinimum";}
            else if(field=="settings.csm") module="FOM";
            else if(draft_.profile(flow).selection.minimum>draft_.profile(flow).selection.maximum) {module=flow==WorkflowId::peak_search?"Peak Selection":"Peak Extraction";key="selectionMinimum";}
            const int page=std::max(0,int(modules(flow).indexOf(module)));flows_->setCurrentRow(int(flow));tabs_[index(flow)]->setCurrentIndex(page);
            auto &p=pages_[index(flow)][page];p.error->setText(QString::fromStdString(valid.error().message));p.error->show();
            for(auto &f:fields_) if(f.flow==flow&&f.control->objectName()==prefix(flow)+key) {f.error->setText(p.error->text());f.error->show();f.control->setFocus();break;}
            return false;
        }
    }
    return true;
}
void WorkflowSettingsDialog::restorePage() {
    // Preserve all other page drafts, even when the current page contains invalid text.
    for(auto &f:fields_) if((!f.check||f.check().isEmpty())) f.read();
    const auto flow=selectedWorkflow();const auto page=tabs_[index(flow)]->currentIndex();
    copyPage(draft_.profile(flow),ProjectSettings{}.profile(flow),selectedModule());
    for(auto &f:fields_) if(f.flow==flow&&f.page==page) {f.write();f.error->hide();}
    pages_[index(flow)][page].error->hide();
}
void WorkflowSettingsDialog::save() {
    if(workspace_.searches().closing()||workspace_.searches().running()||!collect()) return;
    auto status=workspace_.session().saveDefaultSettings(draft_);
    if(!status) {status_->setText(QString::fromStdString(status.error().message));return;}
    QDialog::accept();
}
}
