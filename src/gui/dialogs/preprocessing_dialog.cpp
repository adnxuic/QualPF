#include "gui/dialogs/preprocessing_dialog.h"
#include "gui/support/workflow_confirmation.h"
#include "qualpf/application/workflow.h"
#include <set>
#include <algorithm>
#include "gui/state/workspace_context.h"
#include "gui/state/preprocessing_product.h"
#include "gui/mainwindow.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/plotting/qt_plot_sink.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
namespace qualpf::gui {
bool PreprocessingDialog::enabled() {
    if (qEnvironmentVariable("QUALPF_PREPROCESSING_BACKEND") == "legacy") return false;
    if (gui::cppPreprocessingProduct()) return true;
    return qEnvironmentVariable("QUALPF_CPP_PREPROCESSING_DIALOGS") == "1";
}
PreprocessingDialog::PreprocessingDialog(QDialog &dialog, MainWindow *window)
    : dialog_(dialog), window_(window) {
    auto &session = qualpf::gui::workspaceContext(&dialog_).searchSession();
    const auto state = session.snapshot();
    request.fwhm = state->projectSettings ? state->projectSettings->profile(application::WorkflowId::preprocessing).fwhm :
        state->preprocessing.fwhm.value_or(application::FwhmConfiguration{});
    request.wavelength = WavelengthAngstrom{state->pattern.radiation.lines.empty()
                                                ? state->peaks.wave
                                                : state->pattern.radiation.lines.front().wavelength.value};
    request.tolerance = AngularWidthDegrees{state->peakTolerance.value_or(.15)};

    edit_ = std::make_unique<gui::PreprocessingEdit>(session, true);
    if (window_ && qualpf::gui::workspaceContext(&dialog_).presentation().redrawEnabled())
        sink_ = std::make_unique<QtPlotSink>(window_->xpdViewer());
    dialog_.setProperty("preprocessingBackend", "cpp");
}
PreprocessingDialog::~PreprocessingDialog() {
    QObject::disconnect(detailsConnection_);
    cancel();
}
void PreprocessingDialog::clearPlot() {
    if (sink_ && (!window_ || !window_->isClosing())) {
        using namespace application::presentation;
        PlotSeries series;
        series.identity = "preprocessing.preview";
        series.kind = SeriesKind::preprocessing_preview;
        PlotUpdate update;
        update.commands.push_back({CommandKind::erase, 1, std::move(series)});
        auto applied = sink_->apply(update);
        if (!applied && window_ && applied.error().reason != "view_destroyed")
            window_->setStatusMessage(QString::fromStdString(applied.error().message));
    }
}
bool PreprocessingDialog::preview() {
    if (window_ && window_->isClosing()) return false;
    if (!edit_)
        return false;
    clearPlot();
    auto status = request.crop_range ? edit_->previewRange(request) : edit_->preview(request);
    dialog_.setProperty("preprocessingField", status ? QString() : QString::fromStdString(status.error().field));
    dialog_.setProperty("preprocessingReason", status ? QString()
                                                      : QString::fromStdString(status.error().reason.empty()
                                                                                   ? status.error().field
                                                                                   : status.error().reason));
    if (auto *buttons = dialog_.findChild<QDialogButtonBox *>())
        buttons->button(QDialogButtonBox::Ok)->setEnabled(bool(status));
    auto *diagnostic = dialog_.findChild<QLabel *>("preprocessingDiagnostic");
    if (!diagnostic && dialog_.layout()) {
        diagnostic = new QLabel(&dialog_);
        diagnostic->setObjectName("preprocessingDiagnostic");
        diagnostic->setWordWrap(true);
        dialog_.layout()->addWidget(diagnostic);
    }
    if (diagnostic) {
        const auto summary = status ? peakRejectionSummary(edit_->preview()->preprocessing) : QString();
        diagnostic->setText(status ? summary : QString::fromStdString(status.error().reason + ": " + status.error().message) +
            QObject::tr("\nNothing was applied. Adjust the parameters, or Cancel and use "
                        "Preprocessing > Restart preprocessing... / Peak Search > Add Peak."));
        diagnostic->setToolTip(status ? peakRejectionDetails(edit_->preview()->preprocessing) : QString());
        diagnostic->setVisible(!status || !summary.isEmpty());
        auto *details = dialog_.findChild<QPushButton *>("peakRejectionDetails");
        if (!details) {
            details = new QPushButton(QObject::tr("Candidate details..."), &dialog_);
            details->setObjectName("peakRejectionDetails");
            dialog_.layout()->addWidget(details);
        }
        if (!detailsConnection_)
            detailsConnection_ = QObject::connect(details, &QPushButton::clicked, &dialog_, [this] {
                if (!edit_ || !edit_->preview()) return;
                const auto &configuration = edit_->preview()->preprocessing;
                QMessageBox message(QMessageBox::Information, QObject::tr("Rejected peak candidates"),
                    peakRejectionSummary(configuration), QMessageBox::Ok, &dialog_);
                message.setInformativeText(QObject::tr("Indices start at zero. Valid peaks can be applied."));
                message.setDetailedText(peakRejectionDetails(configuration));
                message.exec();
            });
        details->setVisible(status && !summary.isEmpty());
    }
    if (!status) {
        if (window_)
            window_->setStatusMessage(QString::fromStdString(status.error().message));
        return false;
    }
    if (sink_) {
        using namespace application::presentation;
        const auto &state = *edit_->preview();
        QVector<double> x, y;
        if (request.peaks || request.manual || request.table || request.inferred_graph || request.imported) {
            x = state.peaks.tth;
            y = state.peaks.intensity;
        } else {
            const bool background = (show_background && state.pattern.background && state.pattern.background->window) || request.background || !request.background_filters.empty();
            const auto &values = background ? state.pattern.background->values : state.pattern.observed;
            const auto first = background ? state.pattern.background->window->first : 0;
            for (std::size_t i = 0; i < values.size(); ++i)
                x.append(state.pattern.two_theta[first + i].value);
            for (auto v : values)
                y.append(v);
        }
        PlotSeries series;
        series.x.assign(x.begin(), x.end());
        series.y.assign(y.begin(), y.end());
        series.wavelength = request.wavelength.value;
        auto projected = project_preview(std::move(series), QtPlotSink::abscissa(*window_->xpdViewer()),
            request.peaks || request.manual || request.table || request.inferred_graph || request.imported ? PreviewGeometry::peak_impulses : PreviewGeometry::curve);
        if (!projected) { rejectPreview(QString::fromStdString(projected.error().reason)); return false; }
        PlotUpdate update;
        update.snapshot.abscissa = QtPlotSink::abscissa(*window_->xpdViewer());
        update.snapshot.overlays.push_back(projected.value());
        update.commands.push_back({CommandKind::upsert, 1, std::move(projected).value()});
        auto applied = sink_->apply(update);
        if (!applied) { rejectPreview(QString::fromStdString(applied.error().reason)); return false; }
    }
    return true;
}
void PreprocessingDialog::rejectPreview(const QString &reason) {
    application::PreprocessingRequest empty;
    const auto invalidated = edit_->preview(empty);
    (void)invalidated;
    clearPlot();
    dialog_.setProperty("preprocessingReason", reason);
    if (auto *diagnostic = dialog_.findChild<QLabel *>("preprocessingDiagnostic")) {
        diagnostic->setText(reason);
        diagnostic->setToolTip({});
        diagnostic->show();
    }
    if (auto *details = dialog_.findChild<QPushButton *>("peakRejectionDetails")) details->hide();
    if (auto *buttons = dialog_.findChild<QDialogButtonBox *>())
        buttons->button(QDialogButtonBox::Ok)->setEnabled(false);
    if (window_)
        window_->setStatusMessage(reason);
}
bool PreprocessingDialog::commit() {
    if (window_ && window_->isClosing()) return false;
    if (!edit_)
        return false;
    const auto source=qualpf::gui::workspaceContext(&dialog_).workspace().session().snapshot();
    if(source->projectSettings && edit_->preview()) {
        std::set<std::uint64_t> affected;
        const auto step=request.crop_range?application::WorkflowStep::range:
            request.smoothing?application::WorkflowStep::smoothing:
            request.peaks?application::WorkflowStep::peaks:application::WorkflowStep::background;
        if(const auto *record=application::currentWorkflowRecord(source->workflow,step)) {
            const auto closure=application::workflowDependents(source->workflow,record->id);
            if(closure.size()>1)affected.insert(closure.begin(),closure.end());
        }
        const auto &preview=edit_->preview()->canonical->workflow;
        for(const auto &record:source->workflow.records) {
            const auto found=std::find_if(preview.records.begin(),preview.records.end(),[&](const auto &r){return r.id==record.id;});
            if(record.status!=application::WorkflowStepStatus::invalidated && found!=preview.records.end() &&
                found->status==application::WorkflowStepStatus::invalidated)affected.insert(record.id);
        }
        if(!confirmWorkflowImpact(&dialog_,source,{affected.begin(),affected.end()},QObject::tr("Apply the edited step?")))return false;
    }
    // Remove the private overlay before Session::changed creates/updates plots.
    clearPlot();
    auto status = edit_->commit();
    if (!status) {
        dialog_.setProperty("preprocessingReason", QString::fromStdString(status.error().field));
        if (window_)
            window_->setStatusMessage(QString::fromStdString(status.error().message));
        return false;
    }
    clearPlot();
    edit_.reset();
    if (window_) window_->refreshPatternPresentation();
    if (window_) window_->refreshPreprocessingDisplay();
    if (window_)
        window_->restoreEnabledActions();
    return true;
}
void PreprocessingDialog::cancel() {
    if (!edit_)
        return;
    clearPlot();
    if (edit_) {
        edit_->cancel();
        edit_.reset();
    }
    if (window_ && !window_->isClosing())
        window_->restoreEnabledActions();
    if (window_ && !window_->isClosing()) {
        window_->refreshPatternPresentation();
        window_->refreshPreprocessingDisplay();
    }
}
} // namespace qualpf::gui
