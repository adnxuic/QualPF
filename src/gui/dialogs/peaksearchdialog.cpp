#include "gui/dialogs/peaksearchdialog.h"
#include "qualpf/application/workflow.h"
#include "gui/state/workspace_context.h"
#include "gui/state/preprocessing_product.h"
#include "gui/dialogs/preprocessing_dialog.h"
#include "gui/mainwindow.h"
#include "ui_peaksearchdialog.h"

#include <QPushButton>
#include <QSignalBlocker>
#include <cmath>
#include <algorithm>

PeakSearchDialog::PeakSearchDialog(MainWindow *mw) :
    QDialog(mw),
    ui(new Ui::PeakSearchDialog),
    mWindow(mw)
{
    ui->setupUi(this);

    //Disable emission of valueChanged signal when spin is edited
    ui->intensitySpinBox->setKeyboardTracking(false);
    ui->minSpinBox->setKeyboardTracking(false);
    ui->maxSpinBox->setKeyboardTracking(false);
    ui->sensitivitySpinBox->setKeyboardTracking(false);
    ui->numPeaksSpinBox->setKeyboardTracking(false);

    connect(ui->intensitySlider, &QSlider::valueChanged, ui->intensitySpinBox,
            [this](double value) { ui->intensitySpinBox->setValue(value * 0.01); });
    connect(ui->intensitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), ui->intensitySlider,
            [this](double value) { ui->intensitySlider->setValue(value * 100); });
    connect(ui->intensitySpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PeakSearchDialog::onIntensitySpinBoxValueChanged);
    connect(ui->sensitivitySlider, &QSlider::valueChanged,ui->sensitivitySpinBox,&QSpinBox::setValue);
    connect(ui->sensitivitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged),ui->sensitivitySlider,&QSlider::setValue);
    connect(ui->sensitivitySpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &PeakSearchDialog::onSensitivitySpinBoxValueChanged);
    connect(ui->appendCheckBox, &QCheckBox::clicked, this, [this](bool state) {
        pkSettings.append = state;
        if (cpp_)
            preprocessingAction(3);
    });
    connect(ui->numPeaksSpinBox, &QSpinBox::valueChanged, this, &PeakSearchDialog::onNumPeaksSpinBoxValueChanged);
    connect(ui->minSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PeakSearchDialog::onMinSpinBoxValueChanged);
    connect(ui->maxSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PeakSearchDialog::onMaxSpinBoxValueChanged);
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &PeakSearchDialog::onButtonBoxClicked);
}

void PeakSearchDialog::setOptions()
{
    if (auto *owner = qobject_cast<MainWindow *>(parent()); owner && (owner->isClosing() || owner->searchTasks()->busy())) return;
    preprocessingAction(1);

    initializing_ = true;
    setWidgets(pkSettings);
    initializing_ = false;

    pkSettingsSaved = pkSettings;

    // To prevent that enter key close the dialog. Call this after the show. See
    // also accept()
    ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setAutoDefault(false);

    mWindow->setZoomAction(); // Force zoom action and disable del/add peak action
    mWindow->saveEnabledActions();
    mWindow->enableActions(MainWindow::DialogOpenAction);
}

PeakSearchDialog::~PeakSearchDialog()
{
    delete ui;
}

void PeakSearchDialog::onNumPeaksSpinBoxValueChanged()
{
    if (ui->numPeaksSpinBox->value() != pkSettings.numPeaks) {
        pkSettings.numPeaks = ui->numPeaksSpinBox->value();
        preprocessingAction(5);
        ui->intensitySpinBox->setValue(pkSettings.threshold);
        updatePeakListTable();
    }
}

void PeakSearchDialog::setWidgets(const qualpf::gui::PeakSearchPresentationOptions &psettings)
{
    const QSignalBlocker minBlock(ui->minSpinBox), maxBlock(ui->maxSpinBox), countBlock(ui->numPeaksSpinBox),
        intensityBlock(ui->intensitySpinBox), sensitivityBlock(ui->sensitivitySpinBox),
        intensitySliderBlock(ui->intensitySlider), sensitivitySliderBlock(ui->sensitivitySlider);
    ui->minSpinBox->setRange(psettings.minRange, psettings.maxRange);
    ui->minSpinBox->setValue(psettings.minSearch);

    ui->maxSpinBox->setRange(psettings.minRange, psettings.maxRange);
    ui->maxSpinBox->setValue(psettings.maxSearch);

    ui->numPeaksSpinBox->setMaximum(std::max(psettings.numPeaksTot, psettings.numPeaks));
    ui->numPeaksSpinBox->setValue(psettings.numPeaks);

    ui->intensitySpinBox->setValue(psettings.threshold);
    ui->intensitySlider->setValue(int(psettings.threshold * 100));

    ui->sensitivitySpinBox->setValue(psettings.sensitivity);
    ui->sensitivitySlider->setValue(psettings.sensitivity);

    ui->appendCheckBox->setChecked(psettings.append);
}

