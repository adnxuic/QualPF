#include "gui/mainwindow.h"
#include "gui/dialogs/fwhm_options_dialog.h"
#include "gui/state/preprocessing_product.h"
#include "qualpf/application/configured_fwhm.h"
#include "ui_mainwindow.h"
#include <QMessageBox>

using namespace qualpf;
using namespace qualpf::application;
Status MainWindow::startConfiguredFwhm(ProductHandle source, FwhmConfiguration configuration,
    qualpf::gui::SearchTaskController::Completion completion) {
    if (closingRequested_ || searchTasks_->busy() || !source || source->editing ||
        source != context().workspace().session().snapshot())
        return Status{Error{ErrorCode::stale_snapshot, "fwhm.source", "Finish the active operation and reopen FWHM options"}};
    auto valid = validateFwhmConfiguration(configuration);
    if (!valid) return valid;
    if (source->pattern.observed.empty())
        return Status{Error{ErrorCode::missing_data, "fwhm.pattern", "A continuous pattern is required"}};
    setProperty("fwhmErrorReason", QString());
    setStatusMessage(configuration.use_fixed ? tr("Applying fixed FWHM...") : tr("Estimating FWHM..."));
    return searchTasks_->submit(context().referenceDatabase(),
        [source, request = ConfiguredFwhmRequest{source->pattern, std::move(configuration), {}, true}](const SearchTask &task, SearchProgress) {
            using Outcome = qualpf::gui::SearchTaskController::Outcome;
            if (task.source != source) return Outcome{Error{ErrorCode::stale_snapshot, "fwhm.source", "FWHM input changed"}};
            auto measured = evaluateConfiguredFwhm(source, request, task.context);
            if (!measured) return Outcome{measured.error()};
            PreparedSearchResult prepared;
            prepared.source = source; prepared.taskId = task.id; prepared.databaseVersion = task.database.selectionVersion;
            prepared.next = *source; prepared.changed = false;
            prepared.fwhm = std::make_shared<const ConfiguredFwhmEvaluation>(std::move(measured).value());
            if (source->projectSettings && !prepared.fwhm->usage)
                return Outcome{configuredFwhmFailure(*prepared.fwhm)};
            if (prepared.fwhm->usage) {
                auto applied = applyConfiguredFwhm(prepared.next, *prepared.fwhm);
                if (!applied) return Outcome{applied.error()};
                if (prepared.next.projectSettings) prepared.next.projectSettings->profile(WorkflowId::preprocessing).fwhm = prepared.fwhm->configuration;
                prepared.changed = true;
            }
            return Outcome{std::move(prepared)};
        },
        [this, completion = std::move(completion)](auto outcome) mutable {
            if (closingRequested_) return;
            if (!outcome) {
                setProperty("fwhmErrorReason", QString::fromStdString(outcome.error().reason));
                setStatusMessage(QString::fromStdString(outcome.error().message));
            } else if (outcome.value().fwhm && outcome.value().fwhm->usage) {
                markProjectModified();
                setStatusMessage(tr("Default FWHM: %1 degrees 2 theta").arg(outcome.value().fwhm->usage->width.value, 0, 'g', 12));
            } else if (outcome.value().fwhm) {
                const auto error = configuredFwhmFailure(*outcome.value().fwhm);
                setProperty("fwhmErrorReason", QString::fromStdString(error.reason));
                setStatusMessage(QString::fromStdString(error.message));
            }
            updateFwhmActions();
            if (completion) completion(std::move(outcome));
        }, {}, SearchPublication::commit, {}, WorkflowTarget{WorkflowId::preprocessing,WorkflowStep::fwhm});
}
void MainWindow::onActionFwhmOptionsTriggered() {
    if (closingRequested_ || searchTasks_->busy() || !ui->actionFwhmEstimationOptions->isEnabled()) return;
    if (fwhmDialog_->isVisible()) { fwhmDialog_->raise(); return; }
    if (fwhmDialog_->setOptions()) fwhmDialog_->show();
}
void MainWindow::onActionFwhmEstimationTriggered() {
    if (closingRequested_ || searchTasks_->busy() || !ui->actionFwhmEstimation->isEnabled() || fwhmDialog_->isVisible()) return;
    const auto source = context().workspace().session().snapshot();
    const auto configuration = source->projectSettings ? source->projectSettings->profile(WorkflowId::preprocessing).fwhm :
        source->preprocessing.fwhm.value_or(FwhmConfiguration{});
    if (configuration.use_fixed) return;
    if (!confirmWorkflowReplacement(WorkflowStep::fwhm)) return;
    auto status = startConfiguredFwhm(source, configuration, [this](auto outcome) {
        if (!outcome && outcome.error().code != ErrorCode::cancelled && outcome.error().code != ErrorCode::stale_snapshot)
            QMessageBox::warning(this, tr("FWHM estimation failed"), QString::fromStdString(outcome.error().message));
        else if (outcome && outcome.value().fwhm && !outcome.value().fwhm->usage)
            QMessageBox::warning(this, tr("FWHM estimation failed"), QString::fromStdString(configuredFwhmFailure(*outcome.value().fwhm).message));
    });
    if (!status) setStatusMessage(QString::fromStdString(status.error().message));
}
void MainWindow::updateFwhmActions() {
    if (closingRequested_) return;
    const auto source = context().workspace().session().snapshot();
    const bool available = ui->actionFwhmEstimationOptions->isEnabled() && !searchTasks_->busy() && !source->editing &&
        !source->pattern.observed.empty() && !(fwhmDialog_ && fwhmDialog_->isVisible());
    const auto configuration = source->projectSettings ? source->projectSettings->profile(WorkflowId::preprocessing).fwhm :
        source->preprocessing.fwhm.value_or(FwhmConfiguration{});
    ui->actionFwhmEstimation->setEnabled(available && !configuration.use_fixed);
}
