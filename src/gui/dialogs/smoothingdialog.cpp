#include "gui/dialogs/smoothingdialog.h"
#include "gui/state/workspace_context.h"
#include "gui/dialogs/preprocessing_dialog.h"
#include "gui/mainwindow.h"
#include "ui_smoothingdialog.h"
#include <QSignalBlocker>
#include <algorithm>

SmoothingDialog::SmoothingDialog(MainWindow *mw) :
    QDialog(mw),
    ui(new Ui::SmoothingDialog),
    mWindow(mw)
{
    ui->setupUi(this);

    ui->pointsSpinBox->setKeyboardTracking(false);
    ui->polynomialSpinBox->setKeyboardTracking(false);
    connect(ui->pointsSlider,&QSlider::valueChanged,ui->pointsSpinBox,&QSpinBox::setValue);
    connect(ui->pointsSpinBox,QOverload<int>::of(&QSpinBox::valueChanged),ui->pointsSlider,&QSlider::setValue);
    connect(ui->polynomialSlider,&QSlider::valueChanged,ui->polynomialSpinBox,&QSpinBox::setValue);
    connect(ui->polynomialSpinBox,QOverload<int>::of(&QSpinBox::valueChanged),ui->polynomialSlider,&QSlider::setValue);
}

SmoothingDialog::~SmoothingDialog()
{
    delete ui;
}

void SmoothingDialog::setSmoothing()
{
    if (auto *owner = qobject_cast<MainWindow *>(parent()); owner && (owner->isClosing() || owner->searchTasks()->busy())) return;
    preprocessingAction(0);
    initializing_ = true;
    sOptionsSaved = sOptions;
    ui->methodComboBox->setCurrentIndex(sOptions.method);
    ui->pointsSpinBox->setValue(getPoints());
    ui->polynomialSpinBox->setValue(sOptions.pol_order);

    initializing_ = false;
    mWindow->saveEnabledActions();
    mWindow->enableActions(MainWindow::DialogOpenAction);
}

void SmoothingDialog::on_methodComboBox_currentIndexChanged(int index)
{    
    ui->polynomialLabel->setEnabled(index == SavGol);
    ui->polynomialSlider->setEnabled(index == SavGol);
    ui->polynomialSpinBox->setEnabled(index == SavGol);
    if (cpp_ && !initializing_ && sOptions.method != index &&
        (index == SavGol || index == Averaging)) {
        const int rememberedPoints = index == SavGol ? sOptions.npoints_sg : sOptions.npoints_ave;
        if (rememberedPoints >= 1 && rememberedPoints <= ui->pointsSpinBox->maximum()) {
            // Changing a widget minimum can emit valueChanged synchronously. Select
            // the new memory first and synchronize without touching the inactive one.
            {
                const QSignalBlocker spinBlocker(ui->pointsSpinBox);
                const QSignalBlocker sliderBlocker(ui->pointsSlider);
                sOptions.method = index;
                const int minimum = index == SavGol ? 2 : 1;
                const int points = std::max(rememberedPoints, minimum);
                ui->pointsSlider->setMinimum(minimum);
                ui->pointsSpinBox->setMinimum(minimum);
                setPoints(points);
                ui->pointsSpinBox->setValue(points);
                ui->pointsSlider->setValue(points);
            }
            preprocessingAction(1);
            return;
        }
    }
    switch (index) {
    case SavGol:
        ui->pointsSlider->setMinimum(2);
        ui->pointsSpinBox->setMinimum(2);
        break;
    case Averaging:
        ui->pointsSlider->setMinimum(1);
        ui->pointsSpinBox->setMinimum(1);
        break;
    }
    if (sOptions.method != index) {
        sOptions.method = index;
        ui->pointsSpinBox->setValue(getPoints());
        preprocessingAction(1);
    }
}

int SmoothingDialog::getPoints() const
{
    switch (sOptions.method) {
    case SavGol: return sOptions.npoints_sg;
    case Averaging: return sOptions.npoints_ave;
    default: return 10;
    }
}

void SmoothingDialog::setPoints(int npoints)
{
    switch (sOptions.method) {
    case SavGol:
        sOptions.npoints_sg = npoints;
        break;
    case Averaging:
        sOptions.npoints_ave = npoints;
        break;
    }
}

void SmoothingDialog::on_pointsSpinBox_valueChanged(int arg1)
{
    if (getPoints() != arg1) {
        setPoints(arg1);
        if (cpp_ || 2 * arg1 >= sOptions.pol_order)
            preprocessingAction(1);
    }
}

void SmoothingDialog::on_polynomialSpinBox_valueChanged(int arg1)
{
    if (sOptions.pol_order != arg1) {
        sOptions.pol_order = arg1;
        if (cpp_ || 2 * arg1 >= sOptions.pol_order)
            preprocessingAction(2);
    }
}

void SmoothingDialog::accept()
{
    if (auto *owner = qobject_cast<MainWindow *>(parent()); owner && owner->isClosing()) return;
    //'ok' get focus if you hide the dialog. This avoid that enter key close the dialog
    if (!ui->buttonBox->button(QDialogButtonBox::Ok)->hasFocus()) return;

    if (cpp_ && !cpp_->commit())
        return;
    QDialog::accept();
}

void SmoothingDialog::restoreOptions()
{
    if (mWindow->isClosing()) { if (cpp_) cpp_->cancel(); return; }
    sOptions = sOptionsSaved;
    preprocessingAction(3);
}

void SmoothingDialog::reject()
{
    restoreOptions();
    mWindow->restoreEnabledActions();
    QDialog::reject();
}

void SmoothingDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    QDialogButtonBox::StandardButton stdButton = ui->buttonBox->standardButton(button);

    if(stdButton == QDialogButtonBox::Ok) {
        if (!cpp_) {
            preprocessingAction(4);
            mWindow->restoreEnabledActions();
        }
    } else if (stdButton == QDialogButtonBox::Cancel) {
        restoreOptions();
    }
}

void SmoothingDialog::preprocessingAction(int action) {
    using namespace qualpf;
    if (action == 0) {
        cpp_.reset();
        const auto state = qualpf::gui::workspaceContext(this).searchSession().snapshot();
        auto o = state->projectSettings ? state->projectSettings->profile(application::WorkflowId::preprocessing).smoothing :
            state->preprocessing.smoothing.value_or(preprocessing::SmoothingOptions{});
        sOptions.method = int(o.method);
        sOptions.npoints_sg = o.half_window;
        sOptions.npoints_ave = o.half_window;
        sOptions.pol_order = o.polynomial_degree;
        cpp_ = std::make_unique<gui::PreprocessingDialog>(*this, mWindow);
    }
    if (!cpp_)
        return;
    if (initializing_)
        return;
    if (action == 3) {
        cpp_->cancel();
        return;
    }
    cpp_->request.smoothing = preprocessing::SmoothingOptions{preprocessing::SmoothingMethod(sOptions.method),
                                                              getPoints(), sOptions.pol_order, 0};
    cpp_->preview();
}
