



#include "qualpf/application/domain_inputs.h"
#include "gui/state/pattern_input.h"
#include "gui/presentation/product_plot_refresh.h"
#include "gui/presentation/project_view_values.h"
#include "gui/plotting/qt_plot_sink.h"
#include "gui/dialogs/wavedialog.h"
#include "gui/state/preprocessing_product.h"
#include "gui/dialogs/manual_peak_dialog.h"
#include "gui/dialogs/fwhm_options_dialog.h"
#include "gui/dialogs/initial_peak_options_dialog.h"
#include "gui/dialogs/pfsm_options_dialog.h"
#include "qualpf/application/pfsm.h"
#include "qualpf/application/kalpha2.h"
#include "gui/mainwindow.h"
#include <QScopedValueRollback>
#include <QTableView>
#include <QHeaderView>
#include "gui/widgets/workflow_dock.h"
#include "ui_mainwindow.h"
#include <QSignalBlocker>
#include "config.h"
#include "gui/state/workspace_context.h"
#include "gui/export/pattern_export_service.h"
#include "gui/import/powder_cif_import_service.h"

#include "gui/model/experimentalpeaks.h"
#include "gui/search/searchmatch.h"
#include "gui/search/peakassoc.h"
#include "gui/search/search_request.h"
#include "gui/dialogs/managedatabasesdialog.h"
#include "qualpf/application/settings_keys.h"
#include "gui/adapters/preferences.h"
#include "gui/dialogs/restraintsdialog.h"
#include "gui/dialogs/savedialog.h"
#include "gui/dialogs/searchoptionsdialog.h"
#include "gui/dialogs/imgsettingsdialog.h"


#include "gui/plotting/xpdutils.h"
#include "gui/plotting/background_coordinate.h"
#include "gui/plotting/peak_coordinate.h"
#include "gui/dialogs/inferred_peak_dialog.h"
#include "gui/state/peak_policy.h"

#include "gui/widgets/dbresultswidget.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include "gui/platform/updater.h"
#include "gui/platform/maintenancetool.h"
#include "gui/printing/printdialog.h"
#include "gui/widgets/reportwidget.h"
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QFileInfo>
#include <QUrl>
#include <QDebug>
#include <bit>
#include <QInputDialog>



#include <QMenu>
#include <QMessageBox>
#include <QStandardPaths>

#include <algorithm>

#include <QTimer>

QString MainWindow::pathDataFiles = "";




















MainWindow::MainWindow(qualpf::gui::WorkspaceContext &workspaceContext, QWidget *parent, std::shared_ptr<qualpf::application::CandidateReaderFactory> factory,
                       qualpf::gui::SearchTaskController::Trace trace)
    : QMainWindow(parent)
    , context_(workspaceContext)
    , ui(new Ui::MainWindow)
    , mAction(NoAction)
    , savedZoomAction(mAction)
    , projectFileSaved(true)
{
    if (!context().presentation().configured())
        context().presentation().configure(qualpf::application::PresentationMode::interactive_gui);

    qualpf::gui::bindWorkspaceContext(*this, context());
    ui->setupUi(this);
    searchTasks_ = new qualpf::gui::SearchTaskController(context().workspace().searches(),
        factory ? std::move(factory) : context().readerFactory(), this, std::move(trace));
    connect(searchTasks_, &qualpf::gui::SearchTaskController::started, this, [this](quint64 id) {
        if (closingRequested_) return;
        searchUiTask_ = id;
        if (!searchUiActive_) {
            searchUiActive_ = true;
            if (!(fwhmDialog_ && fwhmDialog_->isVisible()) && !(initialPeakDialog_ && initialPeakDialog_->isVisible())) enableActions(SaveAction);
            QApplication::setOverrideCursor(Qt::WaitCursor);
        }
        enableActions(RunAction); ui->resultsWidget->setEnabled(false); ui->quantWidget->setEnabled(false);
        m_restraintsDialog->setBusy(true);
        statusProgressBar->setRange(0,100); statusProgressBar->setValue(0); statusProgressBar->show();
        setProperty("searchState", "running");
    });
    connect(searchTasks_, &qualpf::gui::SearchTaskController::progress, this, [this](quint64 id, quint64 current, quint64 total) {
        if (closingRequested_) return;
        if (id == searchUiTask_ && !closingRequested_) statusProgressBar->setValue(total ? int(100.0*double(current)/double(total)) : 0);
    });
    connect(searchTasks_, &qualpf::gui::SearchTaskController::finished, this, [this](quint64 id, bool) {
        if (closingRequested_) return;
        if (!searchTasks_->busy() && pfsmPreviewPending_) {
            pfsmPreviewPending_ = false;
            previewPfsmCandidate(pfsmPreviewCandidate_);
        }
        if (id != searchUiTask_ || closingRequested_ || searchTasks_->busy()) return;
        if (searchUiActive_) {
            searchUiActive_=false; QApplication::restoreOverrideCursor();
            if (!(fwhmDialog_ && fwhmDialog_->isVisible()) && !(initialPeakDialog_ && initialPeakDialog_->isVisible())) enableActions(RestoreAction);
        }
        statusProgressBar->hide(); ui->resultsWidget->setEnabled(true); ui->quantWidget->setEnabled(true);
        setProperty("searchState", "idle");
        m_restraintsDialog->setBusy(false);
        updateSearchActions();
    });
    connect(searchTasks_, &qualpf::gui::SearchTaskController::drained, this, [this] {
        if (closingRequested_) QTimer::singleShot(0,this,[this] { close(); });
    });
    connect(xpdViewer(), &XpdViewWidget::plotError, this, &MainWindow::setStatusMessage);
    ui->cardBrowser->setOpenExternalLinks(true);
    ui->resultsWidget->setEntryToolBar(ui->toolBarEntry);
    ui->resultsWidget->setContextMenuActions({
        ui->actionLoad_Add,
        nullptr,  // separator
        ui->actionAccept_Selected_Entries,
        ui->actionDelete,
        ui->actionChange_Color
    });

    tabifyDockWidget(ui->peakDockWidget, ui->dockWidgetCompare);
    tabifyDockWidget(ui->dockWidgetCompare, ui->dockWidgetCard);
    tabifyDockWidget(ui->dockWidgetCard, ui->dockWidgetQuant);
    tabifyDockWidget(ui->dockWidgetQuant, ui->dockWidgetReport);
    ui->peakDockWidget->raise();

    setWindowTitle(qApp->applicationDisplayName()+"-"+qApp->applicationVersion());

    // set objects for the statusbar
    statusLabel1 = new QLabel(this);
    statusLabel1->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    statusProgressBar = new QProgressBar(this);
    statusProgressBar->setTextVisible(true);
    statusProgressBar->hide();
    ui->statusBar->addPermanentWidget(statusLabel1,3);
    ui->statusBar->addPermanentWidget(statusProgressBar,1);
    // send statustip on statusLabel1
    connect(ui->statusBar, &QStatusBar::messageChanged, this, [=](){
        statusLabel1->setText(ui->statusBar->currentMessage());
    });

    createActionGroup();
    createDialogs();
    createWorkflowUi();
    actionsSetup();
    readAction();

    xpdViewer()->connectLabels(ui->plotLabel1, ui->plotLabel2, ui->plotLabel3);
    xpdViewer()->connectHorizontalScrollBar(ui->horizontalScrollBar);
    xpdViewer()->connectAddDeletePoints();
    xpdViewer()->connectSelectionChanged();
    connect(xpdViewer(), &XpdViewWidget::deleteSelectedPeaksSignal, this, &MainWindow::deleteSelectedPeaks);
    connect(xpdViewer(), &XpdViewWidget::addDeletePointSignal, this, &MainWindow::addDeleteSelectedPoint);
    connect(xpdViewer(), &XpdViewWidget::fileDropped, this, &MainWindow::onActionFileDropped);
    ui->actionLegend->setChecked(xpdViewer()->pSettings().legendVisible);

    ui->xpdWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->xpdWidget, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        if (closingRequested_) return;
        QMenu menu(this);
        menu.addActions(ui->menuView->actions());
        menu.exec(ui->xpdWidget->mapToGlobal(pos));
    });

    defaultState = saveState();
    readSettings();
    createRecentActions();
    enableActions(InitAction);
    connect(xpdViewer(), &QCustomPlot::afterReplot, this, &MainWindow::captureProjectView);
    connect(ui->resultsWidget, &DbResultsWidget::selectedCardsChanged, this, [this] { captureProjectView(); });
    connect(ui->resultsWidget->sourceModel, &QAbstractItemModel::layoutChanged, this, [this] { captureProjectView(); });

    auto dragStart=std::make_shared<std::shared_ptr<const qualpf::application::ProjectViewState>>();
    connect(xpdViewer(), &QCustomPlot::mousePress, this, [this,dragStart](QMouseEvent *) { *dragStart=context().workspace().projectView(); });
    connect(xpdViewer(), &QCustomPlot::mouseRelease, this, [this,dragStart](QMouseEvent *) {
        captureProjectView();
        if(*dragStart && **dragStart!=*context().workspace().projectView()) markProjectModified();
        dragStart->reset();
    });
    auto rememberQueuedView=[this] {
        const auto before=context().workspace().projectView();
        QTimer::singleShot(0,this,[this,before] { captureProjectView(); if(*before!=*context().workspace().projectView()) markProjectModified(); });
    };
    connect(xpdViewer(), &QCustomPlot::mouseWheel, this, [rememberQueuedView](QWheelEvent *) { rememberQueuedView(); });
    connect(xpdViewer(), &QCustomPlot::mouseDoubleClick, this, [rememberQueuedView](QMouseEvent *) { rememberQueuedView(); });
    if(auto *table=ui->resultsWidget->findChild<QTableView *>())
        connect(table->horizontalHeader(), &QHeaderView::sectionClicked, this, [this] { captureProjectView(); markProjectModified(); });

    qualpf::gui::presentation::bind_product_plot_refresh(context(), this, [this]() -> qualpf::Status {
        if (refreshPatternPresentation()) return qualpf::Status{std::monostate{}};
        return qualpf::Status{qualpf::Error{qualpf::ErrorCode::invalid_input, "plot.presenter",
            "Cannot refresh the product plot", "projection_or_sink_failure"}};
    });
}

MainWindow::~MainWindow()
{
    Q_ASSERT(!searchTasks_->busy());
    closingRequested_ = true;
    xpdViewer()->setProperty("workspaceClosing", true);
    qualpf::gui::presentation::unbind_product_plot_refresh(context(), this);
    QObject::disconnect(&context().searchSession(), nullptr, this, nullptr);
    // Range cancellation must finish while Session presentation widgets live.
    const auto ranges = findChildren<RangeDialog *>(QString(),Qt::FindDirectChildrenOnly);
    for (auto *range : ranges) delete range;
    rangeDialog = nullptr;
    // The peak draft's Cancel publishes through Session::changed. Finish its
    // lifetime while the presentation widgets and this window are still alive.
    delete peakSearchDialog;
    peakSearchDialog = nullptr;
    // Finish the smoothing draft while its presentation owner is alive.
    delete smoothingDialog;
    smoothingDialog = nullptr;
    // Finish the background draft while its presentation owner is alive.
    delete backgroundDialog;
    backgroundDialog = nullptr;
    delete initialPeakDialog_; initialPeakDialog_ = nullptr;
    delete fwhmDialog_;
    fwhmDialog_ = nullptr;
    // Retire the observer and selection callbacks while their workspace and controls live.
    delete workflowDock_; workflowDock_ = nullptr;
    delete ui;
}

