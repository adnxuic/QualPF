#include "gui/dialogs/rangedialog.h"
#include "ui_rangedialog.h"
#include "gui/mainwindow.h"
#include "gui/plotting/background_coordinate.h"
#include "gui/plotting/qt_plot_sink.h"
#include "gui/state/workspace_context.h"
#include <QCloseEvent>
#include <QShowEvent>
#include <QTimer>
#include <algorithm>

RangeDialog::RangeDialog(QWidget *parent)
    : QDialog(parent),ui(new Ui::RangeDialog),
      legacyOwner_(false),
      axis_(qualpf::application::presentation::Abscissa::two_theta) {
    ui->setupUi(this);
    setProperty("preprocessingBackend",legacyOwner_ ? "legacy" : "cpp");
    ui->minSpinBox->userRequest=[this](double value) { setMinimum(value); };
    ui->maxSpinBox->userRequest=[this](double value) { setMaximum(value); };
    if (legacyOwner_) {
        ui->minSpinBox->setKeyboardTracking(true);ui->maxSpinBox->setKeyboardTracking(true);
        connect(ui->minSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&RangeDialog::setMinimum);
        connect(ui->maxSpinBox,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,&RangeDialog::setMaximum);
    }
}

void RangeDialog::setRange(CustomPlotZoom *plot) {
    if (initialized_ || closed_) return;
    mPlot=plot;
    auto *window=qobject_cast<MainWindow*>(parent());
    const auto state=qualpf::gui::workspaceContext(this).searchSession().snapshot();
    if (!mPlot || !window || window->isClosing() || window->searchTasks()->busy() || state->pattern.two_theta.empty()) return;
    minValue=state->pattern.two_theta.front().value;maxValue=state->pattern.two_theta.back().value;
    cppEdit=std::make_unique<qualpf::gui::PreprocessingDialog>(*this,window);
    initialized_=true;minValueSav=minValue;maxValueSav=maxValue;
    axis_=qualpf::gui::QtPlotSink::abscissa(*window->xpdViewer());
    if (!state->pattern.radiation.lines.empty()) wave_=state->pattern.radiation.lines.front().wavelength;
    synchronizing_=true;
    if (!legacyOwner_) { ui->minSpinBox->setRange(-1e100,1e100);ui->maxSpinBox->setRange(-1e100,1e100); }
    else { ui->minSpinBox->setRange(minValue,maxValue);ui->maxSpinBox->setRange(minValue,maxValue); }
    ui->minSpinBox->initialize(minValue);ui->maxSpinBox->initialize(maxValue);
    synchronizing_=false;
    connect(mPlot,&CustomPlotZoom::leftCursorPositionChanged,this,&RangeDialog::lcursorPositionChanged);
    connect(mPlot,&CustomPlotZoom::rightCursorPositionChanged,this,&RangeDialog::rcursorPositionChanged);
    if (legacyOwner_) mPlot->setDoubleCursorPosition(minValue,maxValue);else synchronizeCursors();
    ui->buttonBox->button(QDialogButtonBox::Ok)->setDefault(false);
    ui->buttonBox->button(QDialogButtonBox::Ok)->setAutoDefault(false);
    if (!legacyOwner_) { window->saveEnabledActions();window->enableActions(MainWindow::DialogOpenAction); }
    window->saveAction();window->checkAction(MainWindow::NoZoom);
}