void PeakSearchDialog::accept()
{
    if (auto *owner = qobject_cast<MainWindow *>(parent()); owner && owner->isClosing()) return;
    //'ok' get focus if you hide the dialog. This avoid that enter key close the dialog
    if (!ui->buttonBox->button(QDialogButtonBox::Ok)->hasFocus()) return;

    if (cpp_) {
        if (!cpp_->commit())
            return;
    } else
        qualpf::gui::workspaceContext(this).searchSession().endLegacyEdit("peaks", true);
    QDialog::accept();
}

void PeakSearchDialog::reject()
{
    if (cpp_)
        cpp_->cancel();
    else
        restoreSavedSettings();
    mWindow->restoreEnabledActions();
    QDialog::reject();
}

void PeakSearchDialog::restoreSavedSettings()
{
    pkSettings = pkSettingsSaved;
    setWidgets(pkSettings);
    preprocessingAction(0);
    updatePeakListTable();
}

void PeakSearchDialog::onButtonBoxClicked(QAbstractButton *button)
{
    QDialogButtonBox::StandardButton stdButton = ui->buttonBox->standardButton(button);

    if(stdButton == QDialogButtonBox::Ok) {
        if (!cpp_) mWindow->restoreEnabledActions();

    } else if (stdButton == QDialogButtonBox::Cancel) {
        //restoreSavedSettings();

    } else if (stdButton == QDialogButtonBox::RestoreDefaults) {
        preprocessingAction(6);
        setWidgets(pkSettings);
        updatePeakListTable();
    }
}

void PeakSearchDialog::updatePeakListTable()
{
    MainWindow *mw = qobject_cast<MainWindow *>(this->parent());
    //FIX THIS LATER
    //mw->updatePeakListTable();
}

void PeakSearchDialog::onIntensitySpinBoxValueChanged(double arg1)
{
    if (std::abs(arg1 - pkSettings.threshold) >= 0.01) {
        pkSettings.threshold = arg1;
        preprocessingAction(4);
        ui->numPeaksSpinBox->setValue(pkSettings.numPeaks);
        updatePeakListTable();
    }
}

void PeakSearchDialog::onMinSpinBoxValueChanged(double arg1)
{
    if (std::abs(arg1 - pkSettings.minSearch) > 0.01) {
        if (arg1 > ui->maxSpinBox->value()) {
            ui->minSpinBox->setValue(pkSettings.minSearch);
            return;
        }
        pkSettings.minSearch = arg1;
        preprocessingAction(3);
        ui->numPeaksSpinBox->setValue(pkSettings.numPeaks);
        updatePeakListTable();
    }
}

void PeakSearchDialog::onMaxSpinBoxValueChanged(double arg1)
{
    if (std::abs(arg1 - pkSettings.maxSearch) > 0.01) {
        if (ui->maxSpinBox->value() < ui->minSpinBox->value()) {
            ui->maxSpinBox->setValue(pkSettings.maxSearch);
            return;
        }
        pkSettings.maxSearch = arg1;
        preprocessingAction(3);
        ui->numPeaksSpinBox->setValue(pkSettings.numPeaks);
        updatePeakListTable();
    }
}

void PeakSearchDialog::onSensitivitySpinBoxValueChanged(int arg1)
{
    if (arg1 != pkSettings.sensitivity) {
        pkSettings.sensitivity = arg1;
        preprocessingAction(7);
        ui->numPeaksSpinBox->setValue(pkSettings.numPeaks);
        updatePeakListTable();
    }
}

