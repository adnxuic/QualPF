#include "gui/dialogs/backgrounddialog.h"
#include "qualpf/application/workflow.h"
#include "gui/state/workspace_context.h"
#include "gui/state/preprocessing_product.h"
#include "gui/dialogs/preprocessing_dialog.h"
#include "gui/mainwindow.h"
#include "ui_backgrounddialog.h"

#include <QPushButton>
#include <QLabel>
#include <QLayout>
#include <QSignalBlocker>



BackgroundDialog::BackgroundDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::BackgroundDialog)
{
    ui->setupUi(this);

    ui->chebyshevWidget->setMaximumCoef(36);
    ui->polynomialWidget->setMaximumCoef(12);
    ui->fourierWidget->setMaximumCoef(12);

    bkSettings[Chebyshev] = {Chebyshev,true,0,0,0,0,0};
    bkSettings[Polynomial] = {Polynomial,true,0,0,0,0,0};
    bkSettings[Cosine_Fourier] = {Cosine_Fourier,true,0,0,0,0,0};
    bkSettings[Cubic_Spline] = {Cubic_Spline,false,0,0,0,0,0};
    bkSettings[Bezier_Spline] = {Bezier_Spline,false,0,0,0,0,0};
    bkSettings[Filter] = {Filter,false,6,5,50,0,0};
    bkSettings[None] = {None,false,0,0,0,0,0};

    connect(ui->chebyshevWidget, &BackgroundCoefWidget::autoCoefChanged, this, &BackgroundDialog::applyAutoChanged);
    connect(ui->chebyshevWidget, &BackgroundCoefWidget::ncoefChanged, this, &BackgroundDialog::applyNcoefChanged);
    connect(ui->polynomialWidget, &BackgroundCoefWidget::autoCoefChanged, this, &BackgroundDialog::applyAutoChanged);
    connect(ui->polynomialWidget, &BackgroundCoefWidget::ncoefChanged, this, &BackgroundDialog::applyNcoefChanged);
    connect(ui->fourierWidget, &BackgroundCoefWidget::autoCoefChanged, this, &BackgroundDialog::applyAutoChanged);
    connect(ui->fourierWidget, &BackgroundCoefWidget::ncoefChanged, this, &BackgroundDialog::applyNcoefChanged);
    connect(ui->resetFilterButton, &QPushButton::clicked, this, &BackgroundDialog::onResetFilterButtonClicked);
    connect(ui->applyFilterButton, &QPushButton::clicked, this, &BackgroundDialog::onApplyFilterButtonClicked);
    connect(ui->typeComboBox, &QComboBox::currentIndexChanged, this, &BackgroundDialog::onTypeComboBoxCurrentIndexChanged);
    connect(ui->buttonBox, &QDialogButtonBox::clicked, this, &BackgroundDialog::onButtonBoxClicked);
}

BackgroundDialog::~BackgroundDialog()
{
    delete ui;
}

void BackgroundDialog::setBackground()
{
    if (auto *owner = qobject_cast<MainWindow *>(parent()); owner && (owner->isClosing() || owner->searchTasks()->busy())) return;
    if (auto *owner = qobject_cast<MainWindow *>(parent())) owner->setBackgroundSubtractionDisplay(false);
    preprocessingAction(0);
    initializing_ = true;
    setWidgets(bkCurrentSettings);
    initializing_ = false;
    ui->minSpinBox->setRange(bkCurrentSettings.minf, bkCurrentSettings.maxf);
    ui->maxSpinBox->setRange(bkCurrentSettings.minf, bkCurrentSettings.maxf);
    bkSavedSettings = bkCurrentSettings;
    bkSettings[Filter].minf = bkCurrentSettings.minf;
    bkSettings[Filter].maxf = bkCurrentSettings.maxf;
    oldIndex = bkCurrentSettings.btype;

    // Set menu only if parent is a MainWindow
    MainWindow *mw = qobject_cast<MainWindow *>(this->parent());
    if (mw) {
        mw->saveEnabledActions();
        mw->enableActions(MainWindow::DialogOpenAction);
    }
}

void BackgroundDialog::onTypeComboBoxCurrentIndexChanged(int index)
{
    if (index == Cubic_Spline || index == Bezier_Spline || index == None) {
        enableBkWidget(false, oldIndex);
    } else {
        enableBkWidget(true, index);
        ui->stackedWidget->setCurrentIndex(index);
        oldIndex = index;
    }
    bkSettings[bkCurrentSettings.btype] = bkCurrentSettings;
    bkCurrentSettings = bkSettings[index];
    preprocessingAction(1);
    setPage(bkCurrentSettings);
}

