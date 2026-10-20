#include "gui/dialogs/fwhm_options_dialog.h"
#include "qualpf/application/workflow.h"
#include "gui/mainwindow.h"
#include "gui/state/preprocessing_product.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cmath>

using namespace qualpf;
using namespace qualpf::application;
FwhmOptionsDialog::FwhmOptionsDialog(MainWindow *window) : QDialog(window), window_(window) {
    setObjectName("fwhmOptionsDialog");
    setWindowTitle(tr("FWHM Estimation Options"));
    setWindowModality(Qt::WindowModal);
    setMinimumWidth(462);
    auto *layout = new QVBoxLayout(this);
    auto *algorithmLayout = new QFormLayout;
    algorithm_ = new QComboBox(this); algorithm_->setObjectName("fwhmAlgorithmComboBox");
    algorithm_->addItem(tr("Gaussian fit (Classical)")); algorithm_->addItem(tr("Geometric FWHM"));
    algorithmLayout->addRow(tr("Algorithm:"), algorithm_); layout->addLayout(algorithmLayout);
    samplingPanel_ = new QWidget(this); samplingPanel_->setObjectName("fwhmSamplingPanel");
    auto *samplingLayout = new QVBoxLayout(samplingPanel_); samplingLayout->setContentsMargins(0, 0, 0, 0);
    samplingLayout->addWidget(new QLabel(tr("Minimum sampling density Q:"), samplingPanel_));
    auto *controls = new QHBoxLayout;
    samplingSlider_ = new QSlider(Qt::Horizontal, samplingPanel_); samplingSlider_->setObjectName("fwhmSamplingSlider");
    samplingSpin_ = new QSpinBox(samplingPanel_); samplingSpin_->setObjectName("fwhmSamplingSpinBox");
    samplingSlider_->setRange(2, 8); samplingSpin_->setRange(2, 8); samplingSpin_->setKeyboardTracking(false);
    const auto explanation = tr("Minimum FWHM divided by the largest sample step across the measured peak. Default: 6.");
    samplingSlider_->setToolTip(explanation); samplingSpin_->setToolTip(explanation);
    controls->addWidget(samplingSlider_); controls->addWidget(samplingSpin_); samplingLayout->addLayout(controls);
    layout->addWidget(samplingPanel_);
    auto separator = [this, layout](const char *name) {
        auto *line = new QFrame(this); line->setObjectName(QString::fromLatin1(name));
        line->setFrameShape(QFrame::HLine); line->setFrameShadow(QFrame::Sunken); layout->addWidget(line);
    };
    separator("fwhmAlgorithmSeparator");
    fixed_ = new QCheckBox(tr("Use fixed Default FWHM"), this); fixed_->setObjectName("useFixedFwhmCheckBox");
    layout->addWidget(fixed_);
    auto *widthLayout = new QFormLayout;
    width_ = new QLineEdit(this); width_->setObjectName("defaultFwhmEdit");
    auto *validator = new QDoubleValidator(0, 180, 16, width_); validator->setLocale(QLocale::c());
    validator->setNotation(QDoubleValidator::ScientificNotation); width_->setValidator(validator);
    widthLayout->addRow(tr("Default FWHM (\u00b02\u03b8):"), width_); layout->addLayout(widthLayout);
    status_ = new QLabel(this); status_->setObjectName("fwhmStatus"); status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse); layout->addWidget(status_);
    separator("fwhmButtonSeparator");
    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    buttons_->setObjectName("buttonBox"); layout->addWidget(buttons_);
    connect(samplingSlider_, &QSlider::valueChanged, samplingSpin_, &QSpinBox::setValue);
    connect(samplingSpin_, &QSpinBox::valueChanged, samplingSlider_, &QSlider::setValue);
    connect(algorithm_, &QComboBox::currentIndexChanged, this, [this] { updateMode(); });
    connect(samplingSpin_, &QSpinBox::valueChanged, this, [this] {
        if (!fixed_->isChecked()) status_->setText(tr("Awaiting automatic estimation"));
    });
    connect(fixed_, &QCheckBox::toggled, this, [this](bool fixed) {
        if (!fixed && widthEdited_) {
            bool ok = false; const double value = QLocale::c().toDouble(width_->text(), &ok);
            if (ok && std::isfinite(value) && value > 0 && value <= 180) draft_.default_fwhm = AngularWidthDegrees{value};
            const QSignalBlocker blocker(width_); width_->setText(QString::number(draft_.default_fwhm.value, 'g', 12));
            widthEdited_ = false;
        }
        updateMode();
    });
    connect(width_, &QLineEdit::textChanged, this, [this] {
        if (!width_->isReadOnly()) {
            widthEdited_ = true;
            status_->setText(tr("Fixed value; FWHM estimation is skipped"));
        }
    });
    connect(buttons_, &QDialogButtonBox::accepted, this, &FwhmOptionsDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &FwhmOptionsDialog::reject);
    connect(buttons_->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &FwhmOptionsDialog::restoreDefaults);
    connect(window->searchTasks(), &qualpf::gui::SearchTaskController::drained, this, [this] {
        if (running_ && window_ && window_->isClosing()) { running_ = false; QDialog::reject(); }
    });
    resize(462, sizeHint().height());
}
bool FwhmOptionsDialog::setOptions() {
    if (!window_ || window_->isClosing() || window_->searchTasks()->busy()) return false;
    auto allowed = qualpf::gui::peakEditEntryStatus(window_->context());
    if (!allowed) return false;
    source_ = window_->context().workspace().session().snapshot();
    if (source_->pattern.observed.empty()) return false;
    draft_ = source_->projectSettings ? source_->projectSettings->profile(WorkflowId::preprocessing).fwhm : source_->preprocessing.fwhm.value_or(FwhmConfiguration{});
    result_.reset(); running_ = cancelRequested_ = widthEdited_ = false;
    setWidgets();
    if (!draft_.use_fixed) {
        const auto &usage = source_->preprocessing.fwhm_usage;
        if (usage && usage->current && (!source_->projectSettings || reusableWorkflowStep(*source_, WorkflowId::preprocessing, WorkflowStep::fwhm))) status_->setText(tr("Estimated with %1").arg(
            usage->origin == FwhmOrigin::geometric ? tr("Geometric FWHM (noise unknown)") : tr("Gaussian fit (Classical)")));
        else status_->setText(usage ? tr("Previous value; awaiting automatic estimation") : tr("Initial value; not yet estimated"));
    }
    window_->saveEnabledActions(); window_->enableActions(MainWindow::DialogOpenAction);
    return true;
}
void FwhmOptionsDialog::setWidgets() {
    const QSignalBlocker a(algorithm_), f(fixed_), w(width_), s(samplingSpin_), l(samplingSlider_);
    algorithm_->setCurrentIndex(draft_.method == FwhmMethod::geometric ? 1 : 0);
    samplingSpin_->setValue(draft_.minimum_sampling_density); samplingSlider_->setValue(draft_.minimum_sampling_density);
    fixed_->setChecked(draft_.use_fixed); width_->setText(QString::number(draft_.default_fwhm.value, 'g', 12));
    widthEdited_ = false; updateMode();
}
void FwhmOptionsDialog::updateMode() {
    const bool fixed = fixed_->isChecked();
    samplingPanel_->setVisible(algorithm_->currentIndex() == 1);
    algorithm_->setEnabled(!fixed && !running_); samplingPanel_->setEnabled(!fixed && !running_);
    width_->setReadOnly(!fixed || running_);
    status_->setText(fixed ? tr("Fixed value; FWHM estimation is skipped") : tr("Awaiting automatic estimation"));
    status_->setToolTip({});
    adjustSize();
}
void FwhmOptionsDialog::setRunning(bool running) {
    running_ = running;
    fixed_->setEnabled(!running); buttons_->button(QDialogButtonBox::Ok)->setEnabled(!running);
    buttons_->button(QDialogButtonBox::RestoreDefaults)->setEnabled(!running);
    updateMode();
    if (running) status_->setText(draft_.use_fixed ? tr("Applying fixed FWHM...") : tr("Estimating FWHM..."));
}
void FwhmOptionsDialog::restoreDefaults() {
    const auto extensions = draft_.extensions;
    draft_ = {}; draft_.extensions = extensions; result_.reset(); setWidgets();
}
void FwhmOptionsDialog::accept() {
    if (running_ || !window_ || window_->isClosing()) return;
    draft_.method = algorithm_->currentIndex() == 1 ? FwhmMethod::geometric : FwhmMethod::gaussian;
    draft_.use_fixed = fixed_->isChecked(); draft_.minimum_sampling_density = samplingSpin_->value();
    if (draft_.use_fixed && widthEdited_) {
        bool ok = false; const double value = QLocale::c().toDouble(width_->text(), &ok);
        if (!ok || !std::isfinite(value) || value <= 0 || value > 180) {
            status_->setText(tr("Enter a finite FWHM greater than 0 and at most 180 degrees.")); return;
        }
        draft_.default_fwhm = AngularWidthDegrees{value};
    }
    auto valid = validateFwhmConfiguration(draft_);
    if (!valid) { status_->setText(QString::fromStdString(valid.error().message)); return; }
    if (!window_->confirmWorkflowReplacement(WorkflowStep::fwhm)) return;
    cancelRequested_ = false; setRunning(true);
    const QPointer<FwhmOptionsDialog> guard(this);
    auto admitted = window_->startConfiguredFwhm(source_, draft_, [guard](auto outcome) {
        if (guard) guard->completed(std::move(outcome));
    });
    if (!admitted) { setRunning(false); status_->setText(QString::fromStdString(admitted.error().message)); }
}
void FwhmOptionsDialog::completed(qualpf::gui::SearchTaskController::Outcome outcome) {
    setRunning(false);
    if (cancelRequested_ || !window_ || window_->isClosing()) { QDialog::reject(); return; }
    if (!outcome) { status_->setText(QString::fromStdString(outcome.error().message)); return; }
    result_ = outcome.value().fwhm;
    if (!result_ || !result_->usage) {
        if (result_) status_->setText(QString::fromStdString(configuredFwhmFailure(*result_).message));
        return;
    }
    draft_ = result_->configuration;
    setWidgets(); QDialog::accept();
}
void FwhmOptionsDialog::reject() {
    if (running_) {
        cancelRequested_ = true;
        if (window_) window_->searchTasks()->cancel();
        status_->setText(tr("Canceling FWHM estimation..."));
        return;
    }
    result_.reset(); QDialog::reject();
}
