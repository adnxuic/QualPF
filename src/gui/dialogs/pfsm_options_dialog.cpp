#include "gui/dialogs/pfsm_options_dialog.h"
#include "gui/mainwindow.h"
#include "gui/adapters/preferences.h"
#include "gui/adapters/metadata.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <climits>
#include <cmath>
using namespace qualpf;
using namespace qualpf::application;
namespace {
constexpr std::array<const char *, 5> recallKeys{
    "recallCandidates", "localShortlist", "protectedPerChannel", "reviewLimit", "formalLimit"};
QLabel *label(const QString &text = {}) {
    auto *out = new QLabel(text); out->setWordWrap(true); out->setTextFormat(Qt::PlainText); return out;
}
QLabel *errorLabel(const QString &name) {
    auto *out = label(); out->setObjectName(name); out->setStyleSheet("color: #b42318;"); out->hide(); return out;
}
QFormLayout *formLayout(QWidget *parent = nullptr) {
    auto *out = new QFormLayout(parent);
    out->setRowWrapPolicy(QFormLayout::WrapLongRows);
    out->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    out->setSizeConstraint(QLayout::SetMinAndMaxSize);
    out->setVerticalSpacing(12); return out;
}
// Do not coerce fractional/overflowing counts or unknown switches before validation.
MetadataValue recallPreference(const QVariant &value, bool boolean) {
    if (boolean) {
        if (value.metaType().id() == QMetaType::Bool) return value.toBool();
        if (value.metaType().id() == QMetaType::QString) {
            if (value.toString() == "true") return true;
            if (value.toString() == "false") return false;
        }
        return value.toString().toStdString();
    }
    if (value.metaType().id() == QMetaType::Bool) return value.toBool();
    if (value.metaType().id() == QMetaType::Double || value.metaType().id() == QMetaType::Float) return value.toDouble();
    bool ok = false;
    const auto count = value.toLongLong(&ok);
    return ok ? MetadataValue{std::int64_t(count)} : MetadataValue{value.toString().toStdString()};
}
}
PfsmOptionsDialog::PfsmOptionsDialog(MainWindow *window) : QDialog(window), window_(window) {
    setObjectName("pfsmOptionsDialog");
    setWindowTitle(tr("PFSM Options"));
    setSizeGripEnabled(true);
    auto *layout = new QVBoxLayout(this);
    recallWarning_ = label(); recallWarning_->setObjectName("pfsmRecallWarning"); recallWarning_->hide(); layout->addWidget(recallWarning_);
    tabs_ = new QTabWidget; tabs_->setObjectName("pfsmOptionsTabs"); layout->addWidget(tabs_, 1);
    const auto page = [&](const QString &title, const char *name, QScrollArea *&scroll) {
        scroll = new QScrollArea; scroll->setObjectName(name); scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame); scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *content = new QWidget;
        auto *box = new QVBoxLayout(content); box->setContentsMargins(16,16,16,16); box->setSpacing(14);
        box->setSizeConstraint(QLayout::SetMinAndMaxSize);
        scroll->setWidget(content);
        tabs_->addTab(scroll, title); return box;
    };
    auto *general = page(tr("General"), "pfsmGeneralScroll", generalScroll_);
    auto *form = formLayout();
    auto *bounds = new QHBoxLayout; bounds->setSpacing(8);
    lower_ = new QLineEdit; upper_ = new QLineEdit;
    lower_->setObjectName("lowerShift"); upper_->setObjectName("upperShift");
    lower_->setAccessibleName(tr("Lower shift bound")); upper_->setAccessibleName(tr("Upper shift bound"));
    for (auto *edit : {lower_, upper_}) {
        auto *validator = new QDoubleValidator(edit); validator->setLocale(QLocale::c());
        edit->setValidator(validator); edit->setMaximumWidth(110); bounds->addWidget(edit);
        if (edit == lower_) bounds->addWidget(new QLabel(tr("to")));
    }
    bounds->addWidget(new QLabel(tr("°2θ"))); bounds->addStretch();
    form->addRow(tr("Shift bounds"), bounds);
    shiftError_ = errorLabel("pfsmShiftError"); form->addRow(shiftError_);
    width_ = new QLabel; width_->setObjectName("pfsmFwhmSource"); width_->setWordWrap(true);
    form->addRow(tr("FWHM"), width_);
    form->addRow(tr("Signal"), new QLabel(tr("Current working observations")));
    weighting_ = new QComboBox; weighting_->setObjectName("pfsmWeighting");
    weighting_->addItems({tr("Auto from input"), tr("Equal weights")});
    form->addRow(tr("Weighting"), weighting_);
    resolved_ = new QLabel; resolved_->setObjectName("pfsmResolvedWeights");
    resolved_->setWordWrap(true);
    form->addRow(tr("Resolved"), resolved_);
    auto *fom = new QHBoxLayout;
    slider_ = new QSlider(Qt::Horizontal); slider_->setRange(0, 1000);
    minimum_ = new QDoubleSpinBox; minimum_->setObjectName("minimumRelativeFom");
    slider_->setAccessibleName(tr("Minimum relative FOM")); minimum_->setAccessibleName(tr("Minimum relative FOM"));
    minimum_->setDecimals(3); minimum_->setRange(0, 1); minimum_->setSingleStep(.01);
    fom->addWidget(slider_); fom->addWidget(minimum_);
    form->addRow(tr("Min. relative FOM"), fom);
    general->addLayout(form);
    auto *advanced = advanced_ = new QToolButton;
    advanced->setObjectName("pfsmAdvanced");
    advanced->setText(tr("Advanced")); advanced->setCheckable(true);
    advanced->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); advanced->setArrowType(Qt::RightArrow);
    auto *panel = new QWidget; panel->setVisible(false);
    auto *advancedForm = formLayout(panel);
    workers_ = new QSpinBox; workers_->setObjectName("pfsmWorkers");
    workers_->setRange(0, INT_MAX); workers_->setSpecialValueText(tr("Auto"));
    cache_ = new QCheckBox(tr("Enable (direct final verification)")); cache_->setObjectName("pfsmTemplateCache");
    advancedForm->addRow(tr("Workers"), workers_); advancedForm->addRow(tr("Template cache"), cache_);
    advancedForm->addRow(label(tr("Enabled (default): Uses interpolated templates during fitting iterations to reduce "
                                 "repeated profile calculations and can speed up searches. Preparing and storing the "
                                 "cache takes extra time and memory, so small searches may not benefit. Final fits "
                                 "are always verified by direct profile evaluation.")));
    advancedForm->addRow(label(tr("Disabled: Evaluates profiles directly throughout fitting, avoiding interpolation "
                                 "and its cache preparation and memory costs. Repeated profile calculations can "
                                 "make searches slower.")));
    general->addWidget(advanced); general->addWidget(panel); general->addStretch();
    auto *scopeNotice=label(tr("FOM: immediate display filter. Workers: next run.\nShift bounds, weighting and cache: next new analysis."));scopeNotice->setObjectName("pfsmScopeNotice");general->addWidget(scopeNotice);
    auto *recall = page(tr("Candidate Recall"), "pfsmRecallScroll", recallScroll_);
    recallEnabled_ = new QCheckBox(tr("Enable candidate recall")); recallEnabled_->setObjectName("pfsmRecallEnabled");
    recall->addWidget(recallEnabled_); recall->addWidget(label(tr("Preselect candidates before formal PFSM fitting.")));
    recall->addWidget(label(tr("The counts below apply to standard v2 recall. Bounded integral recall uses fixed channel budgets and retains cutoff ties and protected candidates.")));
    auto *resourceForm = new QFormLayout;
    resourceFallback_ = new QComboBox; resourceFallback_->setObjectName("pfsmResourceFallback");
    resourceFallback_->addItems({tr("Bounded integral recall"), tr("Full-pool formal fitting")});
    resourceForm->addRow(tr("On resource limit"), resourceFallback_); recall->addLayout(resourceForm);
    auto *selection = new QGroupBox(tr("Candidate selection"));
    auto *fallback = new QGroupBox(tr("Full-pool fallback thresholds"));
    auto *selectionForm = formLayout(selection), *fallbackForm = formLayout(fallback);
    const std::array<QString, 5> names{tr("Recall candidates"),tr("Local shortlist"),
        tr("Protected candidates per channel"),tr("Review set size"),tr("Formal candidate set size")};
    const std::array<QString, 5> tips{
        tr("Total target from alternating original and residual channels; cutoff ties and numeric protection are additional."),
        tr("Top-K used before and after local review, including cutoff ties. Protected candidates are additional."),
        tr("Protect the top candidates of each channel within the recalled set, including cutoff ties."),
        tr("Fit the full pool when the review set exceeds this value. Protected candidates are never truncated."),
        tr("Fit the full pool when the proposed formal set exceeds this value. This is not a fit-count cap.")};
    for (std::size_t i=0;i<recallCounts_.size();++i) {
        auto *spin = recallCounts_[i] = new QSpinBox;
        spin->setObjectName(QString::fromLatin1(recallKeys[i])); spin->setRange(1,INT_MAX); spin->setKeyboardTracking(false);
        spin->setAccessibleName(names[i]); spin->setToolTip(tips[i]); spin->setSizePolicy(QSizePolicy::Maximum,QSizePolicy::Fixed);
        auto *field = new QHBoxLayout; field->addStretch(); field->addWidget(spin);
        recallErrors_[i] = errorLabel(QString::fromLatin1(recallKeys[i])+"Error");
        auto *rows = i<3 ? selectionForm : fallbackForm;
        rows->addRow(names[i],field); rows->addRow(recallErrors_[i]);
    }
    recall->addWidget(selection); recall->addWidget(fallback);
    recall->addWidget(label(tr("Above either threshold, fit the full candidate pool.\nCutoff ties and protected candidates remain included.")));
    currentRecall_ = label(); currentRecall_->setObjectName("pfsmCurrentRecall"); currentRecall_->hide(); recall->addWidget(currentRecall_);
    recall->addStretch(); recall->addWidget(label(tr("Recall changes apply to the next new PFSM analysis.")));
    error_ = errorLabel("pfsmOptionsError"); layout->addWidget(error_);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Cancel | QDialogButtonBox::Ok);
    buttons->setObjectName("pfsmOptionsButtons"); layout->addWidget(buttons);
    connect(advanced, &QToolButton::toggled, this, [this, advanced, panel](bool open) {
        panel->setVisible(open); advanced->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    });
    connect(recallEnabled_, &QCheckBox::toggled, this, &PfsmOptionsDialog::updateRecallEnabled);
    connect(slider_, &QSlider::valueChanged, this, [this](int v) { QSignalBlocker b(minimum_); minimum_->setValue(v / 1000.); });
    connect(minimum_, &QDoubleSpinBox::valueChanged, this, [this](double v) { QSignalBlocker b(slider_); slider_->setValue(qRound(v * 1000)); });
    connect(weighting_, &QComboBox::currentIndexChanged, this, &PfsmOptionsDialog::updateResolved);
    connect(buttons, &QDialogButtonBox::accepted, this, &PfsmOptionsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] { setWidgets({}); recallWarning_->hide(); });
}
PfsmSettings qualpf::gui::savedPfsmSettings(WorkspaceContext &context, std::optional<Error> *recallWarning) {
    auto source = context.workspace().session().snapshot();
    if (source->projectSettings) {if(recallWarning)recallWarning->reset();return source->projectSettings->profile(WorkflowId::pfsm).pfsm;}
    return pfsmSettings(source->searchOptions, recallWarning);
}
bool PfsmOptionsDialog::setOptions() {
    if (window_->isClosing() || window_->searchTasks()->busy()) return false;
    source_ = window_->context().workspace().session().snapshot();
    const auto source = source_;
    std::optional<Error> warning;
    setWidgets(qualpf::gui::savedPfsmSettings(window_->context(), &warning));
    tabs_->setCurrentIndex(0); advanced_->setChecked(false);
    recallWarning_->setVisible(bool(warning));
    if (warning) recallWarning_->setText(tr("Invalid saved candidate-recall settings; defaults are shown for the affected settings.\n%1")
        .arg(QString::fromStdString(warning->message)));
    if (source->projectSettings) {
        if (auto *notice=findChild<QLabel *>("pfsmScopeNotice")) notice->setText(tr("These options apply to the current project. FOM filters the display; workers apply to the next run. Changes to frozen inputs require confirmation before dependent results are removed."));
    }
    currentRecall_->setVisible(bool(source->pfsm));
    if (source->pfsm) {
        const auto &c = source->pfsm->problem->candidate_recall;
        const auto &v = c.limits;
        const auto mode = c.mode == PfsmCandidateRecallMode::off ? tr("Off") :
            (c.mode == PfsmCandidateRecallMode::enabled ? tr("On") : tr("Shadow"));
        currentRecall_->setText(tr("Current analysis (frozen): %1\nRecall %2 · Local %3 · Per channel %4\nReview threshold %5 · Formal threshold %6\nOn resource limit: %7")
            .arg(mode).arg(v.recall_candidates).arg(v.local_shortlist).arg(v.protected_per_channel).arg(v.review_limit).arg(v.formal_limit)
            .arg(c.resource_fallback == PfsmResourceFallback::full_pool ? tr("Full-pool formal fitting") : tr("Bounded integral recall")));
    }
    const auto configuration = source->projectSettings ? source->projectSettings->profile(WorkflowId::pfsm).fwhm : source->preprocessing.fwhm.value_or(FwhmConfiguration{});
    QString width = tr("Automatic · Geometric if needed (Q = %1)").arg(configuration.minimum_sampling_density);
    if (source->pfsm && source->pfsm->problem->physical_width) {
        const auto &u = *source->pfsm->problem->physical_width;
        width = tr("%1 °2θ · %2 · frozen for current analysis").arg(u.width.value, 0, 'g', 12)
            .arg(u.origin == FwhmOrigin::fixed ? tr("Fixed") : (u.manual ? tr("Manual") : tr("Automatic geometric")));
    } else if (configuration.use_fixed)
        width = tr("%1 °2θ · Fixed").arg(configuration.default_fwhm.value, 0, 'g', 12);
    else if (source->preprocessing.fwhm_usage && compatibleManualPfsmWidth(*source, *source->preprocessing.fwhm_usage))
        width = tr("%1 °2θ · Manual %2").arg(source->preprocessing.fwhm_usage->width.value, 0, 'g', 12)
            .arg(source->preprocessing.fwhm_usage->origin == FwhmOrigin::gaussian ? tr("Gaussian") : tr("geometric"));
    width_->setText(width);
    const auto available = screen()->availableGeometry().size();
    resize(std::min(640, available.width()-40), std::min(650, available.height()-80));
    return true;
}
void PfsmOptionsDialog::clearErrors() {
    error_->clear(); error_->hide(); shiftError_->clear(); shiftError_->hide();
    for (auto *error : recallErrors_) { error->clear(); error->hide(); }
}
void PfsmOptionsDialog::setWidgets(const PfsmSettings &s) {
    const auto number = [](double value) {
        const auto shortText = QString::number(value, 'f', 3);
        return shortText.toDouble() == value ? shortText : QString::number(value, 'g', 17);
    };
    lower_->setText(number(s.lower_shift)); upper_->setText(number(s.upper_shift));
    weighting_->setCurrentIndex(s.weighting == PfsmSettings::Weighting::equal ? 1 : 0);
    minimum_->setValue(s.minimum_relative_fom); slider_->setValue(qRound(s.minimum_relative_fom * 1000));
    workers_->setValue(s.threads); cache_->setChecked(s.template_cache);
    recallEnabled_->setChecked(s.candidate_recall_enabled);
    resourceFallback_->setCurrentIndex(s.candidate_recall_resource_fallback == PfsmResourceFallback::full_pool ? 1 : 0);
    const auto &v = s.candidate_recall_limits;
    const std::array<int, 5> values{v.recall_candidates,v.local_shortlist,v.protected_per_channel,v.review_limit,v.formal_limit};
    for (std::size_t i=0;i<values.size();++i) recallCounts_[i]->setValue(values[i]);
    updateResolved(); updateRecallEnabled(); clearErrors();
}
void PfsmOptionsDialog::updateRecallEnabled() {
    resourceFallback_->setEnabled(recallEnabled_->isChecked());
    for (auto *spin : recallCounts_) spin->setEnabled(recallEnabled_->isChecked());
}
void PfsmOptionsDialog::updateResolved() {
    const auto source = window_->context().workspace().session().snapshot();
    const auto &e = source->original_signal_provenance;
    resolved_->setText(weighting_->currentIndex() == 1 ? tr("Equal weights · selected by user") :
        (e && e->standard_deviations ? tr("Inverse variance · supplied σ") :
        (e && e->raw_poisson_counts ? tr("Poisson · declared raw counts") : tr("Equal weights · noise unknown"))));
}
void PfsmOptionsDialog::showRecallError(std::size_t i, const QString &message) {
    tabs_->setCurrentIndex(1); recallErrors_[i]->setText(message); recallErrors_[i]->show();
    if (recallCounts_[i]->isEnabled()) { recallCounts_[i]->setFocus(); recallCounts_[i]->selectAll(); }
    else recallEnabled_->setFocus();
    recallScroll_->ensureWidgetVisible(recallCounts_[i]);
}
void PfsmOptionsDialog::accept() {
    if (window_->isClosing() || window_->searchTasks()->busy()) return;
    clearErrors();
    PfsmSettings s;
    bool low = false, high = false;
    s.lower_shift = QLocale::c().toDouble(lower_->text(), &low); s.upper_shift = QLocale::c().toDouble(upper_->text(), &high);
    if (!low || !high || !std::isfinite(s.lower_shift) || !std::isfinite(s.upper_shift) || s.lower_shift > s.upper_shift) {
        tabs_->setCurrentIndex(0); shiftError_->setText(tr("Enter finite shift bounds with lower ≤ upper.")); shiftError_->show();
        auto *edit = !low || !std::isfinite(s.lower_shift) || s.lower_shift > s.upper_shift ? lower_ : upper_;
        edit->setFocus(); generalScroll_->ensureWidgetVisible(edit); return;
    }
    s.minimum_relative_fom = minimum_->value(); s.threads = workers_->value(); s.template_cache = cache_->isChecked();
    s.weighting = weighting_->currentIndex() == 1 ? PfsmSettings::Weighting::equal : PfsmSettings::Weighting::automatic;
    s.candidate_recall_enabled = recallEnabled_->isChecked();
    s.candidate_recall_resource_fallback = resourceFallback_->currentIndex() == 1 ? PfsmResourceFallback::full_pool : PfsmResourceFallback::bounded_integral;
    for (std::size_t i=0;i<recallCounts_.size();++i) {
        if (!recallCounts_[i]->hasAcceptableInput()) { showRecallError(i,tr("Enter an integer from 1 to 2147483647.")); return; }
        recallCounts_[i]->interpretText();
    }
    s.candidate_recall_limits = {recallCounts_[0]->value(),recallCounts_[1]->value(),recallCounts_[2]->value(),
                                recallCounts_[3]->value(),recallCounts_[4]->value()};
    const auto valid = validatePfsmSettings(s);
    if (!valid) {
        for (std::size_t i=0;i<recallKeys.size();++i)
            if (valid.error().field == std::string("pfsm.candidate_recall.")+recallKeys[i]) {
                showRecallError(i,QString::fromStdString(valid.error().message)); return;
            }
        error_->setText(QString::fromStdString(valid.error().message)); error_->show(); return;
    }
    auto &context = window_->context();
    if (source_ && source_->projectSettings) {
        auto settings=*source_->projectSettings;settings.profile(WorkflowId::pfsm).pfsm=s;
        auto status=window_->applyProjectOptions(source_,settings);
        if (!status) {if(status.error().code!=ErrorCode::cancelled){error_->setText(QString::fromStdString(status.error().message));error_->show();}return;}
        emit settingsApplied();QDialog::accept();return;
    }
    auto source = context.workspace().session().snapshot();
    auto next = *source; storePfsmSettings(next.searchOptions, s);
    if (next.projectSettings) next.projectSettings->profile(WorkflowId::pfsm).pfsm = s;
    auto status = context.workspace().session().commit(source, std::move(next));
    if (!status) { error_->setText(QString::fromStdString(status.error().message)); error_->show(); return; }
    emit settingsApplied(); QDialog::accept();
}