void MainWindow::createDialogs()
{
    if (closingRequested_) return;
    peakSearchDialog   = new PeakSearchDialog(this);
    backgroundDialog   = new BackgroundDialog(this);
    fwhmDialog_ = new FwhmOptionsDialog(this);
    initialPeakDialog_ = new InitialPeakOptionsDialog(this);
    connect(initialPeakDialog_, &QDialog::finished, this, [this] {
        if (!closingRequested_) { restoreEnabledActions(); updateInitialPeakActions(); }
    });
    pfsmDialog_ = new PfsmOptionsDialog(this);
    connect(pfsmDialog_, &PfsmOptionsDialog::settingsApplied, this, [this] {
        ui->resultsWidget->refresh(); markProjectModified();
    });
    connect(fwhmDialog_, &QDialog::finished, this, [this] {
        if (!closingRequested_) { restoreEnabledActions(); updateFwhmActions(); }
    });
    m_restraintsDialog = new RestraintsDialog(this);
    smoothingDialog    = new SmoothingDialog(this);
    plotStyleDialog    = new PlotStyleDialog(this);
    aboutDialog = new AboutDialog(this);
    aboutDialog->setWebsiteUrl("https://www.ba.ic.cnr.it/softwareic/qualx/");
    aboutDialog->setContactsUrl("https://www.ba.ic.cnr.it/softwareic/expo/contact-us/");
    aboutDialog->setCitationUrl("https://doi.org/10.1017/S0885715617000240");

    connect(&context().searchSession(), &qualpf::gui::SessionView::changed, this,
            [this, previous = context().searchSession().snapshot()]() mutable {
        if (closingRequested_) return;
        const auto state = context().searchSession().snapshot();
        if (previous->pattern.id != state->pattern.id || previous->pattern.source_id != state->pattern.source_id)
            context().presentation().setBackgroundSubtraction(false);
        updateBackgroundActions();
        // Checked RIR revisions retain the frozen problem and fit generation.
        // Detect this canonical change for every caller without resetting candidates.
        const auto &oldPfsm = previous->pfsm;
        const auto &newPfsm = state->pfsm;
        if (displayedPfsmPlot_ && oldPfsm && oldPfsm == newPfsm)
            displayedPfsmSource_ = context().workspace().session().snapshot();
        if (oldPfsm && newPfsm && oldPfsm->problem == newPfsm->problem &&
            oldPfsm->universe == newPfsm->universe &&
            oldPfsm->quantification_library == newPfsm->quantification_library &&
            oldPfsm->generation == newPfsm->generation &&
            oldPfsm->rir_revisions != newPfsm->rir_revisions) {
            // Quantification-only revisions retain the displayed joint trial.
            if (displayedPfsmPlot_) displayedPfsmSource_ = context().workspace().session().snapshot();
            previous = state;
            ui->quantWidget->refresh();
            return;
        }
        const auto &oldBackground = previous->pattern.background;
        const auto &newBackground = state->pattern.background;
        const bool sameBackground = (!oldBackground && !newBackground) ||
            (oldBackground && newBackground && oldBackground->values == newBackground->values &&
             oldBackground->window.has_value() == newBackground->window.has_value() &&
             (!oldBackground->window || oldBackground->window->first == newBackground->window->first));
        auto oldConfiguration = qualpf::application::preprocessingValues(previous->preprocessing);
        auto newConfiguration = qualpf::application::preprocessingValues(state->preprocessing);
        // Peak persistence equality excludes regenerated row IDs (P2).
        for (const auto *key : {"candidate_pool", "candidate_failure", "peaks", "selection"}) {
            oldConfiguration.erase(key);
            newConfiguration.erase(key);
        }
        if (!state->editing &&
            (!sameBackground || previous->pattern.observed != state->pattern.observed ||
             previous->pattern.two_theta != state->pattern.two_theta ||
             previous->pattern.processing.kalpha2_subtracted != state->pattern.processing.kalpha2_subtracted ||
             oldConfiguration != newConfiguration))
            markProjectModified();
        if (!state->editing && !qualpf::gui::peakEditorEquivalent(*previous,*state))
            markProjectModified();
        if (!state->editing && previous->projectSettings && state->projectSettings) {
            const auto &before=previous->projectSettings->profile(qualpf::application::WorkflowId::preprocessing).range;
            const auto &after=state->projectSettings->profile(qualpf::application::WorkflowId::preprocessing).range;
            if (bool(before)!=bool(after) || (before && after && (before->minimum!=after->minimum || before->maximum!=after->maximum))) markProjectModified();
        }
        if ((!previous->candidates.empty() && state->candidates.empty()) ||
            (!previous->phases.empty() && state->phases.empty())) {
            ui->peakCompareWidget->clearCard();
            ui->peakCompareWidget->clearAcceptedPhases();
        }
        previous = state;
        const auto bars = xpdViewer()->refSet;
        for (const auto &bar : bars)
            if (!bar.id.isEmpty() && (state->pfsm || std::none_of(state->phases.begin(), state->phases.end(),
                    [&](const auto &card) { return card.getId() == bar.id; })))
                xpdViewer()->removePhaseReflections(bar.id);
        // PFSM publishes exact joint curves. Its discrete d/I library does not
        // carry the Classical display-angle arrays used by reflection bars.
        if (!state->pfsm)
            for (const auto &card : state->phases)
                xpdViewer()->addPhaseReflections(card, cardColor(card.getId(), this));
        refreshPreprocessingDisplay();
        updateInitialPeakActions();
        ui->resultsWidget->refresh();
        ui->quantWidget->refresh();
        ui->peakCompareWidget->refresh();
        ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult());
        updateReport();
    });

    auto *recovery = ui->actionRestartPreprocessing;
    connect(recovery, &QAction::triggered, this, [this] {
        if (qualpf::gui::cppPreprocessingProduct()) qualpf::gui::recoverPreprocessing(*this);
        else setStatusMessage(tr("Restart preprocessing is available with the C++ preprocessing backend."));
    });
    auto *clearPeaks = ui->actionDeleteAllPeaks;
    connect(clearPeaks, &QAction::triggered, this, [this] {
        auto allowed = qualpf::gui::peakEditEntryStatus(qualpf::gui::workspaceContext(this));
        if (!allowed) {setStatusMessage(QString::fromStdString(allowed.error().reason));return;}
        if (!confirmWorkflowReplacement(qualpf::application::WorkflowStep::peaks)) return;
        qualpf::gui::PreprocessingEdit edit(context().searchSession());
        qualpf::application::PreprocessingRequest request;
        request.manual = qualpf::application::ManualPeakChange{};
        request.manual->operation = qualpf::application::ManualPeakChange::Operation::clear;
        request.tolerance = qualpf::AngularWidthDegrees{context().searchSession().snapshot()->peakTolerance.value_or(.15)};
        auto status = edit.preview(request);
        if (status) status = edit.commit();
        if (!status) setStatusMessage(QString::fromStdString(status.error().message));
        else { refreshPatternPresentation(); refreshPreprocessingDisplay(); }
    });

    connect(ui->resultsWidget, &DbResultsWidget::hasResultsChanged,
            m_restraintsDialog, &RestraintsDialog::setMergeEnabled);

    connect(m_restraintsDialog, &RestraintsDialog::loadCardsRequested,
            this, [this]() {
        if (closingRequested_) return; onRestraintsSearch(false); });
    connect(m_restraintsDialog, &RestraintsDialog::loadAndMergeCardsRequested,
            this, [this]() {
        if (closingRequested_) return; onRestraintsSearch(true); });
    connect(m_restraintsDialog, &RestraintsDialog::searchWithRestraintsRequested,
            this, &MainWindow::onRestraintsSearchMatch);

    connect(ui->resultsWidget, &DbResultsWidget::cardSelected,
            this, &MainWindow::onCardSelected);

    connect(ui->quantWidget, &QuantWidget::cardSelected,
            this, &MainWindow::onCardSelected);

    connect(ui->resultsWidget, &DbResultsWidget::cardDataSelected,
            this, [this](const CardType &card) {
        if (closingRequested_) return;
        if (context().workspace().session().snapshot()->pfsm) return;
                ui->peakCompareWidget->setSelectedCard(
                    card, card.getId(), SearchOptionsDialog::savedDelta2theta(qualpf::gui::workspaceContext(this).workspace().session().snapshot(), qualpf::gui::workspaceContext(this).settings()));
            });

    connect(ui->resultsWidget, &DbResultsWidget::entrySelectionChanged,
            this, [this](bool hasSelection) {
        if (closingRequested_) return;
                if (!hasSelection)
                    ui->peakCompareWidget->clearCard();
            });

    connect(ui->resultsWidget, &DbResultsWidget::phaseAccepted,
            this, [this](const CardType &selected) {
        if (closingRequested_) return; acceptPhases({selected}); });

    connect(ui->resultsWidget, &DbResultsWidget::phasesAccepted, this, [this](const QVector<CardType> &cards) {
        if (closingRequested_) return; acceptPhases(cards); });

    connect(ui->quantWidget, &QuantWidget::phaseRemoved,
            this, [this](const CardType &card) {
        if (closingRequested_) return;
                ui->resultsWidget->addCard(card);
                ui->peakCompareWidget->removeAcceptedPhase(card.getId());
                ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult());
                xpdViewer()->removePhaseReflections(card.getId());
            });
    connect(ui->quantWidget, &QuantWidget::phaseRemovalRequested, this, &MainWindow::removePfsmPhase);
    connect(ui->quantWidget, &QuantWidget::quantificationChanged, this, [this] {
        if (closingRequested_) return;
        const auto &result = ui->quantWidget->quantificationResult();
        ui->reportWidget->updateQuantitative(result);
        ui->dockWidgetQuant->setWindowTitle(result.method == qualpf::application::QuantificationMethod::pfsm_rir
            ? tr("Quantitative analysis · PFSM/RIR") : tr("Quantitative analysis"));
    });

    connect(ui->peakCompareWidget, &PeakCompareWidget::selectedComparePointsChanged,
            this, [this](const QVector<double> &tth,
                         const QVector<double> &intensity,
                         const QVector<QColor> &colors) {
        if (closingRequested_) return;
                xpdViewer()->drawSelectedComparePoints(tth, intensity, colors);
            });
}

void MainWindow::actionsSetup()
{
    if (closingRequested_) return;
    //File menu
    connect(ui->actionNew, &QAction::triggered, this, &MainWindow::onActionNewTriggered);
    connect(ui->actionImportDiffractionPattern, &QAction::triggered, this, &MainWindow::onActionImportDiffractionPatternTriggered);
    connect(ui->actionOpen_Project, &QAction::triggered, this, &MainWindow::onActionLoadProjectTriggered);
    connect(ui->actionSave_Project, &QAction::triggered, this, &MainWindow::onActionSaveProjectTriggered);
    connect(ui->actionSave_Project_As, &QAction::triggered, this, &MainWindow::onActionSaveProjectAsTriggered);
    connect(ui->actionExport_Diffraction_Pattern, &QAction::triggered, this, &MainWindow::onActionExportDiffractionPattern);
    connect(ui->actionImage_Powder_Pattern, &QAction::triggered, this, &MainWindow::onActionImagePowderPatternTriggered);
    connect(ui->actionExit, &QAction::triggered, this, &QWidget::close);

    auto *legacyCifAction = new QAction(tr("Import Powder CIF (Legacy)…"), this);
    for (auto *menu : findChildren<QMenu *>()) {
        if (menu->actions().contains(ui->actionImportDiffractionPattern)) {
            menu->insertAction(ui->actionImportDiffractionPattern, legacyCifAction);
            break;
        }
    }
    connect(legacyCifAction, &QAction::triggered, this, [this] {
        const auto file = QFileDialog::getOpenFileName(this, tr("Import Powder CIF (Legacy)"),
            qualpf::gui::Preferences(qualpf::gui::workspaceContext(this).preferences()).value(DEFAULT_DIR_KEY).toString(), tr("Powder CIF (*.cif *.rtv)"));
        if (!file.isEmpty()) loadPowderCif(file);
    });

    //Preprocessing and independent Peak Search menus
    connect(ui->actionRange, &QAction::triggered, this, &MainWindow::onActionRangeTriggered);
    connect(ui->actionBackground, &QAction::triggered, this, &MainWindow::onActionBackgroundTriggered);
    connect(ui->actionBackgroundOptions, &QAction::triggered, this, &MainWindow::onActionBackgroundOptionsTriggered);
    connect(ui->actionExport_Background, &QAction::triggered, this, &MainWindow::onActionBackgroundExportTriggered);
    connect(ui->actionSubtract_Background, &QAction::triggered, this, &MainWindow::onActionSubtractBackgroundTriggered);
    connect(ui->actionSmoothing, &QAction::triggered, this, &MainWindow::onActionSmoothingTriggered);
    connect(ui->actionK_alpha2_Stripping, &QAction::triggered, this, &MainWindow::onActionKAlpha2StrippingTriggered);
    connect(ui->actionPeak_Search, &QAction::triggered, this, &MainWindow::onActionPeakSearchTriggered);
    connect(ui->actionLoad_Peaks, &QAction::triggered, this, &MainWindow::onActionLoadPeaksTriggered);
    connect(ui->actionSave_Peaks, &QAction::triggered, this, &MainWindow::onActionSavePeaksTriggered);
    connect(ui->actionPeak_Search_Conditions, &QAction::triggered, this, &MainWindow::onActionPeakSearchConditionsTriggered);
    connect(ui->actionInitialPeakSearchOptions, &QAction::triggered, this, &MainWindow::onActionInitialPeakOptionsTriggered);
    connect(ui->actionInitialPeakSearch, &QAction::triggered, this, &MainWindow::onActionInitialPeakSearchTriggered);
    connect(ui->actionFwhmEstimationOptions, &QAction::triggered, this, &MainWindow::onActionFwhmOptionsTriggered);
    connect(ui->actionFwhmEstimation, &QAction::triggered, this, &MainWindow::onActionFwhmEstimationTriggered);

    //View menu
    connect(ui->actionPlot_Style, &QAction::triggered, this, &MainWindow::onActionPlotStyleTriggered);
    connect(ui->actionReset_Zoom, &QAction::triggered, this, &MainWindow::onActionResetZoomTriggered);
    connect(ui->actionAutoscale, &QAction::triggered, this, &MainWindow::onActionAutoscaleTriggered);
    connect(ui->actionLegend, &QAction::triggered, this, [=](bool state){
        xpdViewer()->setLegendVisible(state);
        captureProjectView(); markProjectModified();
    });
    connect(plotStyleDialog, &PlotStyleDialog::dialogClosed, this, &MainWindow::plotStyleClosed);
    connect(plotStyleDialog, &PlotStyleDialog::applyOffsetRequested, this, [this](double offset){
        if (closingRequested_) return; xpdViewer()->applyOffset(offset); });
    connect(plotStyleDialog, &PlotStyleDialog::redrawRequested, this, [this](){
        if (closingRequested_) return; xpdViewer()->redrawPlot(); });

    //S&M menu
    connect(ui->actionSearch_Match, &QAction::triggered, this, &MainWindow::onActionSearchMatchTriggered);
    connect(ui->actionSearch_Match_Options, &QAction::triggered, this, &MainWindow::onActionSearchMatchOptionsTriggered);
    connect(ui->actionPFSMOptions, &QAction::triggered, this, [this] {
        if (closingRequested_ || searchTasks_->busy() || context().searchSession().snapshot()->editing) return;
        if (pfsmDialog_->isVisible()) { pfsmDialog_->raise(); return; }
        if (pfsmDialog_->setOptions()) pfsmDialog_->show();
    });
    connect(ui->actionPFSM, &QAction::triggered, this, [this] { requestMatch(qualpf::application::SearchMethod::pfsm); });
    connect(ui->actionRestraints, &QAction::triggered, this, &MainWindow::actionRestraintsTriggered);
    connect(ui->actionTestDatabase, &QAction::triggered, this, &MainWindow::onActionTestDatabaseTriggered);
    connect(ui->actionCancel_Search, &QAction::triggered, this, &MainWindow::onActionCancelSearchTriggered);
    //connect(ui->actionGetCard, &QAction::triggered, this, &MainWindow::onActionGetCardTriggered);
    connect(ui->actionLoad_Add, &QAction::triggered, this, &MainWindow::onActionLoadAddTriggered);
    connect(ui->actionManage_Databases, &QAction::triggered, this, &MainWindow::actionManageDatabasesTriggered);

    ui->actionRecalculate_FOM->setEnabled(false);
    connect(ui->resultsWidget, &DbResultsWidget::hasResultsChanged,
            ui->actionRecalculate_FOM, &QAction::setEnabled);
    connect(ui->actionRecalculate_FOM, &QAction::triggered,
            this, &MainWindow::onActionRecalculateFomTriggered);

    //Window menu
    QList<QAction *> actions = createPopupMenu()->actions();
    foreach (QAction *action, actions) {
        ui->menuWindow->addAction(action);
    }
    connect(ui->actionDefault_Layout, &QAction::triggered, this, &MainWindow::onActionDefaultLayoutTriggered);

    //Help menu
    connect(ui->actionDocumentation_HTML,   &QAction::triggered, this, &MainWindow::onActionDocumentationHtmlTriggered);
    connect(ui->actionDocumentation_PDF,    &QAction::triggered, this, &MainWindow::onActionDocumentationPdfTriggered);
    connect(ui->actionPrint,                &QAction::triggered, this, &MainWindow::onActionPrintTriggered);
    connect(ui->actionCheck_for_Updates,    &QAction::triggered, this, &MainWindow::onActionCheckForUpdatesTriggered);
    connect(ui->actionAbout,                &QAction::triggered, this, &MainWindow::onActionAboutTriggered);

    //Entry menu
    ui->actionAccept_Selected_Entries->setEnabled(false);
    ui->actionDelete->setEnabled(false);
    ui->actionChange_Color->setEnabled(false);

    auto updateEntryActions = [this]() {
        const bool on = !closingRequested_ && !searchTasks_->busy() && ui->resultsWidget->hasResults() && ui->resultsWidget->hasSelection();
        ui->actionAccept_Selected_Entries->setEnabled(on);
        ui->actionDelete->setEnabled(on);
        ui->actionChange_Color->setEnabled(on);
    };
    connect(searchTasks_, &qualpf::gui::SearchTaskController::finished, this, [updateEntryActions] { updateEntryActions(); });
    connect(ui->resultsWidget, &DbResultsWidget::hasResultsChanged,
            this, [updateEntryActions](bool) { updateEntryActions(); });
    connect(ui->resultsWidget, &DbResultsWidget::entrySelectionChanged,
            this, [updateEntryActions](bool) { updateEntryActions(); });

    connect(ui->actionAccept_Selected_Entries, &QAction::triggered, this, [this]() {
        if (closingRequested_) return;
        ui->resultsWidget->acceptSelectedCards();
    });
    connect(ui->actionChange_Color, &QAction::triggered, this, [this]() {
        if (closingRequested_) return;
        ui->resultsWidget->changeSelectedCardColor();
    });
    connect(ui->resultsWidget, &DbResultsWidget::cardColorChanged,
            this, [this](const QString &id) {
        if (closingRequested_) return;
                onCardSelected(id);
                markProjectModified();
                if (ui->dockWidgetCompare->isVisible())
                    ui->peakCompareWidget->refresh();
            });
    connect(ui->actionDelete, &QAction::triggered, this, [this]() {
        if (closingRequested_) return;
        ui->resultsWidget->deleteSelectedCards();
        setStatusMessage(tr("%1 card(s)").arg(ui->resultsWidget->allCards().size()));
    });

    auto buildAndShowPeaks = [this](const QVector<CardType> &cards) {
        if (closingRequested_) return;
        if (context().workspace().session().snapshot()->pfsm) {
            xpdViewer()->setCardPeaks({});
            previewPfsmCandidate(cards.isEmpty() ? QString() : cards.front().getId());
            return;
        }
        QVector<CardPeakData> peaks;
        peaks.reserve(cards.size());
        for (const CardType &card : cards) {
            if (card.getTth().isEmpty()) continue;
            CardPeakData cpd;
            const QVector<double> &pw = xpdViewer()->plotWave;
            cpd.id        = card.getId();
            cpd.color     = cardColor(card.getId(), this);
            cpd.tth       = card.getTth();
            cpd.d         = card.getD();
            cpd.intensityAbsolute = (card.getScale() > 0.0);
            cpd.intensity = card.getScaledIntensity();
            cpd.wave      = pw.isEmpty() ? qualpf::application::defaultWavelength() : pw.first();
            peaks.append(cpd);
        }
        xpdViewer()->setCardPeaks(peaks);
    };

    connect(ui->resultsWidget, &DbResultsWidget::selectedCardsChanged,
            this, buildAndShowPeaks);

    connect(ui->quantWidget, &QuantWidget::selectedPhasesChanged,
            this, buildAndShowPeaks);
}

