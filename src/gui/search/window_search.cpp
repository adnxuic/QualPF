#include "gui/dialogs/restraintsdialog.h"
#include "gui/dialogs/searchoptionsdialog.h"
#include "gui/dialogs/fwhm_options_dialog.h"
#include "gui/dialogs/initial_peak_options_dialog.h"
#include "gui/dialogs/pfsm_options_dialog.h"
#include "gui/mainwindow.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/search/searchmatch.h"
#include "gui/state/workspace_context.h"
#include "gui/state/preprocessing_product.h"
#include "gui/widgets/dbresultswidget.h"
#include "gui/widgets/peakcomparewidget.h"
#include "gui/widgets/quantwidget.h"
#include "gui/widgets/reportwidget.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/configured_fwhm.h"
#include "qualpf/application/result_management.h"
#include "qualpf/application/search_preparation.h"
#include "qualpf/application/pfsm.h"
#include "gui/plotting/qt_plot_sink.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <algorithm>

using namespace qualpf;
using namespace qualpf::application;

bool MainWindow::searchAdmission() {
    if (initialPeakDialog_ && initialPeakDialog_->isVisible()) {
        setStatusMessage(tr("Finish Initial Peak Search Options first")); return false;
    }
    if (pfsmDialog_ && pfsmDialog_->isVisible()) {
        setStatusMessage(tr("Finish PFSM Options first")); return false;
    }
    if (fwhmDialog_ && fwhmDialog_->isVisible()) {
        setStatusMessage(tr("Finish FWHM Estimation Options first"));
        return false;
    }
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
                if (error.field == "pfsm.fwhm" || error.reason == "pfsm_reimport_required")
                    QMessageBox::warning(this, tr("PFSM could not be started"), QString::fromStdString(error.message));
                return;
            }
            published(outcome.value());
        });
    if (!admitted)
        setStatusMessage(QString::fromStdString(admitted.error().message));
}

void MainWindow::publishSearchView(bool browse) {
    ui->resultsWidget->refresh();
    ui->quantWidget->refresh();
    updateSearchActions();
    const auto warning = qualpf::gui::peakRejectionSummary(context().searchSession().snapshot()->preprocessing);
    setStatusMessage(tr("Found %1 card(s)").arg(ui->resultsWidget->allCards().size()) +
        (warning.isEmpty() ? QString() : "; " + warning));
    if (browse)
        markProjectModified();
    ui->peakCompareWidget->clearAcceptedPhases();
    ui->peakCompareWidget->clearCard();
    ui->peakCompareWidget->setExperimentalPeaks(context().peaks());
    ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult());
    updateReport();
    const auto source = context().workspace().session().snapshot();
    if (source->pfsm) {
        ui->dockWidgetCompare->hide();
        refreshPatternPresentation();
        if (source->pfsm->normalization_maximum == 0)
            setStatusMessage(tr("Current candidates show no improvement"));
        if (ui->resultsWidget->hasResults()) ui->resultsWidget->selectFirstCard();
        return;
    }
    if (ui->resultsWidget->hasResults()) {
        ui->resultsWidget->selectFirstCard();
        ui->dockWidgetCompare->show();
        ui->dockWidgetCompare->raise();
    }
}

