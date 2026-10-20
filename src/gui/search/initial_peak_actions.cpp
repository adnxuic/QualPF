#include "gui/mainwindow.h"
#include "gui/dialogs/initial_peak_options_dialog.h"
#include "qualpf/application/initial_peak_search.h"
#include "ui_mainwindow.h"
#include "gui/widgets/initial_peak_candidates_widget.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QMessageBox>

using namespace qualpf;
using namespace qualpf::application;
Status MainWindow::startInitialPeakSearch(ProductHandle source, InitialPeakConfiguration configuration,
    qualpf::gui::SearchTaskController::Completion completion) {
    if (closingRequested_ || searchTasks_->busy() || !source || source->editing ||
        source != context().workspace().session().snapshot())
        return Status{Error{ErrorCode::stale_snapshot, "peaks.initial.source", "Finish the active operation and reopen initial peak options"}};
    if (source->pattern.observed.empty())
        return Status{Error{ErrorCode::missing_data, "peaks.initial.pattern", "A continuous pattern is required"}};
    setProperty("initialPeakErrorReason", QString()); setStatusMessage(tr("Searching for initial candidates..."));
    return searchTasks_->submit(context().referenceDatabase(),
        [source, configuration = std::move(configuration)](const SearchTask &task, SearchProgress) {
            using Outcome = qualpf::gui::SearchTaskController::Outcome;
            if (task.source != source) return Outcome{Error{ErrorCode::stale_snapshot, "peaks.initial.source", "Initial peak input changed"}};
            auto found = evaluateInitialPeakSearch(source, configuration, task.context);
            if (!found) return Outcome{found.error()};
            PreparedSearchResult prepared; prepared.source = source;
            prepared.taskId = task.id; prepared.databaseVersion = task.database.selectionVersion; prepared.next = *source;
            auto applied = applyInitialPeakSearch(prepared.next, found.value());
            if (!applied) return Outcome{applied.error()};
            if (prepared.next.projectSettings) prepared.next.projectSettings->profile(WorkflowId::preprocessing).initial_peaks = configuration;
            return Outcome{std::move(prepared)};
        },
        [this, completion = std::move(completion)](auto outcome) mutable {
            if (closingRequested_) return;
            const bool success = bool(outcome);
            if (!success) {
                setProperty("initialPeakErrorReason", QString::fromStdString(outcome.error().reason));
                setStatusMessage(QString::fromStdString(outcome.error().message));
            } else {
                refreshInitialPeakCandidates();
                const auto &usage = context().workspace().session().snapshot()->preprocessing.initial_peak_usage;
                if (usage) setStatusMessage(tr("Initial peak candidates: %1").arg(qulonglong(usage->detection.candidates.size())));
            }
            const QPointer<MainWindow> guard(this);
            if (completion) completion(std::move(outcome));
            if (!guard || closingRequested_) return;
            if (success) showInitialPeakCandidates();
            updateInitialPeakActions();
        }, {}, SearchPublication::commit, {}, WorkflowTarget{WorkflowId::preprocessing,WorkflowStep::initial_peaks});
}
void MainWindow::onActionInitialPeakOptionsTriggered() {
    if (closingRequested_ || searchTasks_->busy() || !ui->actionInitialPeakSearchOptions->isEnabled()) return;
    if (initialPeakDialog_->isVisible()) { initialPeakDialog_->raise(); return; }
    if (initialPeakDialog_->setOptions()) initialPeakDialog_->show();
}
void MainWindow::onActionInitialPeakSearchTriggered() {
    if (closingRequested_ || searchTasks_->busy() || !ui->actionInitialPeakSearch->isEnabled() || initialPeakDialog_->isVisible()) return;
    const auto source = context().workspace().session().snapshot();
    if (!confirmWorkflowReplacement(WorkflowStep::initial_peaks)) return;
    auto status = startInitialPeakSearch(source, source->projectSettings ?
        source->projectSettings->profile(WorkflowId::preprocessing).initial_peaks :
        source->preprocessing.initial_peaks.value_or(InitialPeakConfiguration{}),
        [this](auto outcome) {
            if (!outcome && outcome.error().code != ErrorCode::cancelled && outcome.error().code != ErrorCode::stale_snapshot)
                QMessageBox::warning(this, tr("Initial peak search failed"), QString::fromStdString(outcome.error().message));
        });
    if (!status) setStatusMessage(QString::fromStdString(status.error().message));
}
void MainWindow::updateInitialPeakActions() {
    if (closingRequested_) return;
    const auto source = context().workspace().session().snapshot();
    ui->actionInitialPeakSearch->setEnabled(ui->actionInitialPeakSearchOptions->isEnabled() &&
        !searchTasks_->busy() && !source->editing && !source->pattern.observed.empty() &&
        !source->pattern.radiation.lines.empty() && !(initialPeakDialog_ && initialPeakDialog_->isVisible()));
}
void MainWindow::refreshInitialPeakCandidates() {
    if (closingRequested_) return;
    updateInitialPeakActions();
    const auto state = context().workspace().session().snapshot();
    const auto usage = state->editing ? std::shared_ptr<const InitialPeakUsage>{} : state->preprocessing.initial_peak_usage;
    if (initialPeakDialog_) initialPeakDialog_->setUsage(usage);
}
void MainWindow::showInitialPeakCandidates() {
    if (initialPeakDialog_ && initialPeakDialog_->isVisible()) {
        refreshInitialPeakCandidates();initialPeakDialog_->showCandidates();initialPeakDialog_->raise();return;
    }
    if (context().workspace().session().snapshot()->projectSettings) {
        auto *dialog=new QDialog(this);dialog->setObjectName("initialPeakResults");dialog->setWindowTitle(tr("Initial Peak Candidates"));
        dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->resize(700,430);auto *layout=new QVBoxLayout(dialog);
        auto *results=new InitialPeakCandidatesWidget(dialog);results->setUsage(context().workspace().session().snapshot()->preprocessing.initial_peak_usage);layout->addWidget(results);
        auto *buttons=new QDialogButtonBox(QDialogButtonBox::Close,dialog);connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);layout->addWidget(buttons);
        dialog->show();return;
    }
    refreshInitialPeakCandidates();
    if (!initialPeakDialog_->isVisible()) {
        // Independent execution finishes before the task's finished signal.
        // Preserve the pre-run action state before opening the modal Options view.
        if (searchUiActive_) restoreEnabledActions();
        if (!initialPeakDialog_->setOptions()) return;
        initialPeakDialog_->show();
    }
    initialPeakDialog_->showCandidates(); initialPeakDialog_->raise();
}