static QStringList buildActiveRestraints(RestraintsDialog *dlg)
{
    if (!dlg) return {};
    QStringList list;
    if (!dlg->compositionFormula().isEmpty())     list << "Composition";
    if (!dlg->chemicalName().isEmpty())           list << "Chemical name";
    if (!dlg->crystalSystemStrings().isEmpty())   list << "Crystal system";
    if (!dlg->spaceGroupStrings().isEmpty())       list << "Space group";
    if (!dlg->subfilesCodes().isEmpty())           list << "Subfiles";
    if (!dlg->entryIds().isEmpty())                list << "Entry IDs";
    const auto cell = dlg->cellQuery();
    for (double v : cell.values)
        if (v >= 0) { list << "Unit cell"; break; }
    return list;
}

void MainWindow::updateReport()
{
    if (closingRequested_) return;
    if (peakSearchDialog)
        ui->reportWidget->setPeakSearchSettings(peakSearchDialog->settings());
    const QStringList activeR = buildActiveRestraints(m_restraintsDialog);
    ui->reportWidget->setRestraintsInfo(!activeR.isEmpty(), activeR);
    ui->reportWidget->updateReport(context().peaks(), ui->resultsWidget->allCards());
}

void MainWindow::updatePeakListTable()
{
    if (closingRequested_) return;
    if (ui->peakListWidget->isVisible())
        ui->peakListWidget->updatePeakListTable();
    loadExperimentalPeaks(context());
    updateReport();
}

void MainWindow::enableActions(EnabledActions action, bool state)
{
    if (closingRequested_) return;
    //qInfo() << "Enable Actions: " << action << " State: " << state;
    switch (action) {
    case InitAction:  //Init
        ui->actionPFSM->setEnabled(false);
        ui->actionSave_Project->setEnabled(false);
        ui->actionSave_Project_As->setEnabled(false);
        ui->actionExport_Diffraction_Pattern->setEnabled(false);
        ui->actionImage_Powder_Pattern->setEnabled(false);
        ui->actionSearch_Match->setEnabled(false);
        enableMenu(ui->menuPattern, false);
        enableMenu(ui->menuPeakSearch, false);
        enableMenu(ui->menuView, false);
        break;
    case PatternAction:   //Only Pattern
        ui->actionPFSM->setEnabled(true);
        ui->actionSave_Project->setEnabled(true);
        ui->actionSave_Project_As->setEnabled(true);
        ui->actionExport_Diffraction_Pattern->setEnabled(true);
        ui->actionImage_Powder_Pattern->setEnabled(true);
        ui->actionSearch_Match->setEnabled(true);
        enableMenu(ui->menuPattern, true);
        enableMenu(ui->menuPeakSearch, true);
        enableMenu(ui->menuView, true);
        break;
    case DialogOpenAction:
        ui->actionPFSM->setEnabled(false);
        enableMenu(ui->menuFile, false);
        enableMenu(ui->menuPattern, false);
        enableMenu(ui->menuPeakSearch, false);
        ui->actionPFSMOptions->setEnabled(false);
        ui->actionSearch_Match->setEnabled(false);
        break;
    case RunAction:
        ui->actionPFSM->setEnabled(false);
        ui->actionPFSMOptions->setEnabled(false);
        enableMenu(ui->menuFile, false);
        enableMenu(ui->menuPattern, false);
        enableMenu(ui->menuPeakSearch, false);
        enableMenu(ui->menuDatabase, false);
        enableMenu(ui->menuEntry, false);
        ui->actionCancel_Search->setEnabled(true);
        ui->actionSearch_Match->setEnabled(false);
        ui->actionExit->setEnabled(true);
        break;
    case SaveAction:
        saveEnabledActions();
        break;
    case RestoreAction:
        restoreEnabledActions();
        break;
    case PeaksAction: //unused!
        //ui->toolBarPattern
        //qInfo() << "Enable Pattern Menu";
        enableMenu(ui->menuPattern, true);
        enableMenu(ui->menuPeakSearch, true);
        //ui->actionIntervals->setEnabled(false);
        break;
    default:
        break;
    }
    updateBackgroundActions();
}

void MainWindow::enableMenu(QMenu *menu, bool enab)
{
    if (closingRequested_) return;
    QList<QAction*> actions = menu->actions();
    for (int i = 0; i < actions.size(); ++i)
        actions.at(i)->setEnabled(enab);
    QList<QMenu*> menus = menu->findChildren<QMenu*>();
    for (int i = 0; i < menus.size(); ++i) {
        menus.at(i)->setEnabled(enab);
        enableMenu(menus.at(i), enab); //to disable actions on toolbar
    }
    if (menu == ui->menuPattern) { updateFwhmActions(); updateInitialPeakActions(); }
}

void MainWindow::enumerateEnabledActionsMenu(QMenu *menu)
{
    if (closingRequested_) return;
    foreach (QAction *action, menu->actions()) {
        if (action->isSeparator()) {

        } else if (action->menu()) {
            stateActions[action] = action->isEnabled();
            if (action->isEnabled()) enumerateEnabledActionsMenu(action->menu());
        } else {
            stateActions[action] = action->isEnabled();
        }
    }
}

QString MainWindow::getPathDataFiles()
{
    return pathDataFiles;
}

void MainWindow::setPathDataFiles(const QString &newPathDataFiles)
{
    pathDataFiles = newPathDataFiles;
}

void MainWindow::saveEnabledActions()
{
    if (closingRequested_) return;
    stateActions.clear();
    const QList<QAction *> actions = ui->menubar->actions();  //QMenuBar::actions();
    foreach (auto action, actions) {
        stateActions[action] = action->isEnabled();
        if (action->isEnabled()) enumerateEnabledActionsMenu(action->menu());
    }
}

void MainWindow::restoreEnabledActions()
{
    if (closingRequested_) return;
    if (searchTasks_->busy()) return;
    QMutableMapIterator<QAction *, bool> i(stateActions);
    while (i.hasNext()) {
        i.next();
        if(i.key()->menu()) {
            i.key()->menu()->setEnabled(i.value());
        } else {
            i.key()->setEnabled(i.value());
        }
    }
    updateFwhmActions();
    updateInitialPeakActions();
    updateSearchActions();
}

XpdViewWidget *MainWindow::xpdViewer() const
{
    return ui->xpdWidget;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!closingRequested_) {
        const auto title = "Exit " + qApp->applicationDisplayName();
        const auto text = QString("Do you really want \nstop the program %1?").arg(qApp->applicationDisplayName());
        if (QMessageBox::No == QMessageBox::question(this,title,text,QMessageBox::Yes | QMessageBox::No)) { event->ignore(); return; }
        closingRequested_ = true; setProperty("searchState", "closing");
        xpdViewer()->setProperty("workspaceClosing", true);
        qualpf::gui::presentation::unbind_product_plot_refresh(context(), this);
        searchTasks_->close();
        for (auto *action : findChildren<QAction *>()) action->setEnabled(false);
        for (auto *dialog : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
            if (dialog->isVisible() || dialog->property("preprocessingBackend").toString() == "cpp") dialog->reject();
            dialog->setEnabled(false);
        }
        if (searchUiActive_) { QApplication::restoreOverrideCursor(); searchUiActive_ = false; }
        statusProgressBar->hide();
    }
    if (searchTasks_->busy()) { event->ignore(); return; }
    writeSettings(); context().closeDatabases(); event->accept(); QApplication::quit();
}

void MainWindow::writeSettings()
{
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    settings.setValue(QUALX_GEOMETRY_KEY, saveGeometry());
    settings.setValue(QUALX_STATE_KEY, saveState());
    xpdViewer()->pSettings().write(context().preferences());
}

void MainWindow::readSettings()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    const QByteArray geometry = settings.value(QUALX_GEOMETRY_KEY, QByteArray()).toByteArray();
    if (geometry.isEmpty()) {
        const QRect availableGeometry = QGuiApplication::primaryScreen()->availableGeometry();
        resize(availableGeometry.width() * 0.80, availableGeometry.height() * 0.80);
        move((availableGeometry.width() - width()) / 2,
             (availableGeometry.height() - height()) / 2);
        //resizeMyDocks();

    } else {
        restoreGeometry(geometry);
    }

    restoreState(settings.value(QUALX_STATE_KEY).toByteArray());
    ui->peakDockWidget->raise();
    //ui->splitter1->restoreState(settings.value(EXPO_SPLITSIZE_KEY1).toByteArray());
    //ui->splitter2->restoreState(settings.value(EXPO_SPLITSIZE_KEY2).toByteArray());
}

void MainWindow::setStatusMessage(const QString &message)
{
    if (closingRequested_) return;
    statusLabel1->setText(message);
}

void MainWindow::clearStatusMessage()
{
    if (closingRequested_) return;
    statusLabel1->clear();
}

QProgressBar *MainWindow::getStatusProgressBar() const
{
    return statusProgressBar;
}

//
//  File Menu
//

void MainWindow::onActionNewTriggered()
{
    if (closingRequested_) return;
    if (!projectFileSaved) {
        QMessageBox::StandardButton reply = QMessageBox::question(this, tr("New"),
                                                 tr("The current project has unsaved changes.\nDo you want to save it before restarting from scratch?"),
                                                 QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel)
            return;
        if (reply == QMessageBox::Save) {
            onActionSaveProjectTriggered();
            if (!projectFileSaved)
                return;
        }
    }

    // Publish the empty project before refreshing any view. Widget mutation
    // helpers would otherwise publish intermediate states with the old pattern.
    context().searchSession().reset();

    ui->peakCompareWidget->clearAcceptedPhases();
    ui->peakCompareWidget->clearCard();
    ui->peakCompareWidget->setExperimentalPeaks(ExperimentalPeaks());
    ui->reportWidget->clear();
    ui->cardBrowser->clear();
    ui->dockWidgetCard->setWindowTitle(tr("Card"));

    clearProjectFile();
    currentFile.clear();
    setWindowTitle(qApp->applicationDisplayName()+"-"+qApp->applicationVersion());

    enableActions(InitAction);
}

void MainWindow::openRecentFile()
{
    if (closingRequested_) return;
    QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        QString fileIn = action->data().toString();
        if (!context().platform().fileExists(fileIn.toStdString())) {
            QMessageBox::critical(this,"Error",fileIn + " does not exist.");
            return;
        }

        //Extract the fileType from action text and settings
        int index = action->text().section(".",0,0).toInt()-1;
        qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
        settings.beginReadArray(QUALX_RECENT_FILES_KEY);
        settings.setArrayIndex(index);
        QString fileType = settings.value("FileType").toString();
        settings.endArray();

        QMetaEnum metaEnum = QMetaEnum::fromType<RecentFileType>();
        int type = metaEnum.keyToValue(fileType.toStdString().c_str());
        switch (type) {
        case RecentFileType::Input:
            break;

        case RecentFileType::Data:
        {
            loadDiffractionPatterns({fileIn});
            break;
        }
        case RecentFileType::LegacyPowderCif:
            loadPowderCif(fileIn);
            break;
        case RecentFileType::Structure:
            break;

        case RecentFileType::Project:
            loadProject(fileIn);
            break;
        }
    }
}