void MainWindow::beginMatch(bool restraintsOnly) {
    const auto source = context().workspace().session().snapshot();
    auto preparation = prepareSearch(source, source->algorithms->spec.method());
    if (preparation && preparation.value().state == SearchPreparation::State::needs_pfsm_preparation) {
        setStatusMessage(tr("Preparing PFSM background and FWHM..."));
        auto admitted = searchTasks_->submit(context().referenceDatabase(),
            [source](const SearchTask &task, SearchProgress) {
                using Outcome = qualpf::gui::SearchTaskController::Outcome;
                auto next = evaluatePfsmPreparation(source, task.context);
                if (!next) return Outcome{next.error()};
                return Outcome{PreparedSearchResult{source, task.id, task.database.selectionVersion, std::move(next).value()}};
            }, [this, restraintsOnly](auto outcome) {
                if (closingRequested_) return;
                if (!outcome) {
                    setStatusMessage(QString::fromStdString(outcome.error().message));
                    if (outcome.error().code != ErrorCode::cancelled && outcome.error().code != ErrorCode::stale_snapshot)
                        QMessageBox::warning(this, tr("PFSM preparation failed"), QString::fromStdString(outcome.error().message));
                    return;
                }
                markProjectModified();
                QTimer::singleShot(0, this, [this, restraintsOnly] { if (!closingRequested_) beginMatch(restraintsOnly); });
            }, {}, SearchPublication::commit, {}, WorkflowTarget{WorkflowId::pfsm,WorkflowStep::fwhm});
        if (!admitted) setStatusMessage(QString::fromStdString(admitted.error().message));
        return;
    }
    if (!preparation || preparation.value().state == SearchPreparation::State::needs_peak_preparation) {
        auto failed = [this](const Error &error) {
            setStatusMessage(QString::fromStdString(error.message));
            QMessageBox::warning(this, tr("Preprocessing could not be completed"),
                                 QString::fromStdString(error.reason + ": " + error.message) +
                                     tr("\nNothing was applied. Adjust Preprocessing > Initial Peak Search Options / FWHM Estimation Options / Peak Search > Peak Search Options, or use "
                                        "Preprocessing > Restart preprocessing... / Peak Search > Add Peak."));
        };
        if (!preparation) {
            failed(preparation.error());
            return;
        }
        setStatusMessage(tr("Preparing experimental peaks..."));
        if (!preparation.value().peaks->fwhm)
            preparation.value().peaks->fwhm = source->preprocessing.fwhm.value_or(FwhmConfiguration{});
        // This is a separate transaction. A later canceled search retains the
        // peaks already published by this task, as in the synchronous workflow.
        auto admitted = searchTasks_->submit(
            context().referenceDatabase(),
            [source, request = std::move(*preparation.value().peaks)](const SearchTask &task, SearchProgress) {
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
            }, {}, SearchPublication::commit, {}, WorkflowTarget{WorkflowId::csm,WorkflowStep::peaks});
        if (!admitted)
            setStatusMessage(QString::fromStdString(admitted.error().message));
        return;
    }
    const bool pfsm = source->algorithms->spec.method() == SearchMethod::pfsm;
    auto builder = pfsm ? qualpf::gui::SearchRequest{} : buildSearchMatchQuery(qualpf::gui::workspaceContext(this), context().peaks());
    if (pfsm) builder.setDecodeReflections(true);
    applyDialogRestraints(builder);
    SearchInput input;
    input.source = context().workspace().session().snapshot();
    input.method = input.source->algorithms->spec.method();
    input.query = builder.candidateQuery(SearchOptionsDialog::savedCheckStrongest(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings())
                                             ? CandidateQuery::Route::stored_strongest
                                             : CandidateQuery::Route::matching);
    input.operation = SearchOperation::initial;
    input.parameters = currentSearchParameters(qualpf::gui::workspaceContext(this));
    input.threshold = builder.getMinFom();
    if (pfsm) {
        input.query.route = CandidateQuery::Route::constrained_browse;
        auto request = makePfsmRequest(input.source, qualpf::gui::savedPfsmSettings(context()));
        if (!request) { setStatusMessage(QString::fromStdString(request.error().message)); return; }
        input.pfsm = std::make_shared<const PfsmRequest>(std::move(request).value());
    }
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
        auto intent = phaseAcceptanceInput(source, id, currentSearchParameters(qualpf::gui::workspaceContext(this)),
            SearchOptionsDialog::savedResidualSearching(source, qualpf::gui::workspaceContext(this).settings()));
        if (!intent) {
            setStatusMessage(QString::fromStdString(intent.error().message));
            return;
        }
        if (!intent.value()) continue;
        auto input = std::move(*intent.value());
        if (source->pfsm) {
            auto request = makePfsmRequest(source, qualpf::gui::savedPfsmSettings(context()));
            if (!request) { setStatusMessage(QString::fromStdString(request.error().message)); return; }
            input.pfsm = std::make_shared<const PfsmRequest>(std::move(request).value());
        }
        const CardType card(*input.phase);
        // Preserve the old selected-row order and one atomic commit per phase.
        // Only an accepted completion starts the next internal continuation.
        submitSearch(std::move(input), [this, cards = std::move(cards), index, card](const auto &) mutable {
            if (context().workspace().session().snapshot()->pfsm) {
                ui->quantWidget->refresh(); ui->resultsWidget->refresh(); refreshPatternPresentation();
                ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult());
                ui->dockWidgetQuant->setWindowTitle(tr("Quantitative analysis · PFSM/RIR"));
                ui->dockWidgetQuant->show(); ui->dockWidgetQuant->raise();
                if (index + 1 < cards.size()) acceptPhases(std::move(cards), index + 1);
                else { updateSearchActions(); ui->resultsWidget->selectFirstCard(); }
                return;
            }
            ui->quantWidget->refresh();
            ui->resultsWidget->refresh();
            ui->peakCompareWidget->setExperimentalPeaks(context().peaks());
            ui->peakCompareWidget->addAcceptedPhase(card);
            ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult());
            xpdViewer()->addPhaseReflections(card, cardColor(card.getId(), this));
            ui->dockWidgetQuant->show();
            ui->dockWidgetQuant->raise();
            acceptPhases(std::move(cards), index + 1);
        });
        return;
    }
}