void BackgroundDialog::setPage(const qualpf::gui::BackgroundPresentationOptions &bSettings)
{
    const QSignalBlocker chebyshev(ui->chebyshevWidget);
    const QSignalBlocker polynomial(ui->polynomialWidget);
    const QSignalBlocker fourier(ui->fourierWidget);
    if (bSettings.btype == Chebyshev) {
        if (bSettings.autob)
            ui->chebyshevWidget->setAutob(true);
        else
            ui->chebyshevWidget->setAutob(false);
        ui->chebyshevWidget->setNcoef(bSettings.ncoef);
    }

    if (bSettings.btype == Polynomial) {
        if (bSettings.autob)
            ui->polynomialWidget->setAutob(true);
        else
            ui->polynomialWidget->setAutob(false);
        ui->polynomialWidget->setNcoef(bSettings.ncoef);
    }

    if (bSettings.btype == Cosine_Fourier) {
        if (bSettings.autob)
            ui->fourierWidget->setAutob(true);
        else
            ui->fourierWidget->setAutob(false);
        ui->fourierWidget->setNcoef(bSettings.ncoef);
    }

    if (bSettings.btype == Filter) {
        ui->iterationsSpinBox->setValue(bSettings.niterf);
        ui->windowSizeSpinBox->setValue(bSettings.nwinf);
        ui->minSpinBox->setValue(bSettings.minf);
        ui->maxSpinBox->setValue(bSettings.maxf);
    }
}

void BackgroundDialog::enableBkWidget(bool enabled, int index)
{
    if (index == Chebyshev) ui->chebyshevWidget->setEnabled(enabled);
    if (index == Polynomial) ui->polynomialWidget->setEnabled(enabled);
    if (index == Cosine_Fourier) ui->fourierWidget->setEnabled(enabled);
}

void BackgroundDialog::setWidgets(const qualpf::gui::BackgroundPresentationOptions &bSettings)
{
    const QSignalBlocker selection(ui->typeComboBox);
    ui->typeComboBox->setCurrentIndex(bSettings.btype);
    const bool fitted = bSettings.btype <= Cosine_Fourier;
    enableBkWidget(fitted, oldIndex);
    if (fitted || bSettings.btype == Filter) {
        ui->stackedWidget->setCurrentIndex(bSettings.btype);
        oldIndex = bSettings.btype;
    }
    setPage(bSettings);
}

void BackgroundDialog::applyAutoChanged()
{
    BackgroundCoefWidget *widget = qobject_cast<BackgroundCoefWidget*>(sender());
    bkCurrentSettings.autob = widget->getAutob();
    if (bkCurrentSettings.autob) {
        preprocessingAction(1);
        widget->setNcoef(bkCurrentSettings.ncoef);
    }
}

void BackgroundDialog::applyNcoefChanged()
{
    if (bkCurrentSettings.autob) return;

    BackgroundCoefWidget *widget = qobject_cast<BackgroundCoefWidget*>(sender());
    bkCurrentSettings.ncoef = widget->getNcoef();
    preprocessingAction(1);
}

void BackgroundDialog::accept()
{
    if (auto *owner = qobject_cast<MainWindow *>(parent()); owner && owner->isClosing()) return;
    //'ok' get focus if you hide the dialog. This avoid that enter key close the dialog
    if (!ui->buttonBox->button(QDialogButtonBox::Ok)->hasFocus()) return;

    if (cpp_) {
        if (!cpp_->commit())
            return;
    } else if (explicitLegacy_) {
        if (legacySource_ != qualpf::gui::workspaceContext(this).searchSession().snapshot()) {
            setProperty("preprocessingReason", "preprocessing");
            setProperty("preprocessingField", "preprocessing");
            return;
        }
        qualpf::gui::workspaceContext(this).searchSession().endLegacyEdit("background", true);
        legacySource_.reset();
    } else
        return;
    QDialog::accept();
}

void BackgroundDialog::reject()
{
    if (cpp_)
        cpp_->cancel();
    else if (explicitLegacy_ && legacySource_ == qualpf::gui::workspaceContext(this).searchSession().snapshot())
        restoreSavedSettings();
    MainWindow *mw = qobject_cast<MainWindow *>(this->parent());
    if (mw)
        mw->restoreEnabledActions();

    if (explicitLegacy_) {
        qualpf::gui::workspaceContext(this).searchSession().endLegacyEdit("background", false);
        legacySource_.reset();
        if (mw) {
            mw->refreshPatternPresentation();
            mw->refreshPreprocessingDisplay();
        }
    }
    QDialog::reject();
}

void BackgroundDialog::restoreSavedSettings()
{
    bkCurrentSettings = bkSavedSettings;
    setWidgets(bkCurrentSettings);
    preprocessingAction(1);
}

void BackgroundDialog::onButtonBoxClicked(QAbstractButton *button)
{
    QDialogButtonBox::StandardButton stdButton = ui->buttonBox->standardButton(button);

    if(stdButton == QDialogButtonBox::Ok) {
        MainWindow *mw = qobject_cast<MainWindow *>(this->parent());
        if (mw && !cpp_)
            mw->restoreEnabledActions();

    } else if (stdButton == QDialogButtonBox::Cancel) {
        //restoreSavedSettings();

    } else if (stdButton == QDialogButtonBox::RestoreDefaults) {
        bkCurrentSettings = {Chebyshev, true, 0, 0 ,0, 0, 0};
        preprocessingAction(1);
        setWidgets(bkCurrentSettings);
    }
}