bool MainWindow::loadDiffractionPatterns(QStringList files, bool chooseRadiation, bool legacyCif)
{
    if (files.isEmpty()) return false;
    // Preserve the public dedicated-powder selector used by existing callers.
    if (legacyCif) {
        if (files.size() != 1) { setStatusMessage(tr("Select one powder CIF source.")); return false; }
        return loadPowderCif(files.front());
    }
    auto source = context().searchSession().snapshot();
    std::vector<qualpf::Pattern> prepared;
    std::optional<qualpf::application::OriginalSignalProvenance> original;
    bool synchrotron = false;
    for (const auto &file : files) {
        const auto path = QFileInfo(file).absoluteFilePath();
        auto result = context().files().patterns.readImported(path.toStdString());
        if (!result) {
            setStatusMessage(tr("Cannot open %1: %2").arg(file, QString::fromStdString(result.error().message)));
            return false;
        }
        auto imported = std::move(result).value();
        if (prepared.empty()) original = std::move(imported.original_signal_provenance);
        auto read = std::move(imported.input);
        if (!read.file_wavelength && chooseRadiation) {
            WaveDialog dialog(this);
            if (dialog.exec() != QDialog::Accepted) return false;
            if (prepared.empty()) synchrotron = dialog.radiationType() == 1;
            read.pattern.radiation.kind = dialog.radiationType() == 2 ? qualpf::RadiationKind::neutron : qualpf::RadiationKind::x_ray;
            read.pattern.radiation.lines = {{qualpf::WavelengthAngstrom{dialog.wave1()}, 1}};
            if (dialog.nWaves() > 1)
                read.pattern.radiation.lines.push_back({qualpf::WavelengthAngstrom{dialog.wave2()}, dialog.ratio()});
        }
        prepared.push_back(std::move(read.pattern));
    }
    auto first = std::move(prepared.front());
    prepared.erase(prepared.begin());
    prepared = qualpf::application::prepareComparisonPatterns(std::move(prepared));
    auto staged = *source;
    staged.pattern = first;
    staged.comparisonPatterns = prepared;
    auto presentation = qualpf::gui::presentation::project_product_refresh(staged, context().presentation(),
        {1, qualpf::gui::QtPlotSink::abscissa(*xpdViewer())});
    if (!presentation) {
        setStatusMessage(QString::fromStdString(presentation.error().reason + ": " + presentation.error().message));
        return false;
    }
    auto committed = context().searchSession().openPattern(source, std::move(first), std::move(prepared), synchrotron, std::move(original));
    if (!committed) {
        setStatusMessage(QString::fromStdString(committed.error().message));
        return false;
    }
    clearProjectFile();
    refreshPatternPresentation();
    refreshPreprocessingDisplay();
    const auto filename = QFileInfo(files.front()).absoluteFilePath();
    qualpf::gui::Preferences(qualpf::gui::workspaceContext(this).preferences()).setValue(DEFAULT_DIR_KEY, QFileInfo(filename).absolutePath());
    setCurrentFile(filename, QVariant::fromValue(RecentFileType::Data).toString());
    markProjectModified();
    updateReport();
    enableActions(PatternAction);
    clearStatusMessage();
    return true;
}

bool MainWindow::loadPowderCif(const QString &file)
{
    using namespace qualpf::application; using namespace qualpf::gui;
    using namespace qualpf::io::powder_cif;
    PowderCifImportService service(context().searchSession(), context().files().powder);
    auto draft = service.prepare(file);
    const auto showError = [this, &file](const ImportError &error) {
        setStatusMessage(tr("Cannot open %1: %2").arg(file,
            QString::fromStdString(error.error.reason + ": " + error.error.message)));
    };
    if (auto error = std::get_if<ImportError>(&draft)) { showError(*error); return false; }
    const auto &prepared = std::get<PowderCifImportDraft>(draft);
    std::optional<UserSuppliedRadiation> supplied;
    bool synchrotron = false;
    if (prepared.preparation.selected().wavelength_state != WavelengthState::SOURCE_WAVELENGTH) {
        WaveDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted) return false;
        // Existing GUI transport, explicitly accepted input; never source data.
        supplied = UserSuppliedRadiation{std::bit_cast<std::uint32_t>(float(dialog.wave1())),
            dialog.radiationType() == 2 ? UserSuppliedRadiation::Kind::neutron : UserSuppliedRadiation::Kind::x_ray};
        synchrotron = dialog.radiationType() == 1;
    }
    auto complete = service.complete(prepared, supplied, synchrotron);
    if (auto error = std::get_if<ImportError>(&complete)) { showError(*error); return false; }
    auto committed = service.commit(std::move(std::get<PreparedPowderCifImport>(complete)),
        [this](const SessionStateView &next) {
            auto frames = qualpf::gui::presentation::project_product_refresh(next, context().presentation(),
                {1, qualpf::gui::QtPlotSink::abscissa(*xpdViewer())});
            if (!frames) return qualpf::Status{qualpf::Error{qualpf::ErrorCode::invalid_input,"pattern.cif.presentation",frames.error().message,frames.error().reason}};
            qualpf::gui::QtPlotSink sink(xpdViewer());
            for (const auto &frame : frames.value()) {
                auto valid = sink.validate(frame.update);
                if (!valid) return qualpf::Status{qualpf::Error{qualpf::ErrorCode::invalid_input,"pattern.cif.presentation",valid.error().message,valid.error().reason}};
            }
            return qualpf::Status{std::monostate{}};
        },
        [this, &file](const SessionViewHandle &) {
            clearProjectFile();
            refreshPatternPresentation();
            refreshPreprocessingDisplay();
            const auto filename = QFileInfo(file).absoluteFilePath();
            qualpf::gui::Preferences(qualpf::gui::workspaceContext(this).preferences()).setValue(DEFAULT_DIR_KEY, QFileInfo(filename).absolutePath());
            setCurrentFile(filename, QVariant::fromValue(RecentFileType::LegacyPowderCif).toString());
            markProjectModified();
            updateReport();
            enableActions(PatternAction);
            clearStatusMessage();
        });
    if (!committed) { setStatusMessage(QString::fromStdString(committed.error().message)); return false; }
    return true;
}

void MainWindow::onActionImportDiffractionPatternTriggered()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = "All Powder Diffraction Data (*.dat *.xy *.gda *.xye *.xrdml *.pow)";
    QString exts = "All files (*.*);;"
                   "All Powder Diffraction Data (*.dat *.xy *.gda *.xye *.xrdml *.pow);;"
                   "ASCII profile [start,step,end,intensities] (*.dat *.pow);;"
                   "XY profile [2theta and intensities in two colomuns] (*.xy *.pow);;"
                   "GSAS data (*.gda);;"
                   "CCDC Mercury data (*.xye);;"
                   "Siemens data (*.uxd);;"
                   "Sietronics Sieray data (*.cpi);;"
                   "XDD data (*.xdd);;"
                   "DBWS data (*.dbw);;"
                   "XDA data (*.xda);;"
                   "Philips UDF data (*.udf);;"
                   "PANalytical XRDML data (*.xrdml)";

    QStringList files = QFileDialog::getOpenFileNames(this,
                                                      tr("Import Diffraction Pattern From"),
                                                      settings.value(DEFAULT_DIR_KEY).toString(),
                                                      exts, &selectedFilter);

    loadDiffractionPatterns(files);
}

bool MainWindow::loadProject(QString fileName)
{
    if (closingRequested_) return false;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    if (fileName.isEmpty()) return false;

    fileName = QDir::toNativeSeparators(fileName);
    QScopedValueRollback<bool> restoring(restoringProjectView_, true);
    const auto source = context().searchSession().snapshot();
    if (source->editing) { setStatusMessage(tr("Finish the preprocessing edit before loading a project.")); return false; }
    const auto committed=qualpf::application::openProjectInput(context().workspace(),context().files().projects,fileName.toStdString());
    if (!committed) {
        const auto &error=committed.error();
        if (error.reason=="project_read_failed") QMessageBox::warning(this,tr("Open Project"),tr("Cannot read file:\n%1").arg(fileName));
        else if (error.reason=="project_parse_failed") QMessageBox::warning(this,tr("Open Project"),tr("Invalid project file:\n%1").arg(QString::fromStdString(error.message)));
        else if (error.reason=="project_warning") QMessageBox::warning(this,tr("Open Project"),QString::fromStdString(error.message));
        else setStatusMessage(QString::fromStdString(error.message));
        return false;
    }
    const auto restoredView=context().workspace().projectView();
    context().presentation().setBackgroundSubtraction(restoredView->subtract_background);
    for(const auto &[layer,key]:qualpf::gui::projectLayers)
        if(const auto found=restoredView->layers.find(key);found!=restoredView->layers.end())
            context().presentation().setVisibility(layer,qualpf::gui::projectVisibility(found->second));
    // Rebuild the canonical graph items before applying styles and offsets.
    // Old item indexes belong to the preceding canvas until that rebuild finishes.
    if (!context().searchSession().snapshot()->pattern.two_theta.empty())
        refreshPatternPresentation();
    else
        xpdViewer()->clearProjectPlot();
    refreshPreprocessingDisplay();
    m_restraintsDialog->restore(context().searchSession().snapshot()->restraints);
    ui->peakCompareWidget->clearCard();
    updateReport();
    ui->reportWidget->updateQuantitative(ui->quantWidget->quantificationResult());
    ui->resultsWidget->applyProjectView(*restoredView);
    xpdViewer()->applyProjectView(*restoredView);
    ui->actionLegend->setChecked(restoredView->legend);

    currentProjectFile = fileName;
    projectFileSaved = true;
    settings.setValue(DEFAULT_DIR_KEY, QFileInfo(fileName).absolutePath());
    setCurrentFile(fileName, QVariant::fromValue(RecentFileType::Project).toString());
    setStatusMessage(tr("Project loaded: %1").arg(QFileInfo(fileName).fileName()));

    enableActions(PatternAction);
    return true;
}

void MainWindow::onActionLoadProjectTriggered()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = APP_DISPLAY_NAME + " Project (*.qpf)";
    QString exts = selectedFilter;

    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Open Project"), settings.value(DEFAULT_DIR_KEY).toString(),
                                                    exts, &selectedFilter);
    loadProject(fileName);
}

void MainWindow::saveProject(const QString &fileName)
{
    if (closingRequested_) return;
    const auto snapshot = context().searchSession().snapshot();
    if (snapshot->editing) { setStatusMessage(tr("Finish the preprocessing edit before saving a project.")); return; }
    captureProjectView();
    const auto saved=qualpf::application::saveProjectInput(context().workspace(), context().files().projectWriter, fileName.toStdString());
    if (!saved && saved.error().reason != "project_open_failed" && saved.error().reason != "project_save_failed") {
        QMessageBox::warning(this,tr("Save Project"),QString::fromStdString(saved.error().message)); return;
    }
    if (!saved) {
        const auto message=saved.error().reason=="project_open_failed" ? tr("Cannot write file:\n%1") : tr("Cannot save file:\n%1");
        QMessageBox::warning(this,tr("Save Project"),message.arg(fileName)); return;
    }
    projectFileSaved = true;
    currentProjectFile = fileName;
    setCurrentFile(fileName, QVariant::fromValue(RecentFileType::Project).toString());
}

void MainWindow::onActionSaveProjectTriggered()
{
    if (closingRequested_) return;
    if (currentProjectFile.isEmpty())
        onActionSaveProjectAsTriggered();
    else
        saveProject(currentProjectFile);
}

void MainWindow::onActionSaveProjectAsTriggered()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = "expo";
    QString exts = APP_DISPLAY_NAME + " Project (*.qpf)";
    QString fileName = SaveDialog::run(this,tr("Save Project As"),
                                       settings.value(DEFAULT_DIR_KEY).toString(),
                                       QFileInfo(currentFile).baseName(),exts,
                                       "qpf",selectedFilter);
    if (!fileName.isEmpty()) {
        fileName = QDir::toNativeSeparators(fileName);
        saveProject(fileName);
    }
}

void MainWindow::onActionExportDiffractionPattern()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = "xy";
    int code = 0;
    QString exts = "XY files (*.xy);; CIF files (*.cif)";
    QString fileName = SaveDialog::run(this,
                                       tr("Export Data As"),
                                       settings.value(DEFAULT_DIR_KEY).toString(),
                                       QFileInfo(currentFile).baseName(),
                                       exts,
                                       "xy",
                                       selectedFilter,
                                       &code);
    if (!fileName.isEmpty()) {
        settings.setValue(DEFAULT_DIR_KEY,QFileInfo(fileName).absolutePath());
        if (selectedFilter.contains("cif", Qt::CaseInsensitive)) {
            // Export as CIF
            { // Qualified canonical C++ export owner.
                const auto result = qualpf::gui::exporting::PatternExportService(context().searchSession().snapshot(), context().files().exports)
                    .save(fileName, qualpf::exporting::PatternFormat::powder_cif);
                if (!result) {
                    if (result.error().reason == "incomplete_background")
                        setStatusMessage(tr("CIF export requires a complete aligned background. Use XY for this partial snapshot."));
                    else
                        setStatusMessage(QString::fromStdString(result.error().reason + ": " + result.error().message));
                } else {
                    refreshPatternPresentation();
                    refreshPreprocessingDisplay();
                }
                return;
            }
        } else {
            // Export as XY
            { // Qualified canonical C++ export owner.
                const auto result = qualpf::gui::exporting::PatternExportService(context().searchSession().snapshot(), context().files().exports)
                    .save(fileName, qualpf::exporting::PatternFormat::xy_observed);
                if (!result)
                    setStatusMessage(QString::fromStdString(result.error().reason + ": " + result.error().message));
                return;
            }
        }
    }
}

void MainWindow::onActionImagePowderPatternTriggered()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = settings.value(QUALX_IMAGE_FILTER_KEY).toString();
    int code = 0;
    QString exts = "PNG (*.png);;"
                   "PDF (*.pdf);;"
                   "BMP (*.bmp);;"
                   "JPEG (*.jpeg *jpg *jpe)";
    QString fileName = SaveDialog::run(this,
                                       tr("Export Image As"),
                                       settings.value(DEFAULT_DIR_KEY).toString(),
                                       QFileInfo(currentFile).baseName(),
                                       exts,
                                       "png",
                                       selectedFilter,
                                       &code);
    settings.setValue(QUALX_IMAGE_FILTER_KEY,selectedFilter);
    if (!fileName.isEmpty()) {
        ImgSettingsDialog dialog(this, QFileInfo(fileName).suffix());
        if (dialog.exec()) {
            settings.setValue(DEFAULT_DIR_KEY,QFileInfo(fileName).absolutePath());
            if (dialog.isTransparent()) {
                // remove background for transparency
                xpdViewer()->setBackground(QBrush(Qt::NoBrush));
                xpdViewer()->axisRect()->setBackground(QBrush(Qt::NoBrush));
            }
            if( fileName.endsWith(".png") ){
                xpdViewer()->savePng( fileName, xpdViewer()->width(), xpdViewer()->height(), dialog.scale() );
            }
            if( fileName.endsWith(".jpg") || fileName.endsWith(".jpeg") || fileName.endsWith(".jpe")){
                xpdViewer()->saveJpg( fileName, xpdViewer()->width(), xpdViewer()->height(), dialog.scale() );
            }
            if( fileName.endsWith(".pdf") ){
                xpdViewer()->savePdf( fileName, xpdViewer()->width(), xpdViewer()->height());
            }
            if (dialog.isTransparent()) {
                // add background
                xpdViewer()->pSettings().applyToBackground(xpdViewer());
                xpdViewer()->pSettings().applyToLayer(xpdViewer());
            }
        }
    }
}

