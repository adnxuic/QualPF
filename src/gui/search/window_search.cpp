#include "gui/dialogs/restraintsdialog.h"
#include "gui/dialogs/searchoptionsdialog.h"
#include "gui/mainwindow.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/search/searchmatch.h"
#include "gui/state/workspace_context.h"
#include "gui/widgets/dbresultswidget.h"
#include "gui/widgets/peakcomparewidget.h"
#include "gui/widgets/quantwidget.h"
#include "gui/widgets/reportwidget.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <algorithm>

using namespace qualpf;
using namespace qualpf::application;

bool MainWindow::searchAdmission() {
    if (closingRequested_ || searchTasks_->closing())
        return false;
    if (searchTasks_->busy()) {
        setStatusMessage(tr("A search is already running"));
        return false;
    }
    return true;
}

void MainWindow::submitSearch(SearchInput input,
                              std::function<void(const PreparedSearchResult &)> published) {
    const auto id = QString::fromStdString(input.cardId);
    auto admitted = searchTasks_->start(
        std::move(input), context().referenceDatabase(),
        [this, id, published = std::move(published)](qualpf::gui::SearchTaskController::Outcome outcome) {
            if (closingRequested_)
                return;
            if (!outcome) {
                const auto &error = outcome.error();
                setStatusMessage(QString::fromStdString(error.message));
                if (error.reason == "card_not_found")
                    QMessageBox::warning(this, tr("Card Not Found"),
                                         tr("No card with ID \"%1\" found in the database.").arg(id));
                return;
            }
            published(outcome.value());
        },
        [this](PreparedSearchResult &result) -> Status {
            if (result.changed)
                result.next.candidates =
                    ui->resultsWidget->orderForPublication(std::move(result.next.candidates));
            return Status{std::monostate{}};
        });
    if (!admitted)
        setStatusMessage(QString::fromStdString(admitted.error().message));
}

void MainWindow::publishSearchView(bool browse) {
    ui->resultsWidget->refresh();
    setStatusMessage(tr("Found %1 card(s)").arg(ui->resultsWidget->allCards().size()));
    if (browse)
        markProjectModified();
    ui->peakCompareWidget->clearAcceptedPhases();
    ui->peakCompareWidget->clearCard();
    ui->peakCompareWidget->setExperimentalPeaks(context().peaks());
    ui->reportWidget->clearQuantitative();
    updateReport();
    if (ui->resultsWidget->hasResults()) {
        ui->resultsWidget->selectFirstCard();
        ui->dockWidgetCompare->show();
        ui->dockWidgetCompare->raise();
    }
}

void MainWindow::beginMatch(bool restraintsOnly) {
    const auto source = context().workspace().session().snapshot();
    if (!source->peaks.valid) {
        auto request = automaticPreprocessingRequest(source, PreprocessingAction::prepare_search);
        auto failed = [this](const Error &error) {
            setStatusMessage(QString::fromStdString(error.message));
            QMessageBox::warning(this, tr("Preprocessing could not be completed"),
                                 QString::fromStdString(error.reason + ": " + error.message) +
                                     tr("\nNothing was applied. Adjust Peak Search conditions, or use "
                                        "Pattern > Restart preprocessing / Add Peak."));
        };
        if (!request) {
            failed(request.error());
            return;
        }
        setStatusMessage(tr("Preparing experimental peaks..."));
        // This is a separate transaction. A later canceled search retains the
        // peaks already published by this task, as in the synchronous workflow.
        auto admitted = searchTasks_->submit(
            context().referenceDatabase(),
            [source, request = std::move(request).value()](const SearchTask &task, SearchProgress) {
                using Outcome = qualpf::gui::SearchTaskController::Outcome;
                if (task.source != source)
                    return Outcome{Error{ErrorCode::stale_snapshot, "preprocessing", "Search input changed"}};
                PreprocessingEvaluation evaluation(source);
                auto status = evaluation.preview(request, task.context.cancellation);
                if (!status)
                    return Outcome{status.error()};
                PreparedSearchResult result;
                result.source = source;
                result.taskId = task.id;
                result.databaseVersion = task.database.selectionVersion;
                result.next = *evaluation.preview();
                return Outcome{std::move(result)};
            },
            [this, restraintsOnly, failed](qualpf::gui::SearchTaskController::Outcome outcome) {
                if (closingRequested_)
                    return;
                if (!outcome) {
                    if (outcome.error().code == ErrorCode::cancelled ||
                        outcome.error().code == ErrorCode::stale_snapshot)
                        setStatusMessage(QString::fromStdString(outcome.error().message));
                    else
                        failed(outcome.error());
                    return;
                }
                refreshPatternPresentation();
                beginMatch(restraintsOnly);
            });
        if (!admitted)
            setStatusMessage(QString::fromStdString(admitted.error().message));
        return;
    }
    if (source->peaks.tth.empty()) {
        QMessageBox::warning(this, tr("No Peaks Found"), tr("Please run Peak Search first."));
        return;
    }
    auto builder = buildSearchMatchQuery(qualpf::gui::workspaceContext(this), context().peaks());
    applyDialogRestraints(builder);
    SearchInput input;
    input.source = context().workspace().session().snapshot();
    input.query = builder.candidateQuery(SearchOptionsDialog::savedCheckStrongest(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings())
                                             ? CandidateQuery::Route::stored_strongest
                                             : CandidateQuery::Route::matching);
    input.operation = SearchOperation::initial;
    input.parameters = currentSearchParameters(qualpf::gui::workspaceContext(this));
    input.threshold = builder.getMinFom();
    setStatusMessage(m_restraintsDialog->hasRestraints() ? tr("Searching database with restraints...")
                                                         : tr("Searching database..."));
    submitSearch(std::move(input), [this](const auto &) { publishSearchView(false); });
}

void MainWindow::acceptPhases(QVector<CardType> cards, int index) {
    if (!searchAdmission())
        return;
    for (; index < cards.size(); ++index) {
        const auto source = context().workspace().session().snapshot();
        if (source->editing)
            return;
        const auto id = cards[index].productValue().id;
        const auto found = std::find_if(source->candidates.begin(), source->candidates.end(),
                                        [&](const auto &card) { return card.id == id; });
        if (found == source->candidates.end() || !found->fomCalculated ||
            std::any_of(source->phases.begin(), source->phases.end(),
                        [&](const auto &card) { return card.id == id; }))
            continue;
        SearchInput input;
        input.source = source;
        input.operation = SearchOperation::accept_phase;
        input.phase = *found;
        input.parameters = currentSearchParameters(qualpf::gui::workspaceContext(this));
        input.subtractResidual = SearchOptionsDialog::savedResidualSearching(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings());
        const CardType card(*found);
        // Preserve the old selected-row order and one atomic commit per phase.
        // Only an accepted completion starts the next internal continuation.
        submitSearch(std::move(input), [this, cards = std::move(cards), index, card](const auto &) mutable {
            ui->quantWidget->refresh();
            ui->resultsWidget->refresh();
            ui->peakCompareWidget->setExperimentalPeaks(context().peaks());
            ui->peakCompareWidget->addAcceptedPhase(card);
            ui->reportWidget->updateQuantitative(ui->quantWidget->phases(),
                                                 ui->quantWidget->quantPercentages());
            xpdViewer()->addPhaseReflections(card, cardColor(card.getId()));
            ui->dockWidgetQuant->show();
            ui->dockWidgetQuant->raise();
            acceptPhases(std::move(cards), index + 1);
        });
        return;
    }
}