void MainWindow::requestMatch(SearchMethod method, bool restraintsOnly) {
    if (!searchAdmission()) return;
    auto &session = context().workspace().session();
    const auto source = session.snapshot();
    if (source->editing || source->pattern.observed.size() < 3) {
        setStatusMessage(tr("A continuous spectrum and completed preprocessing are required")); return;
    }
    if (restraintsOnly && !m_restraintsDialog->hasRestraints()) return;
    const int minimum = m_restraintsDialog->compositionMinSpecies(), maximum = m_restraintsDialog->compositionMaxSpecies();
    if (minimum && maximum && minimum > maximum) { setStatusMessage(tr("Min species must not exceed Max species")); return; }
    const bool different = method != source->algorithms->spec.method();
    const bool existing = source->pfsm || !source->candidates.empty() || !source->phases.empty();
    const bool end = (different && existing) || (method == SearchMethod::pfsm && bool(source->pfsm));
    if (end && QMessageBox::question(this, tr("End current analysis"),
        tr("End the current analysis and start a new %1 search?").arg(method == SearchMethod::pfsm ? "PFSM" : "CSM"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) return;
    if (!searchAdmission()) return;
    auto prepared = prepareSearchMethod(source, method, end);
    if (!prepared) { setStatusMessage(QString::fromStdString(prepared.error().message)); return; }
    auto next = std::move(prepared).value();
    storePfsmSettings(next.searchOptions, qualpf::gui::savedPfsmSettings(context()));
    if (method == SearchMethod::pfsm) {
        auto request = makePfsmRequest(std::make_shared<const ProductState>(next), pfsmSettings(next.searchOptions));
        if (!request) {
            setStatusMessage(QString::fromStdString(request.error().message));
            QMessageBox::warning(this, tr("PFSM input"), QString::fromStdString(request.error().message)); return;
        }
    }
    next.restraints = m_restraintsDialog->snapshot().toObject();
    auto status = session.commit(source, std::move(next));
    if (!status) { setStatusMessage(QString::fromStdString(status.error().message)); return; }
    if (different || end) {
        for (const auto &phase : source->phases) xpdViewer()->removePhaseReflections(QString::fromStdString(phase.id));
        ui->peakCompareWidget->clearAcceptedPhases(); ui->peakCompareWidget->clearCard();
        ui->quantWidget->refresh(); ui->resultsWidget->refresh();
        ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult());
        ui->dockWidgetQuant->setWindowTitle(method == SearchMethod::pfsm ? tr("Quantitative analysis · PFSM/RIR") : tr("Quantitative analysis"));
        xpdViewer()->yAxis->setLabel(tr("Intensity"));
    }
    m_restraintsDialog->setSelectedSearchMethod(method);
    updateSearchActions(); beginMatch(restraintsOnly);
}

void MainWindow::updateSearchActions() {
    if (closingRequested_) return;
    const auto source = context().workspace().session().snapshot();
    updateInitialPeakActions();
    const bool idle = !searchTasks_->busy() && !source->editing;
    const bool pfsm = source->algorithms->spec.method() == SearchMethod::pfsm;
    ui->actionPFSM->setEnabled(idle && source->pattern.observed.size() >= 3);
    ui->actionLoad_Add->setEnabled(idle && !pfsm);
    m_restraintsDialog->setBrowseEnabled(!pfsm);
    m_restraintsDialog->setSearchEnabled(source->pattern.observed.size() >= 3 && !source->editing);
}

void MainWindow::showPfsmPlot(const PfsmPlot &plot) {
    using namespace qualpf::application::presentation;
    const auto source = context().workspace().session().snapshot();
    if (closingRequested_ || !source->pfsm || !context().presentation().redrawEnabled()) return;
    auto projected = project_pfsm_update(plot, context().presentation().snapshot(),
        {displayedPfsmSource_.lock() == source ? 0 : 1, qualpf::gui::QtPlotSink::abscissa(*xpdViewer())});
    if (!projected) { setStatusMessage(QString::fromStdString(projected.error().message)); return; }
    qualpf::gui::QtPlotSink sink(xpdViewer());
    auto status = sink.apply(projected.value());
    if (!status) { setStatusMessage(QString::fromStdString(status.error().message)); return; }
    displayedPfsmPlot_ = std::make_shared<const PfsmPlot>(plot);
    displayedPfsmSource_ = source;
    xpdViewer()->yAxis->setLabel(tr("Intensity (%1)").arg(QString::fromStdString(plot.intensity_units)));
    xpdViewer()->replot();
    refreshInitialPeakCandidates();
}

void MainWindow::previewPfsmCandidate(const QString &id) {
    if (closingRequested_) return;
    const auto source = context().workspace().session().snapshot();
    if (!source->pfsm) return;
    pfsmPreviewCandidate_ = id;
    if (searchTasks_->busy()) {
        // Keep only the latest GUI selection. The existing owned worker drains
        // before another preview is admitted; intermediate results stay hidden.
        pfsmPreviewPending_ = true;
        return;
    }
    pfsmPreviewPending_ = false;
    const bool candidate = std::any_of(source->candidates.begin(), source->candidates.end(), [&](const auto &c) { return c.id == id.toStdString(); });
    if (!candidate) {
        refreshPatternPresentation();
        return;
    }
    auto status = searchTasks_->submit(context().referenceDatabase(),
        [source, id = id.toStdString()](const SearchTask &task, SearchProgress) {
            using Outcome = qualpf::gui::SearchTaskController::Outcome;
            if (task.source != source) return Outcome{Error{ErrorCode::stale_snapshot,"pfsm.plot","Input changed"}};
            auto plot = preparePfsmPlot(source, id, task.context);
            if (!plot) return Outcome{plot.error()};
            PreparedSearchResult prepared{source, task.id, task.database.selectionVersion, *source};
            prepared.changed = false; prepared.pfsmPlot = std::make_shared<const PfsmPlot>(std::move(plot).value());
            return Outcome{std::move(prepared)};
        }, [this, id](auto outcome) {
            if (closingRequested_ || id != pfsmPreviewCandidate_) return;
            if (outcome && outcome.value().pfsmPlot) showPfsmPlot(*outcome.value().pfsmPlot);
            else if (!outcome) setStatusMessage(QString::fromStdString(outcome.error().message));
        }, {}, SearchPublication::prepare_only);
    if (!status) setStatusMessage(QString::fromStdString(status.error().message));
}

void MainWindow::removePfsmPhase(const QString &id) {
    if (!searchAdmission()) return;
    const auto source = context().workspace().session().snapshot();
    auto input = phaseRemovalInput(source, id.toStdString());
    if (!input) { setStatusMessage(QString::fromStdString(input.error().message)); return; }
    auto request = makePfsmRequest(source, qualpf::gui::savedPfsmSettings(context()));
    if (!request) { setStatusMessage(QString::fromStdString(request.error().message)); return; }
    input.value().pfsm = std::make_shared<const PfsmRequest>(std::move(request).value());
    submitSearch(std::move(input).value(), [this](const auto &) { publishSearchView(false); ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult()); });
}