void MainWindow::clearProjectFile()
{
    if (closingRequested_) return;
    projectFileSaved = true;
    currentProjectFile.clear();
    context().workspace().resetProjectPresentation();
}

void MainWindow::markProjectModified()
{
    if (closingRequested_) return;
    projectFileSaved = false;
}

void MainWindow::createRecentActions()
{
    if (closingRequested_) return;
    QAction* recentFileAction;
    for(int i = 0; i < maxFileNr; i++) {
        recentFileAction = new QAction(this);
        recentFileAction->setVisible(false);
        connect(recentFileAction,SIGNAL(triggered()),this,SLOT(openRecentFile()));
        //ui->menuRecent->addAction(recentFileAction);
        ui->menuRecent->addAction(recentFileAction);
        recentFileActionList.append(recentFileAction);
    }

    updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());

    QVector<RecentFileInfo> recentFiles;
    int size = settings.beginReadArray(QUALX_RECENT_FILES_KEY);
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);
        RecentFileInfo info;
        info.fileName = settings.value("FileName").toString();
        info.fileType = settings.value("FileType").toString();
        recentFiles.append(info);
    }
    settings.endArray();

    int numRecentFiles = qMin(recentFiles.size(),static_cast<int>(maxFileNr));

    for(int i = 0; i < numRecentFiles; ++i) {
        recentFileActionList.at(i)->setText(QString::number(i+1)+". "+recentFiles.at(i).fileName);
        recentFileActionList.at(i)->setData(recentFiles.at(i).fileName);
        recentFileActionList.at(i)->setVisible(true);
    }

    for(int i = numRecentFiles; i < maxFileNr; i++) {
        recentFileActionList.at(i)->setVisible(false);
    }
}

void MainWindow::setRecentFiles(const QString &fullFileName, const QString &fileType)
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QVector<RecentFileInfo> recentFiles;
    int size = settings.beginReadArray(QUALX_RECENT_FILES_KEY);
    for (int i = 0; i < size; i++) {
        settings.setArrayIndex(i);
        RecentFileInfo info;
        info.fileName = settings.value("FileName").toString();
        info.fileType = settings.value("FileType").toString();
        if (info.fileName != fullFileName) {
            recentFiles.append(info);
        }
    }
    settings.endArray();
    recentFiles.prepend({fullFileName,fileType});
    while (recentFiles.size() > maxFileNr)
        recentFiles.removeLast();

    settings.beginWriteArray(QUALX_RECENT_FILES_KEY);
    for (int i = 0; i < recentFiles.size(); i++) {
        settings.setArrayIndex(i);
        settings.setValue("FileName", recentFiles.at(i).fileName);
        settings.setValue("FileType", recentFiles.at(i).fileType);
    }
    settings.endArray();
    //

    // if you have several istances of Mainwindow present,
    // call updateRecentFileActions on all top-level windows and
    // all recent files menu will be equal
    foreach (QWidget *widget, QApplication::topLevelWidgets()) {
        QMainWindow *mainwindow = qobject_cast<QMainWindow *>(widget);
        if (mainwindow)
            updateRecentFileActions();
    }
}

void MainWindow::setCurrentFile(const QString &fullFileName, const QString &fileType)
{
    if (closingRequested_) return;
    if (!fullFileName.isEmpty()) {
        currentFile = fullFileName;
        setWindowTitle(currentFile);

        setRecentFiles(fullFileName, fileType);
    }
}

void MainWindow::onActionFileDropped(const QStringList &fileList)
{
    if (closingRequested_) return;
    QStringList xpdExtensions = {"dat","xy","rtv","gda","xye","xrdml","pow"};

    for (int i = 0; i < fileList.size(); i++) {
        QString fileIn = fileList.at(i);
        QFileInfo fi(fileIn);
        QString ext = fi.suffix().toLower();
        if (xpdExtensions.contains(ext)) {
            loadDiffractionPatterns(fileList);
            break;
        } else if (ext == "qpf" || ext == "qxp") {
            loadProject(fileIn);
            break;
        } else {
            QMessageBox::critical(this,"Error",fileIn + " is not a valid file type.");
        }
    }
}

static QString modelToHtml(QAbstractItemModel *model,
                           const QString &title,
                           const QList<int> &skipCols = {})
{
    QString html =
        "<html><head><style>"
        "body{font-family:sans-serif;font-size:10pt;}"
        "h2{color:#003366;}"
        "table{border-collapse:collapse;width:100%;}"
        "th{background:#003366;color:white;padding:4px 8px;text-align:left;}"
        "td{border:1px solid #ccc;padding:3px 8px;}"
        "tr:nth-child(even){background:#f0f4ff;}"
        "</style></head><body>";
    html += "<h2>" + title.toHtmlEscaped() + "</h2><table><tr>";

    for (int c = 0; c < model->columnCount(); ++c) {
        if (skipCols.contains(c)) continue;
        html += "<th>" + model->headerData(c, Qt::Horizontal).toString().toHtmlEscaped() + "</th>";
    }
    html += "</tr>";

    for (int r = 0; r < model->rowCount(); ++r) {
        html += "<tr>";
        for (int c = 0; c < model->columnCount(); ++c) {
            if (skipCols.contains(c)) continue;
            html += "<td>" + model->data(model->index(r, c)).toString().toHtmlEscaped() + "</td>";
        }
        html += "</tr>";
    }

    html += "</table></body></html>";
    return html;
}

static void printHtml(QPrinter *printer, const QString &html)
{
    QTextDocument doc;
    doc.setHtml(html);
    doc.print(printer);
}

static void printWidget(QPrinter *printer, QWidget *widget)
{
    QPixmap pix = widget->grab();
    QPainter painter(printer);
    QRect pr = printer->pageLayout().paintRectPixels(printer->resolution());
    QSize scaled = pix.size().scaled(pr.size(), Qt::KeepAspectRatio);
    QRect target(pr.topLeft(), scaled);
    painter.drawPixmap(target, pix);
}

void MainWindow::onActionPrintTriggered()
{
    if (closingRequested_) return;
    PrintDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted)
        return;

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDlg(&printer, this);
    if (printDlg.exec() != QDialog::Accepted)
        return;

    switch (dlg.selectedArea()) {

    case PrintDialog::Report:
        ui->reportWidget->print(&printer);
        break;

    case PrintDialog::ResultList:
        printHtml(&printer,
                  modelToHtml(ui->resultsWidget->sourceModel,
                              tr("Result List"),
                              {0}));   // skip checkbox column
        break;

    case PrintDialog::PeakList:
        printHtml(&printer,
                  modelToHtml(ui->peakListWidget->peakListModel,
                              tr("Peak List")));
        break;

    case PrintDialog::Pattern: {
        QCPPainter cpPainter(&printer);
        QRect pr = printer.pageLayout().paintRectPixels(printer.resolution());
        ui->xpdWidget->toPainter(&cpPainter, pr.width(), pr.height());
        break;
    }

    case PrintDialog::Card:
        ui->cardBrowser->print(&printer);
        break;

    case PrintDialog::Quantitative:
        printWidget(&printer, ui->quantWidget);
        break;

    case PrintDialog::Compare:
        printHtml(&printer,
                  modelToHtml(ui->peakCompareWidget->m_model,
                              tr("Compare")));
        break;
    }
}

//
//  Pattern Menu
//

void MainWindow::onActionRangeTriggered()
{
    if (closingRequested_ || searchTasks_->busy()) return;
    if (rangeDialog && !rangeDialog->isClosed()) { rangeDialog->raise();rangeDialog->activateWindow();return; }
    if (context().searchSession().snapshot()->editing) {
        setStatusMessage(tr("Finish the preprocessing edit before opening Range."));return;
    }
    if (context().searchSession().snapshot()->pattern.two_theta.empty()) return;
    rangeDialog = new RangeDialog(this);
    rangeDialog->setAttribute(Qt::WA_DeleteOnClose);
    rangeDialog->setRange(xpdViewer());
    rangeDialog->show();
}

void MainWindow::onActionBackgroundTriggered()
{
    if (closingRequested_) return;
    if (searchTasks_->busy() || context().searchSession().snapshot()->editing) return;
    if (!confirmWorkflowReplacement(qualpf::application::WorkflowStep::background)) return;
    if (context().workspace().session().snapshot()->projectSettings) { runWorkflowPreparation(true); return; }
    auto result = qualpf::gui::applyPreprocessing(context().searchSession(), qualpf::application::PreprocessingAction::generate_background);
    if (!result) { setStatusMessage(QString::fromStdString(result.error().reason + ": " + result.error().message)); return; }
    setBackgroundSubtractionDisplay(false);
    refreshPatternPresentation(); refreshPreprocessingDisplay(); updateBackgroundActions();
}

void MainWindow::onActionBackgroundOptionsTriggered() {
    if (closingRequested_ || searchTasks_->busy() || context().searchSession().snapshot()->editing ||
        context().searchSession().snapshot()->pattern.observed.empty()) return;
    setBackgroundSubtractionDisplay(false);
    backgroundDialog->setBackground();
    backgroundDialog->show();
}

void MainWindow::onActionBackgroundExportTriggered()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = "xy";
    int code = 0;
    QString exts = "XY files (*.xy);; CIF files (*.cif)";
    QString fileName = SaveDialog::run(this,
                                       tr("Export Background As"),
                                       settings.value(DEFAULT_DIR_KEY).toString(),
                                       QFileInfo(currentFile).baseName(),
                                       exts,
                                       "xy",
                                       selectedFilter,
                                       &code);
    if (!fileName.isEmpty()) {
        settings.setValue(DEFAULT_DIR_KEY,QFileInfo(fileName).absolutePath());
        if (selectedFilter.contains("cif", Qt::CaseInsensitive)) {
            // Export Background as CIF
            { // Qualified canonical C++ export owner.
                setStatusMessage(tr("Background-only CIF export is unavailable. Select XY to export the aligned background."));
                return;
            }
        } else {
            // Export Background as XY
            { // Qualified canonical C++ export owner.
                const auto result = qualpf::gui::exporting::PatternExportService(context().searchSession().snapshot(), context().files().exports)
                    .save(fileName, qualpf::exporting::PatternFormat::xy_background);
                if (!result)
                    setStatusMessage(QString::fromStdString(result.error().reason + ": " + result.error().message));
                return;
            }
        }
    }
}

void MainWindow::onActionSubtractBackgroundTriggered() {
    if (closingRequested_) return;
    setBackgroundSubtractionDisplay(!context().presentation().snapshot().subtract_background);
    captureProjectView(); markProjectModified();
}
void MainWindow::updateBackgroundActions() {
    if (closingRequested_) return;
    const auto source = context().workspace().session().snapshot();
    const bool aligned = qualpf::application::presentation::can_subtract_background(*source);
    if (!aligned) context().presentation().setBackgroundSubtraction(false);
    const QSignalBlocker blocker(ui->actionSubtract_Background);
    ui->actionSubtract_Background->setChecked(context().presentation().snapshot().subtract_background);
    ui->actionSubtract_Background->setEnabled(aligned && !source->editing);
}
void MainWindow::setBackgroundSubtractionDisplay(bool enabled) {
    if (closingRequested_) return;
    if (enabled && !qualpf::application::presentation::can_subtract_background(*context().workspace().session().snapshot())) enabled = false;
    const bool changed = context().presentation().snapshot().subtract_background != enabled;
    context().presentation().setBackgroundSubtraction(enabled);
    updateBackgroundActions();
    if (!changed) return;
    const auto source = context().workspace().session().snapshot();
    if (source->pfsm && displayedPfsmPlot_ && displayedPfsmSource_.lock() == source) showPfsmPlot(*displayedPfsmPlot_);
    else { refreshPatternPresentation(); refreshPreprocessingDisplay(); }
}

void MainWindow::onActionSmoothingTriggered()
{
    if (closingRequested_ || searchTasks_->busy() || context().searchSession().snapshot()->editing || context().searchSession().snapshot()->pattern.observed.empty()) return;
    smoothingDialog->setSmoothing();
    smoothingDialog->show();
}

void MainWindow::onActionKAlpha2StrippingTriggered()
{
    if (closingRequested_) return;
    if (searchTasks_->busy() || context().workspace().session().snapshot()->editing ||
        !confirmWorkflowReplacement(qualpf::application::WorkflowStep::kalpha2)) return;
    auto status = qualpf::application::stripKalpha2Operation(context().workspace().session(), context().platform());
    if (!status) {
        if (status.error().reason == "invalid_kalpha2_backend")
            setStatusMessage(tr("Unknown K-alpha2 backend. Choose cpp or legacy."));
        else
            setStatusMessage(QString::fromStdString(status.error().reason + ": " + status.error().message));
        return;
    }
    refreshPatternPresentation();
    refreshPreprocessingDisplay();
    updateReport();
}

void MainWindow::deleteSelectedPeaks(const QVector<int> &selected)
{
    if (closingRequested_) return;
    {
        auto allowed = qualpf::gui::peakEditEntryStatus(qualpf::gui::workspaceContext(this));
        if (!allowed) {setStatusMessage(QString::fromStdString(allowed.error().reason));return;}
    }
    if (!confirmWorkflowReplacement(qualpf::application::WorkflowStep::peaks)) return;
    {
        qualpf::gui::PreprocessingEdit edit(context().searchSession());
        qualpf::application::PreprocessingRequest request;
        request.manual = qualpf::application::ManualPeakChange{};
        request.manual->operation = qualpf::application::ManualPeakChange::Operation::erase;
        for (int row : selected) request.manual->rows.push_back(std::size_t(row));
        request.tolerance = qualpf::AngularWidthDegrees{context().searchSession().snapshot()->peakTolerance.value_or(.15)};
        auto status = edit.preview(request);
        if (status) status = edit.commit();
        if (!status) setStatusMessage(QString::fromStdString(status.error().message));
        else { refreshPatternPresentation(); refreshPreprocessingDisplay(); }
        return;
    }
}

