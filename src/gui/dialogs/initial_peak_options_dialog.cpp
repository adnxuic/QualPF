#include "gui/dialogs/initial_peak_options_dialog.h"
#include "qualpf/application/workflow.h"
#include "gui/widgets/initial_peak_candidates_widget.h"
#include "gui/mainwindow.h"
#include "gui/state/preprocessing_product.h"
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleValidator>
#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <cmath>

using namespace qualpf;
using namespace qualpf::application;
InitialPeakOptionsDialog::InitialPeakOptionsDialog(MainWindow *window) : QDialog(window), window_(window) {
    setObjectName("initialPeakOptionsDialog"); setWindowTitle(tr("Initial Peak Search Options"));
    setWindowModality(Qt::WindowModal); setMinimumWidth(480);
    auto *layout = new QVBoxLayout(this);
    tabs_ = new QTabWidget(this); tabs_->setObjectName("initialPeakOptionsTabs"); layout->addWidget(tabs_);
    auto *settings = new QWidget(tabs_); settings->setObjectName("initialPeakSettingsPage");
    auto *settingsLayout = new QVBoxLayout(settings);
    tabs_->addTab(settings, tr("Settings"));
    candidates_ = new InitialPeakCandidatesWidget(tabs_);
    tabs_->addTab(candidates_, tr("Initial Peak Candidates"));
    auto *description = new QLabel(tr("Classical SG third derivative"), settings);
    description->setObjectName("initialPeakAlgorithm"); settingsLayout->addWidget(description);
    auto *algorithmSeparator = new QFrame(settings); algorithmSeparator->setObjectName("initialPeakAlgorithmSeparator");
    algorithmSeparator->setFrameShape(QFrame::HLine); algorithmSeparator->setFrameShadow(QFrame::Sunken);
    settingsLayout->addWidget(algorithmSeparator);
    auto *form = new QFormLayout;
    slope_ = new QLineEdit(this); slope_->setObjectName("initialPeakSlopeEdit");
    auto *validator = new QDoubleValidator(0, 200, 16, slope_); validator->setLocale(QLocale::c());
    validator->setNotation(QDoubleValidator::ScientificNotation); slope_->setValidator(validator);
    slope_->setToolTip(tr("Trigger threshold on the normalized third derivative. Lower values admit more candidates. Default: 0.1."));
    form->addRow(tr("Derivative threshold:"), slope_);
    automatic_ = new QCheckBox(tr("Automatic window"), this); automatic_->setObjectName("initialPeakAutomaticWindow");
    automatic_->setToolTip(tr("Use the Classical wavelength and sample-step rule.")); form->addRow(automatic_);
    halfWindow_ = new QSpinBox(this); halfWindow_->setObjectName("initialPeakHalfWindow");
    halfWindow_->setRange(2, 100); halfWindow_->setKeyboardTracking(false);
    halfWindow_->setToolTip(tr("Half-window in sample points; the full window contains 2w + 1 points. Default: 11."));
    form->addRow(tr("Manual half-window (samples):"), halfWindow_);
    degree_ = new QSpinBox(this); degree_->setObjectName("initialPeakDegree");
    degree_->setRange(3, 20); degree_->setKeyboardTracking(false);
    degree_->setToolTip(tr("Requested SG polynomial degree. The effective degree is capped at twice the half-window. Default: 9."));
    form->addRow(tr("SG polynomial degree:"), degree_); settingsLayout->addLayout(form);
    auto *help = new QLabel(tr("Searches the full working pattern using the Gaussian FWHM analysis signal. "
        "These settings also apply to later Gaussian FWHM preparation. Fixed and geometric FWHM bypass initial search."), settings);
    help->setWordWrap(true); settingsLayout->addWidget(help);
    status_ = new QLabel(settings); status_->setObjectName("initialPeakStatus"); status_->setWordWrap(true);
    status_->setTextInteractionFlags(Qt::TextSelectableByMouse); settingsLayout->addWidget(status_); settingsLayout->addStretch();
    auto *buttonSeparator = new QFrame(this); buttonSeparator->setObjectName("initialPeakButtonSeparator");
    buttonSeparator->setFrameShape(QFrame::HLine); buttonSeparator->setFrameShadow(QFrame::Sunken); layout->addWidget(buttonSeparator);
    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Close, this);
    buttons_->setObjectName("buttonBox"); layout->addWidget(buttons_);
    connect(slope_, &QLineEdit::textChanged, this, [this] { slopeEdited_ = true; });
    connect(automatic_, &QCheckBox::toggled, this, [this] { updateMode(); });
    connect(tabs_, &QTabWidget::currentChanged, this, [this] { updateMode(); });
    connect(buttons_, &QDialogButtonBox::accepted, this, &InitialPeakOptionsDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &InitialPeakOptionsDialog::reject);
    connect(buttons_->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &InitialPeakOptionsDialog::restoreDefaults);
    connect(window->searchTasks(), &qualpf::gui::SearchTaskController::drained, this, [this] {
        if (running_ && window_ && window_->isClosing()) { running_ = false; QDialog::reject(); }
    });
    resize(620, 420);
}
bool InitialPeakOptionsDialog::setOptions() {
    if (!window_ || window_->isClosing() || window_->searchTasks()->busy() ||
        !qualpf::gui::peakEditEntryStatus(window_->context())) return false;
    source_ = window_->context().workspace().session().snapshot();
    if (source_->pattern.observed.empty()) return false;
    draft_ = source_->projectSettings ? source_->projectSettings->profile(WorkflowId::preprocessing).initial_peaks : source_->preprocessing.initial_peaks.value_or(InitialPeakConfiguration{});
    setUsage(source_->editing || (source_->projectSettings && !reusableWorkflowStep(*source_, WorkflowId::preprocessing, WorkflowStep::initial_peaks)) ? std::shared_ptr<const InitialPeakUsage>{} : source_->preprocessing.initial_peak_usage);
    tabs_->setCurrentIndex(0);
    running_ = cancelRequested_ = false; setWidgets(); status_->setText(tr("OK searches and saves on success."));
    window_->saveEnabledActions(); window_->enableActions(MainWindow::DialogOpenAction);
    return true;
}
void InitialPeakOptionsDialog::setWidgets() {
    const QSignalBlocker s(slope_), a(automatic_), w(halfWindow_), d(degree_);
    slope_->setText(QString::number(draft_.options.slope, 'g', 7));
    automatic_->setChecked(draft_.options.automatic_window);
    halfWindow_->setValue(draft_.options.half_window); degree_->setValue(draft_.options.polynomial_degree);
    slopeEdited_ = false; updateMode();
}
void InitialPeakOptionsDialog::updateMode() {
    const bool settings = tabs_->currentIndex() == 0;
    tabs_->setEnabled(!running_);
    slope_->setEnabled(!running_); automatic_->setEnabled(!running_); degree_->setEnabled(!running_);
    halfWindow_->setEnabled(!running_ && !automatic_->isChecked());
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(!running_);
    buttons_->button(QDialogButtonBox::RestoreDefaults)->setEnabled(!running_);
    buttons_->button(QDialogButtonBox::Ok)->setVisible(settings);
    buttons_->button(QDialogButtonBox::RestoreDefaults)->setVisible(settings);
    buttons_->button(QDialogButtonBox::Cancel)->setVisible(settings);
    buttons_->button(QDialogButtonBox::Close)->setVisible(!settings);
}
void InitialPeakOptionsDialog::restoreDefaults() {
    const auto extensions = draft_.extensions; draft_ = {}; draft_.extensions = extensions; setWidgets();
    status_->setText(tr("Defaults restored in this draft. OK searches and saves on success."));
}
void InitialPeakOptionsDialog::accept() {
    if (running_ || !window_ || window_->isClosing() || tabs_->currentIndex() != 0) return;
    if (slopeEdited_) {
        bool ok = false; const double value = QLocale::c().toDouble(slope_->text(), &ok);
        if (!ok || !std::isfinite(value) || value < 0 || value > 200) {
            status_->setText(tr("Enter a finite derivative threshold from 0 to 200.")); return;
        }
        draft_.options.slope = float(value);
    }
    draft_.options.automatic_window = automatic_->isChecked();
    draft_.options.half_window = halfWindow_->value(); draft_.options.polynomial_degree = degree_->value();
    if (!window_->confirmWorkflowReplacement(WorkflowStep::initial_peaks)) return;
    cancelRequested_ = false; running_ = true; updateMode(); status_->setText(tr("Searching for initial candidates..."));
    const QPointer<InitialPeakOptionsDialog> guard(this);
    auto admitted = window_->startInitialPeakSearch(source_, draft_, [guard](auto outcome) {
        if (guard) guard->completed(std::move(outcome));
    });
    if (!admitted) { running_ = false; updateMode(); status_->setText(QString::fromStdString(admitted.error().message)); }
}
void InitialPeakOptionsDialog::completed(qualpf::gui::SearchTaskController::Outcome outcome) {
    running_ = false; updateMode();
    if (cancelRequested_ || !window_ || window_->isClosing()) { QDialog::reject(); return; }
    if (!outcome) {
        status_->setText(outcome.error().field == "smoothing.sg" ?
            tr("The pattern has too few samples for this window. Reduce the half-window or use automatic window.") :
            QString::fromStdString(outcome.error().message));
        return;
    }
    source_ = window_->context().workspace().session().snapshot();
    draft_ = source_->projectSettings ? source_->projectSettings->profile(WorkflowId::preprocessing).initial_peaks : source_->preprocessing.initial_peaks.value_or(InitialPeakConfiguration{});
    setWidgets();
    status_->setText(tr("Settings and candidates saved."));
    showCandidates();
}
void InitialPeakOptionsDialog::setUsage(std::shared_ptr<const InitialPeakUsage> usage) {
    candidates_->setUsage(std::move(usage));
}
void InitialPeakOptionsDialog::showCandidates() {
    tabs_->setCurrentWidget(candidates_);
}
void InitialPeakOptionsDialog::reject() {
    if (running_) {
        cancelRequested_ = true; if (window_) window_->searchTasks()->cancel();
        status_->setText(tr("Canceling initial peak search...")); return;
    }
    QDialog::reject();
}