void BackgroundDialog::onApplyFilterButtonClicked()
{
    bkCurrentSettings.niterf = ui->iterationsSpinBox->value();
    bkCurrentSettings.nwinf = ui->windowSizeSpinBox->value();
    if (ui->minSpinBox->value() < ui->maxSpinBox->value()) {
        bkCurrentSettings.minf = ui->minSpinBox->value();
        bkCurrentSettings.maxf = ui->maxSpinBox->value();
    } else {
        ui->minSpinBox->setValue(bkCurrentSettings.minf);
        ui->maxSpinBox->setValue(bkCurrentSettings.maxf);
    }
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    preprocessingAction(3);
    QApplication::restoreOverrideCursor();
}

void BackgroundDialog::onResetFilterButtonClicked()
{
    QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
    preprocessingAction(1);
    QApplication::restoreOverrideCursor();
}

void BackgroundDialog::preprocessingAction(int action) {
    using namespace qualpf;
    if (action == 0) {
        cpp_.reset();
        legacySource_.reset();
        explicitLegacy_ = false;
        setProperty("backgroundOwnership", "cpp_transaction");
        auto state = qualpf::gui::workspaceContext(this).searchSession().snapshot();
        auto o = state->projectSettings ? state->projectSettings->profile(application::WorkflowId::preprocessing).background :
            state->preprocessing.background.value_or(preprocessing::BackgroundOptions{});
        bkCurrentSettings = {int(o.method),
                             o.automatic,
                             o.coefficients,
                             o.iterations,
                             o.half_window,
                             state->pattern.two_theta.front().value,
                             state->pattern.two_theta.back().value};
        oldIndex = bkCurrentSettings.btype;
        cpp_ = std::make_unique<gui::PreprocessingDialog>(*this, qobject_cast<MainWindow *>(parent()));
        cpp_->show_background = true;

        // T3 permits an identity filter request and records it in the history.
        ui->iterationsSpinBox->setMinimum(0);
        const auto &background = state->pattern.background;
        if (application::hasActiveBackground(state->pattern) && background->window &&
            (!state->projectSettings || application::reusableWorkflowStep(*state->canonical, application::WorkflowId::preprocessing, application::WorkflowStep::background))) {
            if (o.bruckner_range) {
                bkCurrentSettings.minf = o.bruckner_range->minimum.value;
                bkCurrentSettings.maxf = o.bruckner_range->maximum.value;
            }
            const bool partial = background->window->first != 0 ||
                background->values.size() != state->pattern.observed.size();
            if (partial) {
                bkCurrentSettings.minf = state->pattern.two_theta[background->window->first].value;
                bkCurrentSettings.maxf = state->pattern.two_theta[background->window->first+background->values.size()-1].value;
            }
            auto *notice = findChild<QLabel *>("partialBackgroundNotice");
            if (!notice) { notice = new QLabel(this); notice->setObjectName("partialBackgroundNotice"); notice->setWordWrap(true); layout()->addWidget(notice); }
            notice->setText(tr("Restoring the saved background and filter history. Filter operates on this curve. Reset estimates the full current spectrum. Use Range for a local estimate."));
            notice->setVisible(partial);
            cpp_->request.peak_operation = application::PeakOperation::restore_snapshot;
            cpp_->preview();
            return;
        }
        if (auto *notice = findChild<QLabel *>("partialBackgroundNotice")) notice->hide();
    }
    if (!cpp_) {
        setProperty("preprocessingReason", "preprocessing");
        setProperty("preprocessingField", "preprocessing");
        ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        return;
    }
    if (initializing_)
        return;
    cpp_->request.peak_operation = application::PeakOperation::redetect;
    if (action == 3) {
        cpp_->request.background_filters.push_back({bkCurrentSettings.minf, bkCurrentSettings.maxf,
            bkCurrentSettings.niterf, bkCurrentSettings.nwinf});
        cpp_->preview();
        return;
    }
    // Fresh full estimates always receive exact canonical endpoints. Display
    // spinboxes and a saved view/filter interval never define a fresh Reset.
    {
        const auto &pattern = qualpf::gui::workspaceContext(this).searchSession().snapshot()->pattern;
        bkCurrentSettings.minf = pattern.two_theta.front().value;
        bkCurrentSettings.maxf = pattern.two_theta.back().value;
    }
    cpp_->request.background_points.reset();
    cpp_->request.background_filters.clear();
    auto &o = cpp_->request.background.emplace();
    o.method = preprocessing::BackgroundMethod(bkCurrentSettings.btype);
    o.automatic = bkCurrentSettings.autob;
    o.coefficients = bkCurrentSettings.ncoef > 0 ? bkCurrentSettings.ncoef : 6;
    o.iterations = bkCurrentSettings.niterf;
    o.half_window = bkCurrentSettings.nwinf > 0 ? bkCurrentSettings.nwinf : 50;
    if(o.method==preprocessing::BackgroundMethod::bruckner)
        o.bruckner_range=AngularRange{TwoThetaDegrees{bkCurrentSettings.minf},TwoThetaDegrees{bkCurrentSettings.maxf}};
    if(cpp_->preview() && cpp_->state()->preprocessing.background)
        bkCurrentSettings.ncoef=cpp_->state()->preprocessing.background->coefficients;
}