void MainWindow::addDeleteSelectedPoint(int action, double xp, double yp, int &ier)
{
    if (closingRequested_) return;
    if ((action == AddBackgroundPoint || action == DeleteBackgroundPoint)) {
        if (!confirmWorkflowReplacement(qualpf::application::WorkflowStep::background)) { ier = 1; return; }
        setBackgroundSubtractionDisplay(false);
        const auto state = context().searchSession().snapshot();
        const auto wave = state->pattern.radiation.lines.empty() ? std::optional<qualpf::WavelengthAngstrom>{} :
            std::optional{state->pattern.radiation.lines.front().wavelength};
        auto coordinate = qualpf::gui::backgroundCanonicalCoordinate(
            xp, xpdViewer()->pSettings().getAbscissa(), wave);
        auto result = coordinate ? qualpf::gui::editBackgroundPoint(
            context().searchSession(), action == AddBackgroundPoint, coordinate.value(), yp,
            xpdViewer()->peaks.visible) : qualpf::Status{coordinate.error()};
        ier = result ? 0 : 1;
        setProperty("backgroundErrorField", result ? QString() : QString::fromStdString(result.error().field));
        setProperty("backgroundErrorReason", result ? QString() : QString::fromStdString(result.error().reason));
        if (!result) {
            setStatusMessage(QString::fromStdString(result.error().reason + ": " + result.error().message));
            return;
        }
        refreshPatternPresentation();
        refreshPreprocessingDisplay();
        markProjectModified();
        return;
    }
    if ((action == AddPeak || action == AddInferredPeak || action == DeletePeak)) {
        const auto state = context().searchSession().snapshot();
        const auto wave = state->pattern.radiation.lines.empty() ? std::optional<qualpf::WavelengthAngstrom>{} :
            std::optional{state->pattern.radiation.lines.front().wavelength};
        auto coordinate = qualpf::gui::peakCanonicalCoordinate(xp,xpdViewer()->pSettings().getAbscissa(),wave);
        auto entry = qualpf::gui::peakEditEntryStatus(qualpf::gui::workspaceContext(this));
        ier = 1;
        const qualpf::Error *error = !coordinate ? &coordinate.error() : !entry ? &entry.error() : nullptr;
        setProperty("peakErrorField",error?QString::fromStdString(error->field):QString());
        setProperty("peakErrorReason",error?QString::fromStdString(error->reason):QString());
        if (error) {setStatusMessage(QString::fromStdString(error->reason+": "+error->message));return;}
        if (action == AddInferredPeak && !confirmWorkflowReplacement(qualpf::application::WorkflowStep::peaks)) return;
        xp = coordinate.value();
        if (action == AddPeak) ier = qualpf::gui::editManualPeak(*this,{},xp)?0:1;
        else if (action == AddInferredPeak) ier = qualpf::gui::editInferredPeak(*this,xp)?0:1;
        else {
            const auto &positions = state->peaks.tth;
            if (positions.empty()) {
                setProperty("peakErrorField","peaks.manual");
                setProperty("peakErrorReason","no_selected_peaks");
                return;
            }
            auto at = std::min_element(positions.begin(),positions.end(),[xp](double a,double b) {
                return std::abs(a-xp)<std::abs(b-xp);
            });
            deleteSelectedPeaks({int(at-positions.begin())});
            ier = context().searchSession().snapshot()!=state?0:1;
        }
    } else {
        ier = 1;
    }
}

void MainWindow::clearPatternPresentation()
{
    displayedPfsmPlot_.reset();
    displayedPfsmSource_.reset();
    pfsmPreviewCandidate_.clear();
    pfsmPreviewPending_ = false;
    xpdViewer()->clearProjectPlot();
    ui->peakListWidget->updatePeakListTable();
}

void MainWindow::refreshPreprocessingDisplay()
{
    if (closingRequested_) return;
    refreshInitialPeakCandidates();
    updateBackgroundActions();
    if (context().workspace().session().snapshot()->pfsm) return;
    if (!context().presentation().redrawEnabled()) return;
    using namespace qualpf::application::presentation; using namespace qualpf::gui::presentation;
    const auto state = context().searchSession().snapshot();
    auto *plot = xpdViewer();
    if (state->pattern.two_theta.empty()) {
        clearPatternPresentation();
        return;
    }
    auto projected = project_session_update(make_plot_input(*state, context().presentation(), 0),
        {0, qualpf::gui::QtPlotSink::abscissa(*plot)});
    if (!projected) { setStatusMessage(QString::fromStdString(projected.error().reason + ": " + projected.error().message)); return; }
    qualpf::gui::QtPlotSink sink(plot);
    auto &update = projected.value();
    auto checked = sink.validate(update);
    if (!checked) { setStatusMessage(QString::fromStdString(checked.error().reason + ": " + checked.error().message)); return; }
    plot->clearSelectedPeaks();
    if (!update.commands.empty()) sink.accept(update.commands.front());
    ui->peakListWidget->updatePeakListTable();
    for (std::size_t i = 1; i < update.commands.size(); ++i) sink.accept(update.commands[i]);
}

bool MainWindow::refreshPatternPresentation()
{
    if (closingRequested_) return false;
    const auto source = context().workspace().session().snapshot();
    updateBackgroundActions();
    if (source->pfsm) {
        auto plot = qualpf::application::preparePfsmPlot(source, {}, {});
        if (!plot) { setStatusMessage(QString::fromStdString(plot.error().message)); return false; }
        showPfsmPlot(plot.value()); return true;
    }
    if (!context().presentation().redrawEnabled()) return true;
    if (source->pattern.two_theta.empty()) {
        clearPatternPresentation();
        return true;
    }
    using namespace qualpf::application::presentation; using namespace qualpf::gui::presentation;
    auto prepared = project_product_refresh(*context().searchSession().snapshot(), context().presentation(),
        {1, qualpf::gui::QtPlotSink::abscissa(*xpdViewer())});
    if (!prepared) { setStatusMessage(QString::fromStdString(prepared.error().reason + ": " + prepared.error().message)); return false; }
    qualpf::gui::QtPlotSink sink(xpdViewer());
    for (const auto &frame : prepared.value()) {
        auto status = sink.validate(frame.update);
        if (!status) { setStatusMessage(QString::fromStdString(status.error().reason + ": " + status.error().message)); return false; }
    }
    for (const auto &frame : prepared.value()) {
        if (frame.action == 5) {
            const auto state = context().searchSession().snapshot();
            const auto warning = qualpf::gui::peakRejectionSummary(state->preprocessing);
            setStatusMessage(tr("Number of peaks: ") + QString::number(state->peaks.tth.size()) +
                (warning.isEmpty() ? QString() : "; " + warning));
            updatePeakListTable();
        }
        context().presentation().applyPlotAction(frame.action);
        auto status = sink.apply(frame.update);
        if (!status) { setStatusMessage(QString::fromStdString(status.error().reason + ": " + status.error().message)); return false; }
    }
    refreshInitialPeakCandidates();
    return true;
}

void MainWindow::onActionPeakSearchTriggered()
{
    if (closingRequested_) return;
    if (!confirmWorkflowReplacement(qualpf::application::WorkflowStep::peaks)) return;
    {
        auto allowed = qualpf::gui::peakEditEntryStatus(qualpf::gui::workspaceContext(this));
        if (!allowed) {setStatusMessage(QString::fromStdString(allowed.error().reason));return;}
    }
    ui->peakDockWidget->show();
    ui->peakDockWidget->raise();
    {
        if (context().workspace().session().snapshot()->projectSettings) { runWorkflowPreparation(false); return; }
        auto result=qualpf::gui::applyPreprocessing(context().searchSession(),qualpf::application::PreprocessingAction::automatic_peaks);
        if (!result) { setStatusMessage(QString::fromStdString(result.error().reason+": "+result.error().message)); return; }
        refreshPatternPresentation();
        refreshPreprocessingDisplay();
    }
    updateReport();
}

void MainWindow::onActionPeakSearchConditionsTriggered()
{
    if (closingRequested_) return;
    {
        auto allowed = qualpf::gui::peakEditEntryStatus(qualpf::gui::workspaceContext(this));
        if (!allowed) {setStatusMessage(QString::fromStdString(allowed.error().reason));return;}
    }
    ui->peakDockWidget->show();
    ui->peakDockWidget->raise();
    peakSearchDialog->setOptions();
    peakSearchDialog->show();
}

void MainWindow::onActionLoadPeaksTriggered()
{
    if (closingRequested_) return;
    {
        auto allowed = qualpf::gui::peakEditEntryStatus(qualpf::gui::workspaceContext(this));
        if (!allowed) {setStatusMessage(QString::fromStdString(allowed.error().reason));return;}
    }
    ui->peakDockWidget->show();
    ui->peakDockWidget->raise();
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = "2 theta values (*.dat *.txt *.pea)";
    QString exts = "2 theta values (*.dat *.txt *.pea);;"
                   "d values (*.dat *.txt *.pea)";
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    tr("Load File"), settings.value(DEFAULT_DIR_KEY).toString(),
                                                    exts, &selectedFilter);

    if (!fileName.isEmpty()) {
        if (!confirmWorkflowReplacement(qualpf::application::WorkflowStep::peaks)) return;
        int tipo = (selectedFilter.startsWith("2 theta")) ? 1 : 2;
        {
            using namespace qualpf::application; using namespace qualpf::gui;
            const auto status=qualpf::application::importPeakFile(context().workspace().session(),context().files().text,fileName.toStdString(),
                tipo==1?PeakCoordinateRepresentation::two_theta:PeakCoordinateRepresentation::d_spacing);
            setProperty("peakErrorField",status?QString():QString::fromStdString(status.error().field));
            setProperty("peakErrorReason",status?QString():QString::fromStdString(status.error().reason));
            if (!status) {setStatusMessage(QString::fromStdString(status.error().reason=="file_read_failed" ? status.error().message : status.error().reason+": "+status.error().message));return;}
            refreshPatternPresentation();refreshPreprocessingDisplay();updateReport();
            return;
        }
    }
}

void MainWindow::onActionSavePeaksTriggered()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    QString selectedFilter = "2 theta values (*.dat *.txt *.pea)";
    QString exts = "2 theta values (*.dat *.txt *.pea);;"
                   "d values (*.dat *.txt *.pea);;"
                   "Input File for DICVOL (*.dat);;"
                   "Input File for McMaille (*.dat);;"
                   "Peak list (*.txt)";
    QString fileName = SaveDialog::run(this,tr("Save Peaks As"),
                                       settings.value(DEFAULT_DIR_KEY).toString(),
                                       QFileInfo(currentFile).baseName(),exts,
                                       "txt",selectedFilter);
    if (!fileName.isEmpty()) {
        QStringList list = exts.split(";;");
        int tipo = list.lastIndexOf(selectedFilter) + 1;
        // These legacy format branches are disabled in indexing.f90. Do not
        // present a silent no-op as a successful export, under either backend.
        if (tipo == 3 || tipo == 4) {
            setStatusMessage(tr("%1 export is unavailable in this build. Select 2 theta values, d values, or Peak list.")
                                 .arg(tipo == 3 ? QStringLiteral("DICVOL") : QStringLiteral("McMaille")));
            return;
        }
        if ((tipo == 1 || tipo == 2 || tipo == 5)) {
            const auto snapshot = context().searchSession().snapshot();
            auto text = qualpf::application::preparePeakExport(*snapshot->canonical, tipo == 1 ? qualpf::preprocessing::PeakTextFormat::two_theta :
                tipo == 2 ? qualpf::preprocessing::PeakTextFormat::d_spacing : qualpf::preprocessing::PeakTextFormat::details);
            if (!text) { setStatusMessage(QString::fromStdString(text.error().message)); return; }
            if (text.value().empty()) {
                setStatusMessage(tr("Peak export is unavailable: there are no peaks to save."));
                return;
            }
            auto written=context().files().text.writeAtomic(fileName.toStdString(), text.value());
            if (!written) setStatusMessage(tr("Peak export failed: %1").arg(QString::fromStdString(written.error().message)));
            return;
        }
        {
            setStatusMessage(tr("Unsupported peak export format."));
            return;
        }
    }
}

//
//  View Menu
//

void MainWindow::onActionPlotStyleTriggered()
{
    if (closingRequested_) return;
    plotStyleDialog->setOptions(xpdViewer());
    plotStyleDialog->show();
}

void MainWindow::onActionResetZoomTriggered()
{
    if (closingRequested_) return;
    const auto before=context().workspace().projectView();
    xpdViewer()->updateMinMax();
    double lower, upper;
    xpdViewer()->getMinMax(lower, upper);
    xpdViewer()->xAxis->setRange(lower, upper);
    xpdViewer()->redrawPlot(true);
    captureProjectView(); if(*before!=*context().workspace().projectView()) markProjectModified();
}

void MainWindow::onActionAutoscaleTriggered()
{
    if (closingRequested_) return;
    const auto before=context().workspace().projectView();
    xpdViewer()->applyAutoScale();
    captureProjectView(); if(*before!=*context().workspace().projectView()) markProjectModified();
}

void MainWindow::createActionGroup()
{
    if (closingRequested_) return;
    QActionGroup *zoomGroup = new QActionGroup(this);
    zoomGroup->addAction(ui->actionSelection_Mode);
    zoomGroup->addAction(ui->actionHorizontal_Zoom);
    zoomGroup->addAction(ui->actionRectangle_Zoom);
    zoomGroup->addAction(ui->actionPan);
    zoomGroup->addAction(ui->actionAdd_Background_Point);
    zoomGroup->addAction(ui->actionDelete_Background_Point);
    zoomGroup->addAction(ui->actionAdd_Peak);
    zoomGroup->addAction(ui->actionDelete_Peak);
    auto *inferred = ui->actionAddInferredPeak;
    zoomGroup->addAction(inferred);
    connect(ui->actionAdd_Peak, &QAction::changed, inferred, [this,inferred] {
        inferred->setEnabled(ui->actionAdd_Peak->isEnabled());
    });
    connect(zoomGroup,&QActionGroup::triggered, this, &MainWindow::zoomGroupTriggered);
}

void MainWindow::zoomGroupTriggered(QAction *action)
{
    if (closingRequested_) return;
    if (action == ui->actionSelection_Mode) {
        if (mAction != NoZoom) setAction(NoZoom);
    } else if (action == ui->actionHorizontal_Zoom) {
        if (mAction != HorizontalZoom) setAction(HorizontalZoom);
    } else if (action == ui->actionRectangle_Zoom) {
        if (mAction != RectangleZoom) setAction(RectangleZoom);
    } else if (action == ui->actionPan) {
        if (mAction != Pan) setAction(Pan);
    } else if (action == ui->actionAdd_Background_Point) {
        setBackgroundSubtractionDisplay(false);
        if (mAction != AddBackgroundPoint) setAction(AddBackgroundPoint, false);
    } else if (action == ui->actionDelete_Background_Point) {
        setBackgroundSubtractionDisplay(false);
        if (mAction != DeleteBackgroundPoint) setAction(DeleteBackgroundPoint, false);
    } else if (action == ui->actionAdd_Peak) {
        if (mAction != AddPeak) setAction(AddPeak, false);
        ui->peakDockWidget->show();
        ui->peakDockWidget->raise();
    } else if (action->objectName() == "actionAddInferredPeak") {
        setAction(AddInferredPeak, false);
        ui->peakDockWidget->show();
        ui->peakDockWidget->raise();
    } else if (action == ui->actionDelete_Peak) {
        if (mAction != DeletePeak) setAction(DeletePeak, false);
    }
}