RangeDialog::~RangeDialog() { finish(false);delete ui; }
void RangeDialog::showEvent(QShowEvent *event) {
    QDialog::showEvent(event);
    // Qt assigns a default button during show. Text Return must finish editing
    // without clicking OK/Apply; only an explicit button click owns the request.
    for (auto *button : ui->buttonBox->buttons())
        if (auto *push=qobject_cast<QPushButton*>(button)) { push->setAutoDefault(false);push->setDefault(false); }
}
void RangeDialog::finish(bool accepted) {
    if (closed_) return;
    closed_=true;queued_=leftEdited_=rightEdited_=false;
    if (initialized_) {
        if (cppEdit) cppEdit->cancel();
        cppEdit.reset();
        if (mPlot) { disconnect(mPlot,nullptr,this,nullptr);if (!qobject_cast<MainWindow*>(parent())->isClosing()) mPlot->deleteDoubleCursor(); }
        if (auto *window=qobject_cast<MainWindow*>(parent())) window->restoreAction();
    }
}
void RangeDialog::coordinateError(const qualpf::Error &error) {
    coordinateError_=true;
    setProperty("preprocessingField",QString::fromStdString(error.field));
    setProperty("preprocessingReason",QString::fromStdString(error.reason));
    if (cppEdit) cppEdit->rejectPreview(QString::fromStdString(error.reason));
    if (auto *window=qobject_cast<MainWindow*>(parent())) window->setStatusMessage(QString::fromStdString(error.message));
}
bool RangeDialog::checkDisplayContext() {
    auto *window=qobject_cast<MainWindow*>(parent());
    if (window && window->isClosing()) return false;
    if (!mPlot || !window || qualpf::gui::QtPlotSink::abscissa(*window->xpdViewer())!=axis_) {
        coordinateError({qualpf::ErrorCode::stale_snapshot,"pattern.range.coordinate",
            "Display context changed. Close Range and open it again.","display_context_changed"});return false;
    }
    return true;
}
bool RangeDialog::synchronizeCursors() {
    using namespace qualpf::application::presentation;
    if (!checkDisplayContext()) return false;
    PlotSeries lower,upper;lower.x={minValue};upper.x={maxValue};lower.y={0};upper.y={0};
    lower.wavelength=upper.wavelength=wave_ ? wave_->value : 0;
    auto status=project_coordinates(lower,axis_);if (status) status=project_coordinates(upper,axis_);
    if (!status) { coordinateError(status.error());return false; }
    const bool lowLeft=mPlot->xAxis->coordToPixel(lower.x[0])<=mPlot->xAxis->coordToPixel(upper.x[0]);
    displayLeft_=lowLeft ? lower.x[0] : upper.x[0];displayRight_=lowLeft ? upper.x[0] : lower.x[0];
    leftCanonical_=lowLeft ? minValue : maxValue;rightCanonical_=lowLeft ? maxValue : minValue;
    synchronizing_=true;mPlot->setDoubleCursorPosition(displayLeft_,displayRight_);synchronizing_=false;
    coordinateError_=false;return true;
}
void RangeDialog::queueCursorFlush() {
    if (queued_) return;
    queued_=true;QTimer::singleShot(0,this,[this] { if (!closed_) flushCursorPair(); });
}
bool RangeDialog::flushCursorPair() {
    if (!queued_) return !coordinateError_;
    queued_=false;if (closed_ || !checkDisplayContext()) return false;
    const auto left=qualpf::gui::backgroundCanonicalCoordinate(displayLeft_,xpdutils::xAbscissaType(axis_),wave_);
    const auto right=qualpf::gui::backgroundCanonicalCoordinate(displayRight_,xpdutils::xAbscissaType(axis_),wave_);
    if (!left || !right) { coordinateError(!left ? left.error() : right.error());leftEdited_=rightEdited_=false;return false; }
    const double l=leftEdited_ ? left.value() : leftCanonical_,r=rightEdited_ ? right.value() : rightCanonical_;
    if (!(std::min(l,r)<std::max(l,r))) {
        coordinateError({qualpf::ErrorCode::invalid_input,"pattern.range","Invalid crop window","invalid_crop_window"});
        leftEdited_=rightEdited_=false;return false;
    }
    minValue=std::min(l,r);maxValue=std::max(l,r);
    ui->minSpinBox->checkedGraphRequest(minValue,l<=r ? leftEdited_ : rightEdited_);
    ui->maxSpinBox->checkedGraphRequest(maxValue,l<=r ? rightEdited_ : leftEdited_);
    leftEdited_=rightEdited_=false;rangeChanged_=true;return synchronizeCursors();
}
void RangeDialog::on_buttonBox_clicked(QAbstractButton *button) {
    if (closed_ || !initialized_) return;
    const auto which=ui->buttonBox->standardButton(button);
    if (which!=QDialogButtonBox::Apply && which!=QDialogButtonBox::Ok) return;
    if (!legacyOwner_) {
        ui->minSpinBox->commitPendingText();ui->maxSpinBox->commitPendingText();
        if (!flushCursorPair() || !checkDisplayContext()) return;
        cppEdit->request.crop_range=qualpf::AngularRange{qualpf::TwoThetaDegrees{minValue},qualpf::TwoThetaDegrees{maxValue}};
        if (!cppEdit->preview()) return;
        if (which==QDialogButtonBox::Ok && cppEdit->commit()) { finish(true);QDialog::accept(); }
        return;
    }
}
void RangeDialog::accept() { /* The clicked handler alone owns final publication. */ }
void RangeDialog::reject() { if (closed_) return;finish(false);QDialog::reject(); }
void RangeDialog::closeEvent(QCloseEvent *event) { reject();event->accept(); }
void RangeDialog::setMinimum(double value) {
    if (!initialized_ || closed_ || synchronizing_) return;
    minValue=value;rangeChanged_=true;
    if (legacyOwner_) { ui->maxSpinBox->setMinimum(value);mPlot->setDoubleCursorPosition(minValue,maxValue); }
    else synchronizeCursors();
}
void RangeDialog::setMaximum(double value) {
    if (!initialized_ || closed_ || synchronizing_) return;
    maxValue=value;rangeChanged_=true;
    if (legacyOwner_) { ui->minSpinBox->setMaximum(value);mPlot->setDoubleCursorPosition(minValue,maxValue); }
    else synchronizeCursors();
}
void RangeDialog::lcursorPositionChanged(double pos) {
    if (closed_ || synchronizing_) return;
    if (legacyOwner_) { ui->minSpinBox->setValue(pos);return; }
    displayLeft_=pos;leftEdited_=true;queueCursorFlush();
}
void RangeDialog::rcursorPositionChanged(double pos) {
    if (closed_ || synchronizing_) return;
    if (legacyOwner_) { ui->maxSpinBox->setValue(pos);return; }
    displayRight_=pos;rightEdited_=true;queueCursorFlush();
}