void PeakSearchDialog::preprocessingAction(int action) {
    using namespace qualpf;
    if (action == 1) {
        cpp_.reset();
        auto state = qualpf::gui::workspaceContext(this).searchSession().snapshot();
        auto o = state->projectSettings ? state->projectSettings->profile(application::WorkflowId::peak_search).peaks :
            state->preprocessing.peaks.value_or(preprocessing::PeakOptions{});
        if (!state->projectSettings && !state->preprocessing.peaks)
            o.search_range = -1;
        pkSettings.minRange = pkSettings.minSearch = state->pattern.two_theta.front().value;
        pkSettings.maxRange = pkSettings.maxSearch = state->pattern.two_theta.back().value;
        pkSettings.threshold = o.threshold_percent;
        const int mostSensitive = (o.polynomial_degree - 1) / 2 + 1;
        pkSettings.sensitivity = int(std::round(float(o.half_window) * (-100.f / float(50 - mostSensitive)) +
                                                100.f * 50.f / float(50 - mostSensitive)));
        pkSettings.append = false;
        const auto selection = state->projectSettings ? std::optional<application::PeakSelection>{state->projectSettings->profile(application::WorkflowId::peak_search).selection} : state->preprocessing.selection;
        if (selection) {
            const auto &v = *selection;
            pkSettings.minSearch = v.minimum;
            pkSettings.maxSearch = v.maximum;
            pkSettings.threshold = v.maximum_peaks >= 0 && v.append ? o.threshold_percent : v.threshold_percent;
            // Legacy action 1 resets append when a new edit is opened.
            pkSettings.append = false;
        }
        pkSettings.numPeaks = state->peaks.tth.size();
        pkSettings.numPeaksTot = 0;
        cpp_ = std::make_unique<gui::PreprocessingDialog>(*this, mWindow);

        cpp_->request.peak_product_operation = true;
        cpp_->request.peaks = o;
        cpp_->request.selection = selection;
        if (state->projectSettings) {
            const auto &profile = state->projectSettings->profile(application::WorkflowId::peak_search);
            cpp_->request.workflow = application::WorkflowId::peak_search;
            cpp_->request.fwhm = profile.fwhm;
            cpp_->request.initial_peaks = profile.initial_peaks;
        }
    }
    if (!cpp_) return;
    if (initializing_)
        return;
    if (action == 0) {
        cpp_->cancel();
        return;
    }
    cpp_->request.restore_automatic_window = action==7;
    cpp_->request.peak_operation = application::PeakOperation::reselect;
    if (action == 1) {
        const auto source = qualpf::gui::workspaceContext(this).workspace().session().snapshot();
        // Inspect stored unverified measurements without silently redetecting them.
        // Scientific search still requires its existing provenance/restart checks.
        if (source->peaks.valid && (source->provenance.legacyUnverified || !source->projectSettings || application::reusableWorkflowStep(*source, application::WorkflowId::peak_search, application::WorkflowStep::peaks))) {
            cpp_->request.peak_operation = application::PeakOperation::restore_snapshot;
            cpp_->request.selection.reset();
            if (cpp_->preview()) pkSettings.numPeaksTot = int(cpp_->candidateCount());
            return;
        }
        cpp_->request.peak_operation = application::PeakOperation::redetect;
    }
    auto &o = *cpp_->request.peaks;
    if(action==1) o.search_range=-1; // run_peaksearch always initializes range on a new detection.
    if (action == 6) {
        cpp_->request.peak_operation = application::PeakOperation::redetect;
        pkSettings.minSearch = pkSettings.minRange;
        pkSettings.maxSearch = pkSettings.maxRange;
        pkSettings.append = false;
        o = preprocessing::PeakOptions{};
        o.search_range = -1;
        pkSettings.threshold = o.threshold_percent;
    }
    application::PeakSelection selection{pkSettings.minSearch, pkSettings.maxSearch,
                                         float(pkSettings.threshold), action == 5 ? pkSettings.numPeaks : -1,
                                         pkSettings.append};
    if (action == 1 && cpp_->request.selection)
        selection = *cpp_->request.selection;
    cpp_->request.selection = selection;
    if (action == 7) {
        cpp_->request.peak_operation = application::PeakOperation::redetect;
        o.search_range = -1;
        o.automatic_window = false;
        const int mostSensitive = (o.polynomial_degree - 1) / 2 + 1;
        o.half_window =
            int(std::round(float(pkSettings.sensitivity) * (float(mostSensitive - 50) / 100.f) + 50.f));
    }
    cpp_->request.background.reset();
    if(cpp_->request.peak_operation==application::PeakOperation::redetect &&
       !qualpf::gui::workspaceContext(this).searchSession().snapshot()->pattern.processing.background_subtracted) {
        cpp_->rejectPreview("subtraction_state_unknown");return;
    }
    if (cpp_->request.peak_operation == application::PeakOperation::redetect &&
        !qualpf::gui::workspaceContext(this).searchSession().snapshot()->pattern.background)
        {
            const auto source = qualpf::gui::workspaceContext(this).searchSession().snapshot();
            cpp_->request.background = source->projectSettings ? source->projectSettings->profile(application::WorkflowId::peak_search).background :
                source->preprocessing.background.value_or(preprocessing::BackgroundOptions{});
        }
    if (cpp_->preview()) {
        if(cpp_->state()->preprocessing.peaks) o=*cpp_->state()->preprocessing.peaks;
        pkSettings.numPeaksTot = int(cpp_->candidateCount());
        pkSettings.numPeaks = cpp_->state()->peaks.tth.size();
        if (cpp_->state()->preprocessing.selection)
            pkSettings.threshold = cpp_->state()->preprocessing.selection->threshold_percent;
    }
}