void MainWindow::checkAction(MouseAction action)
{
    if (closingRequested_) return;
    if (action == mAction) return;

    switch (action) {
    case NoZoom:
        ui->actionSelection_Mode->setChecked(true);
        break;
    case RectangleZoom:
        ui->actionRectangle_Zoom->setChecked(true);
        break;
    case HorizontalZoom:
        ui->actionHorizontal_Zoom->setChecked(true);
        break;
    case Pan:
        ui->actionPan->setChecked(true);
        break;
    default:
        break;
    }
    setAction(action, false);
}

void MainWindow::saveAction()
{
    if (closingRequested_) return;
    savedAction = mAction;
}

void MainWindow::restoreAction()
{
    if (closingRequested_) return;
    checkAction(savedAction);
}

void MainWindow::setZoomAction()
{
    if (closingRequested_) return;
    if (mAction != savedZoomAction)
        checkAction(savedZoomAction);
}

void MainWindow::readAction()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
    if (settings.contains(XPDVIEW_ACTION_KEY)) {
        QString sAction = settings.value(XPDVIEW_ACTION_KEY).toString();
        QMetaEnum metaEnum = QMetaEnum::fromType<MouseAction>();
        int action = metaEnum.keyToValue(sAction.toStdString().c_str());
        checkAction(static_cast<MouseAction>(action));
    } else {
        checkAction(HorizontalZoom);
    }
}

void MainWindow::setAction(const MouseAction &action, bool writeConfig)
{
    if (closingRequested_) return;
    XpdViewWidget::MouseAction xpdAction = static_cast<XpdViewWidget::MouseAction>(action);
    xpdViewer()->setAction(xpdAction);

    if (writeConfig) {
        qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());
        QMetaEnum metaEnum = QMetaEnum::fromType<MouseAction>();
        const char *key = metaEnum.valueToKey(action);
        settings.setValue(XPDVIEW_ACTION_KEY,key);
    }
}

void MainWindow::plotStyleClosed(QDialogButtonBox::StandardButton button)
{
    if (closingRequested_) return;
    if (button == QDialogButtonBox::Apply) {
        plotStyleDialog->apply(xpdViewer());
        if(plotStyleDialog->anyChangeApplied()) { captureProjectView(); markProjectModified(); }
    } else if (button == QDialogButtonBox::Ok) {
        if (plotStyleDialog->anyChangeToApply()) {
            plotStyleDialog->apply(xpdViewer());
        }

        if (plotStyleDialog->anyChangeApplied()) {
            plotStyleConfig();
            captureProjectView(); markProjectModified();
        }

    } else if (button == QDialogButtonBox::Cancel) {
        plotStyleDialog->cancel(xpdViewer());
    } else if (button == QDialogButtonBox::RestoreDefaults) {
        for (int i = 0; i < plotStyleDialog->obsPen.count(); i++) {
            plotStyleDialog->obsPen[i] = xpdViewer()->obs.at(i).getDefaultPen(i);
            plotStyleDialog->obsScatter[i] = xpdViewer()->obs.at(i).getDefaultScatter(i);
            plotStyleDialog->obsVisible[i] = true;
        }
        QPen backPen = xpdViewer()->back.getDefaultPen();
        QCPScatterStyle backScatter = xpdViewer()->bpoints.getDefaultScatter();
        QPen calcPen = xpdViewer()->calc.getDefaultPen();
        QPen diffPen = xpdViewer()->diff.getDefaultPen();
        QPen cDiffPen = xpdViewer()->cdiff.getDefaultPen();
        QPen peaksPen = xpdViewer()->peaks.getDefaultPen();
        for (int i = 0; i < plotStyleDialog->reflPen.count(); i++) {
            plotStyleDialog->reflPen[i] = xpdViewer()->refSet.at(i).getDefaultPen(i);
            plotStyleDialog->reflVisible[i] = true;
        }
        PlotSettings defaults;
        defaults.restoreDefaults();
        plotStyleDialog->setWidgets(false, backPen, backScatter, calcPen, diffPen, cDiffPen, peaksPen, defaults,
                                    xpdViewer()->back.visible, xpdViewer()->bpoints.visible, xpdViewer()->calc.visible,
                                    xpdViewer()->diff.visible, xpdViewer()->cdiff.visible, xpdViewer()->peaks.visible);
    }
}

void MainWindow::plotStyleConfig()
{
    if (closingRequested_) return;
    qualpf::gui::Preferences settings(qualpf::gui::workspaceContext(this).preferences());

    for (int i = 0; i < plotStyleDialog->obsPen.count(); i++ ) {
        settings.setValue(xpdViewer()->obs[i].getKeyString("pen", i),xpdViewer()->obs.at(i).getPen());
        settings.setValue(xpdViewer()->obs[i].getKeyString("scatterPen", i), xpdViewer()->obs.at(i).getScatter().pen());
        settings.setValue(xpdViewer()->obs[i].getKeyString("scatterStyle", i), xpdViewer()->obs.at(i).getScatter().shape());
    }

    settings.setValue(xpdViewer()->back.getKeyString("pen", -1),xpdViewer()->back.getPen());
    settings.setValue(xpdViewer()->bpoints.getKeyString("scatterPen", -1),xpdViewer()->bpoints.getScatter().pen());
    settings.setValue(xpdViewer()->bpoints.getKeyString("scatterStyle", -1),xpdViewer()->bpoints.getScatter().shape());

    settings.setValue(xpdViewer()->calc.getKeyString("pen", -1),xpdViewer()->calc.getPen());
    settings.setValue(xpdViewer()->diff.getKeyString("pen", -1),xpdViewer()->diff.getPen());
    settings.setValue(xpdViewer()->cdiff.getKeyString("pen", -1),xpdViewer()->cdiff.getPen());
    settings.setValue(xpdViewer()->peaks.getKeyString("pen", -1),xpdViewer()->peaks.getPen());

    for (int i = 0; i < plotStyleDialog->reflPen.count(); i++) {
        settings.setValue(xpdViewer()->refSet[i].getKeyString("pen", i),xpdViewer()->refSet.at(i).pen);
    }
    xpdViewer()->pSettings().write(context().preferences());
}

//
//  Search Menu
//

void MainWindow::onActionSearchMatchTriggered()
{
    if (closingRequested_) return;
    requestMatch(qualpf::application::SearchMethod::classic_sm);
}

void MainWindow::onActionSearchMatchOptionsTriggered()
{
    if (closingRequested_ || searchTasks_->busy() || context().searchSession().snapshot()->editing) return;
    SearchOptionsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onActionCancelSearchTriggered()
{
    if (closingRequested_) return;
    searchTasks_->cancel(); setStatusMessage(tr("Search canceled"));
}

void MainWindow::executeSearch(qualpf::gui::SearchRequest &builder, bool merge)
{
    if (closingRequested_) return;
    if (!searchAdmission()) return;
    using namespace qualpf::application;
    SearchInput input; input.source=context().workspace().session().snapshot(); input.method=input.source->algorithms->spec.method();
    input.query=builder.candidateQuery(builder.getCalcFom()?CandidateQuery::Route::matching:CandidateQuery::Route::constrained_browse);
    input.operation=builder.getCalcFom()?SearchOperation::scored_browse:SearchOperation::metadata_browse;
    input.threshold=builder.getMinFom(); input.merge=merge;
    input.parameters=currentSearchParameters(qualpf::gui::workspaceContext(this));
    input.parameters.delta_two_theta.value=builder.getDelta2theta(); input.parameters.weight_position=builder.getWeight2thetaD();
    input.parameters.weight_intensity=builder.getWeightIntensity(); input.parameters.weight_phases=builder.getWeightPhases();
    setStatusMessage(tr("Querying database..."));
    submitSearch(std::move(input),[this](const auto &) { publishSearchView(true); });
}

void MainWindow::actionRestraintsTriggered()
{
    if (closingRequested_) return;
    const auto source = context().workspace().session().snapshot();
    if (!m_restraintsDialog->isVisible()) {
        const bool analysis = source->pfsm || !source->candidates.empty() || !source->phases.empty();
        m_restraintsDialog->setSelectedSearchMethod(analysis ? source->algorithms->spec.method() : qualpf::application::SearchMethod::classic_sm);
    }
    m_restraintsDialog->setMergeEnabled(ui->resultsWidget->hasResults());
    m_restraintsDialog->setSearchEnabled(source->pattern.observed.size() >= 3 && !source->editing);
    m_restraintsDialog->setBrowseEnabled(source->algorithms->spec.method() != qualpf::application::SearchMethod::pfsm);
    m_restraintsDialog->setBusy(searchTasks_->busy());
    m_restraintsDialog->show();
    m_restraintsDialog->raise();
    m_restraintsDialog->activateWindow();
}

void MainWindow::applyDialogRestraints(qualpf::gui::SearchRequest &builder)
{
    if (closingRequested_) return;
    const auto source = context().workspace().session().snapshot();
    if (source->algorithms->spec.method() == qualpf::application::SearchMethod::classic_sm)
        builder.enableDeleted(SearchOptionsDialog::savedCheckDeleted(source, context().settings()));

    const RestraintsDialog *dlg = m_restraintsDialog;
    builder.setSpeciesBounds(dlg->compositionMinSpecies(), dlg->compositionMaxSpecies());

    const QString formula = dlg->compositionFormula();
    if (!formula.isEmpty()) {
        builder.setElements(formula);
        if (dlg->isExactComposition())
            builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        else if (dlg->isContainsAny())
            builder.setBOperator(qualpf::gui::SearchRequest::JUST_OP);
    }

    const QString chemName = dlg->chemicalName();
    if (!chemName.isEmpty())
        builder.setNames(chemName);

    const QStringList subfiles = dlg->subfilesCodes();
    if (!subfiles.isEmpty())
        builder.setSubfiles(subfiles);

    const QStringList csys = dlg->crystalSystemStrings();
    if (!csys.isEmpty())
        builder.setCsysString(csys);

    const QStringList spg = dlg->spaceGroupStrings();
    if (!spg.isEmpty())
        builder.setSpgString(spg);

    const auto cell = dlg->cellQuery();
    for (int i = 0; i < 6; ++i) {
        if (cell.values[i] > 0.0) {
            const double tol = (i < 3) ? cell.lenTol : cell.angTol;
            builder.setCellParameter(i, cell.values[i] - tol, cell.values[i] + tol);
        }
    }

    const QStringList ids = dlg->entryIds();
    if (!ids.isEmpty())
        builder.setIdEntry(ids);

    const QStringList colors = dlg->colorStrings();
    if (!colors.isEmpty())
        builder.setColorString(colors);

    const double densTol = dlg->densityTolerance();
    const double densCalc = dlg->densityCalc();
    if (densCalc > 0.0)
        builder.setDensityCalc(densCalc - densTol, densCalc + densTol);
    const double densMeas = dlg->densityMeas();
    if (densMeas > 0.0)
        builder.setDensityMeas(densMeas - densTol, densMeas + densTol);
    const auto input = context().searchSession().snapshot();
    auto next = *input;
    next.restraints = dlg->snapshot();
    auto status = context().searchSession().commit(input, std::move(next));
    if (!status) setStatusMessage(QString::fromStdString(status.error().message));
}

void MainWindow::onRestraintsSearch(bool merge)
{
    if (closingRequested_) return;
    if (!searchAdmission()) return;
    if (context().workspace().session().snapshot()->algorithms->spec.method() == qualpf::application::SearchMethod::pfsm) {
        setStatusMessage(tr("Use Search with Restraints to start a new PFSM candidate universe")); return;
    }
    if (!m_restraintsDialog->hasRestraints()) return;

    qualpf::gui::SearchRequest builder;
    builder.setPrintEnabled(true);
    if (loadExperimentalPeaks(context()) > 0) {
        builder = buildSearchMatchQuery(qualpf::gui::workspaceContext(this), context().peaks());
        builder.setMinFom(-1.0); // Explicit browse mode retains every defined score.
    } else {
        builder.setCalcFom(false);
    }
    applyDialogRestraints(builder);
    executeSearch(builder, merge);
}

void MainWindow::onRestraintsSearchMatch(qualpf::application::SearchMethod method)
{
    if (closingRequested_) return;
    if (m_restraintsDialog->hasRestraints()) requestMatch(method, true);
}

void MainWindow::runSearch(const SearchOptions &opts)
{
    if (closingRequested_) return;
    qualpf::gui::SearchRequest builder;
    builder.setPrintEnabled(true);
    builder.setCalcFom(false);

    if (!opts.composition.isEmpty()) {
        builder.setElements(opts.composition);
        if (opts.exactComposition)
            builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        else if (opts.containsAny)
            builder.setBOperator(qualpf::gui::SearchRequest::JUST_OP);
        // else: AND_OR_OP is the default
    }

    executeSearch(builder);
}

void MainWindow::runSearchMatch()
{
    if (closingRequested_) return;
    onActionSearchMatchTriggered();
}

void MainWindow::onActionTestDatabaseTriggered()
{
    if (closingRequested_) return;
    if (!searchAdmission()) return;
    qualpf::gui::SearchRequest builder; builder.setPrintEnabled(true); testSelection(builder,22);
    executeSearch(builder);
}

void MainWindow::actionManageDatabasesTriggered()
{
    if (closingRequested_) return;
    if (!searchAdmission()) return;
    ManageDatabasesDialog dlg(this);
    dlg.setDatabases(context().databases());
    if (dlg.exec() == QDialog::Accepted)
        if (auto changed = context().setDatabases(dlg.databases()); !changed) setStatusMessage(QString::fromStdString(changed.error().message));
}

void MainWindow::onCardSelected(const QString &id)
{
    if (closingRequested_) return;
    const auto info = qualpf::gui::cardDetailsView(context().catalog().details(id.toStdString()));
    if (!info.valid) return;

    auto row = [](const QString &label, const QString &value) -> QString {
        if (value.isEmpty() || value == "0" ) return QString();
        return QString("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(label, value);
    };
    auto rowD = [](const QString &label, double value, int decimals = 4) -> QString {
        if (value == 0.0) return QString();
        return QString("<tr><td><b>%1</b></td><td>%2</td></tr>").arg(label).arg(value, 0, 'f', decimals);
    };

    // Only report cell parameters that are actually known (non-zero); e.g. for a
    // cubic cell only "a" is stored and b, c, alpha, beta, gamma are left at 0.
    QStringList cellParts;
    if (info.a != 0.0)     cellParts << QString("a=%1").arg(info.a, 0, 'f', 4);
    if (info.b != 0.0)     cellParts << QString("b=%1").arg(info.b, 0, 'f', 4);
    if (info.c != 0.0)     cellParts << QString("c=%1").arg(info.c, 0, 'f', 4);
    if (info.alpha != 0.0) cellParts << QString("&alpha;=%1").arg(info.alpha, 0, 'f', 3);
    if (info.beta  != 0.0) cellParts << QString("&beta;=%1").arg(info.beta, 0, 'f', 3);
    if (info.gamma != 0.0) cellParts << QString("&gamma;=%1").arg(info.gamma, 0, 'f', 3);
    const QString cellParams = cellParts.join(" ");

    QString ref;
    if (!info.authors.isEmpty())
        ref = info.authors;
    if (!info.journal.isEmpty()) {
        if (!ref.isEmpty()) ref += "; ";
        ref += info.journal;
        if (!info.journalVolume.isEmpty()) ref += " <b>" + info.journalVolume + "</b>";
        if (info.journalYear > 0)   ref += QString(" (%1)").arg(info.journalYear);
        if (!info.pageStart.isEmpty()) ref += " " + info.pageStart;
        if (!info.pageEnd.isEmpty())   ref += "-" + info.pageEnd;
    }

    QString html = "<html><body style='font-family:sans-serif;font-size:9pt'>";
    const QString idColor = cardColor(info.id, this).name();
    html += QString("<h3 style='margin:4px 0;color:%1'>%2</h3>").arg(idColor, info.id);
    html += "<table cellspacing='2' cellpadding='2'>";
    html += row("Name",             info.name);
    html += row("Mineral Name",     info.mineralName);
    html += row("Formula",          info.chemicalFormula);
    html += row("Space Group",      info.spaceGroup);
    html += row("Quality",          info.quality);
    html += row("RIR",              info.rir);
    if (!cellParams.isEmpty())
        html += "<tr><td><b>Cell</b></td><td>" + cellParams + "</td></tr>";
    html += rowD("Volume (&Aring;&sup3;)", info.volume, 2);
    html += rowD("Z",               static_cast<double>(info.z), 0);
    html += rowD("Density",         info.density, 3);
    html += rowD("Calc. Density",   info.crystalDensity, 3);
    html += rowD("&mu;(CuK&alpha;)",info.muCuKa, 3);
    html += row("Color",            info.color);
    html += row("Type",             info.type);
    if (!ref.isEmpty())
        html += "<tr><td><b>Reference</b></td><td>" + ref + "</td></tr>";

    int dbNcard;
    QString dbType;
    const auto dbInfo = context().catalog().information(); dbNcard = dbInfo.first; dbType = QString::fromStdString(dbInfo.second);
    if (dbType == "COD") {
        const QString codUrl = QString("https://www.crystallography.net/cod/%1.html").arg(info.id);
        html += QString("<tr><td><b>COD Entry</b></td><td><a href='%1'>%1</a></td></tr>").arg(codUrl);
    }
    html += "</table>";

    // --- Reflections table ---
    const QVector<double> &dv = info.dvalues;
    if (!dv.isEmpty()) {
        const QVector<double> &pw = xpdViewer()->plotWave;
        const double wave = pw.isEmpty() ? qualpf::application::defaultWavelength() : pw.first();

        const QVector<double> tth = xpdutils::tthvalue_safe(dv, wave);

        const bool hasI   = (info.intensities.size() == dv.size());
        const bool hasHKL = (info.h.size() == dv.size() &&
                             info.k.size() == dv.size() &&
                             info.l.size() == dv.size());
        // Multiplicities are only meaningful alongside h,k,l; without those,
        // there's no reflection to attach a multiplicity to.
        const bool hasMul = hasHKL && (info.mul.size() == dv.size());

        html += QString("<h4 style='margin:6px 0 2px 0'>Reflections "
                        "(&lambda; = %1 &Aring;)</h4>").arg(wave, 0, 'f', 5);
        html += "<table border='1' cellspacing='0' cellpadding='3' "
                "style='border-collapse:collapse;font-size:8pt'>";
        html += "<tr style='background:#ddd'>"
                "<th>#</th><th>2&theta;</th><th>d (&Aring;)</th><th>I (%)</th>";
        if (hasHKL) html += "<th>h</th><th>k</th><th>l</th>";
        if (hasMul) html += "<th>Mult</th>";
        html += "</tr>";

        for (int i = 0; i < dv.size(); ++i) {
            html += "<tr>";
            html += "<td align='right'>"  + QString::number(i + 1)                     + "</td>";
            html += "<td align='right'>"  + (i < tth.size()
                        ? QString::number(tth[i], 'f', 3) : QStringLiteral("-"))       + "</td>";
            html += "<td align='right'>"  + QString::number(dv[i], 'f', 4)             + "</td>";
            html += "<td align='right'>"  + (hasI
                        ? QString::number(info.intensities[i], 'f', 1) : QStringLiteral("-")) + "</td>";
            if (hasHKL) {
                html += "<td align='center'>" + QString::number(info.h[i]) + "</td>";
                html += "<td align='center'>" + QString::number(info.k[i]) + "</td>";
                html += "<td align='center'>" + QString::number(info.l[i]) + "</td>";
            }
            if (hasMul) {
                html += "<td align='center'>" + QString::number(info.mul[i]) + "</td>";
            }
            html += "</tr>";
        }
        html += "</table>";
    }

    html += "</body></html>";

    ui->cardBrowser->setHtml(html);
    ui->dockWidgetCard->setWindowTitle(id);
}

void MainWindow::performResidualSearch(const CardType &acceptedCard)
{
    if (closingRequested_) return;
    if (!searchAdmission()) return;
    const auto source=context().workspace().session().snapshot();
    if (!source->peaks.valid || source->peaks.tth.empty()) return;
    qualpf::application::SearchInput input; input.source=source; input.method=source->algorithms->spec.method(); input.operation=qualpf::application::SearchOperation::residual;
    input.phase=acceptedCard.productValue(); input.parameters=currentSearchParameters(qualpf::gui::workspaceContext(this));
    submitSearch(std::move(input),[this](const auto &) {
        ui->resultsWidget->refresh(); ui->peakCompareWidget->setExperimentalPeaks(context().peaks());
        setStatusMessage(tr("%1 card(s) after residual search").arg(context().searchSession().snapshot()->candidates.size()));
    });
}

void MainWindow::recalculateFOMs(const QString &statusSuffix)
{
    if (closingRequested_) return;
    if (!searchAdmission()) return;
    qualpf::application::SearchInput input; input.source=context().workspace().session().snapshot(); input.method=input.source->algorithms->spec.method();
    input.operation=qualpf::application::SearchOperation::rescore; input.parameters=currentSearchParameters(qualpf::gui::workspaceContext(this));
    if (input.source->pfsm) {
        auto request = qualpf::application::makePfsmRequest(input.source, qualpf::gui::savedPfsmSettings(context()));
        if (!request) { setStatusMessage(QString::fromStdString(request.error().message)); return; }
        input.pfsm = std::make_shared<const qualpf::application::PfsmRequest>(std::move(request).value());
    }
    submitSearch(std::move(input),[this,statusSuffix](const auto &) {
        ui->resultsWidget->refresh(); ui->quantWidget->refresh();
        if (context().workspace().session().snapshot()->pfsm) refreshPatternPresentation();
        setStatusMessage(tr("%1 card(s)%2").arg(context().searchSession().snapshot()->candidates.size()).arg(statusSuffix));
    });
}

void MainWindow::onActionRecalculateFomTriggered()
{
    if (closingRequested_) return;
    recalculateFOMs();
}

void MainWindow::onActionLoadAddTriggered()
{
    if (closingRequested_) return;
    if (context().workspace().session().snapshot()->algorithms->spec.method() == qualpf::application::SearchMethod::pfsm) {
        setStatusMessage(tr("Start a new PFSM search to change the candidate universe")); return;
    }
    if (!searchAdmission()) return;
    bool ok=false;
    const auto id=QInputDialog::getText(this,tr("Load/Add Card by ID"),tr("Card ID:"),QLineEdit::Normal,QString(),&ok).trimmed();
    if (!ok || id.isEmpty() || !searchAdmission()) return;
    qualpf::application::SearchInput input; input.source=context().workspace().session().snapshot(); input.method=input.source->algorithms->spec.method();
    input.operation=qualpf::application::SearchOperation::load_card; input.cardId=id.toStdString(); input.parameters=currentSearchParameters(qualpf::gui::workspaceContext(this));
    submitSearch(std::move(input),[this,id](const auto &) {
        ui->resultsWidget->refresh(); ui->resultsWidget->selectCard(id);
        setStatusMessage(tr("%1 card(s)").arg(ui->resultsWidget->allCards().size()));
        ui->dockWidgetCard->show(); ui->dockWidgetCard->raise();
    });
}

//
//  Help Menu
//

void MainWindow::onActionDocumentationHtmlTriggered()
{
    if (closingRequested_) return;
    QDesktopServices::openUrl(QUrl("https://ccorrado71.github.io/qualx3/"));
}

void MainWindow::onActionDocumentationPdfTriggered()
{
    if (closingRequested_) return;
    const auto path=context().platform().documentationPath();
    if(!path.empty()) { QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path))); return; }
    QMessageBox::information(this, tr("Documentation"),
        tr("PDF documentation not found.\n"
           "Run 'bash docs/make_pdf.sh' to generate it."));
}

//
//  Window Menu
//

void MainWindow::onActionDefaultLayoutTriggered()
{
    if (closingRequested_) return;
    //bool nextVisible = ui->toolBarNext->isVisible();
    restoreState(defaultState);
    // resizeDocks({ui->dockWidget,ui->dockWidget_2},{50,50},Qt::Horizontal);
    // qInfo() << "Next is visible: " << nextVisible;
    // ui->toolBarNext->setVisible(nextVisible);
}

//
// Helpers
//

void MainWindow::onActionCheckForUpdatesTriggered()
{
    if (closingRequested_) return;
#if defined(Q_OS_WIN)
    auto m_updater = new MaintenanceTool(this);
    connect(m_updater, &MaintenanceTool::stateChanged, [=](MaintenanceTool::ProcessState state){
        if (state == MaintenanceTool::NotRunning && m_updater->hasUpdate()) {
            m_updater->startMaintenanceTool(MaintenanceTool::Updater);
        }
    });

    m_updater->checkUpdate();
#else
    QMessageBox::information(this, tr("QualPF Updates"),
        tr("Online updates for QualPF are not configured.\n"
           "Build a newer QualPF package locally; see this project's installation instructions."));
#endif
}

void MainWindow::onActionAboutTriggered()
{
    if (closingRequested_) return;
    aboutDialog->show();
}

void MainWindow::testSelection(qualpf::gui::SearchRequest &builder, int testCase)
{
    if (closingRequested_) return;
    switch (testCase) {
    case 1:
        builder.setNames("nickel");
        builder.enableDeleted(false);
        break;
    case 2:
        builder.setNames("nickel");
        builder.setSubfiles({"I"});
        break;
    case 3:
        builder.setNames("silicon oxide");
        builder.setSubfiles({"O"});
        break;
    case 4:
        builder.setNames("carbon");
        builder.setSubfiles({"I"});
        break;
    case 5:
        builder.setNames("copper");
        break;
    case 6:
        builder.setElements("Al");
        builder.setNames("copper");
        break;
    case 7:
        builder.setElements("Al and P and O");
        break;
    case 8:
        builder.setElements("Al or C");
        break;
    case 9:
        builder.setElements("Al or P and C");
        break;
    case 10:
        builder.setElements("Al and Si and P or C or O");
        break;
    case 11:
        builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        builder.setElements("Al");
        break;
    case 12:
        builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        builder.setElements("Al and O");
        break;
    case 13:
        builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        builder.setElements("Al and O and P");
        break;
    case 14:
        builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        builder.setElements("Al and O and P and Si");
        break;
    case 15:
        builder.setBOperator(qualpf::gui::SearchRequest::JUST_OP);
        builder.setElements("Al");
        break;
    case 16:
        builder.setBOperator(qualpf::gui::SearchRequest::JUST_OP);
        builder.setElements("Al and O");
        break;
    case 17:
        builder.setBOperator(qualpf::gui::SearchRequest::JUST_OP);
        builder.setElements("Al and O and P");
        break;
    case 18:
        builder.setNames("potassium");
        builder.setSubfiles({"I"});
        builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        builder.setElements("Al and P");
        break;
    case 19:
        builder.setCsysString({"Cubic"});
        break;
    case 20:
        builder.setCsysString({"Cubic","Tetragonal"});
        break;
    case 21:
        builder.setCsysString({"hexagonal"});
        builder.setElements("Al");
        break;
    case 22:
        builder.setSpgString({"P 1 2 1"});
        break;
    case 23:
        builder.setSpgString({"P 1 1 2", "P 2 1 1"});
        break;
    case 24:
        builder.setSpgString({"P 1 1 2", "P 2 1 1"});
        builder.setCsysString({"Cubic"});
        break;
    case 25:
        builder.setCellParameter(0, 5.00, 5.01);
        builder.setCellParameter(1, 5.00, 5.01);
        break;
    case 26:
        builder.setCellParameter(0, 10.00, 11.00);
        builder.setCellParameter(1, 10.00, 11.00);
        builder.setCsysString({"Monoclinic"});
        break;
    case 27:
        builder.setCellParameter(0, 10.00, 11.00);
        builder.setCellParameter(1, 10.00, 11.00);
        builder.setCsysString({"Monoclinic"});
        builder.setSubfiles({"I"});
        break;
    case 28:
        builder.setCellParameter(0, 10.00, 11.00);
        builder.setCellParameter(1, 10.00, 11.00);
        builder.setCsysString({"Monoclinic"});
        builder.setSubfiles({"I"});
        builder.setElements("Al");
        break;
    case 29:
        builder.setIdEntry({"1000000","1000017"});
        builder.setSubfiles({"I"});
        builder.setElements("Al");
        builder.setCsysString({"Monoclinic"});
        break;
    }
}

void MainWindow::captureProjectView() {
    if(closingRequested_ || restoringProjectView_) return;
    auto &workspace=context().workspace();auto view=*workspace.projectView();
    view=xpdViewer()->captureProjectView(std::move(view));view=ui->resultsWidget->captureProjectView(std::move(view));
    const auto &presentation=context().presentation().snapshot();view.subtract_background=presentation.subtract_background;
    for(const auto &[layer,key]:qualpf::gui::projectLayers)
        view.layers[key]=qualpf::gui::projectVisibility(presentation.layers[static_cast<std::size_t>(layer)]);
    const auto status=workspace.updateProjectView(std::move(view));
    (void)status; // Automatic projection synchronization is not a user edit.
}
