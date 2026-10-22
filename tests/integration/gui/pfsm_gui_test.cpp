#include "gui/mainwindow.h"
#include "gui/dialogs/pfsm_options_dialog.h"
#include "gui/widgets/dbresultswidget.h"
#include "gui/widgets/quantwidget.h"
#include "gui/widgets/piechartwidget.h"
#include "gui/models/paginationmodel.h"
#include "gui/adapters/preferences.h"
#include "infrastructure/io/pattern_source_reader.h"
#include "tests/support/workspace_fixture.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QProcess>
#include <QSettings>
#include <QSpinBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <source_location>
#include <thread>
using namespace qualpf;
using namespace qualpf::application;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) throw std::runtime_error("PFSM GUI at line " + std::to_string(at.line()));
}
template<class T> static T *widget(QObject &parent, const char *name = "") {
    auto *value = *name ? parent.findChild<T *>(QString::fromLatin1(name)) : parent.findChild<T *>();
    if (!value) throw std::runtime_error(std::string("PFSM GUI missing widget: ") + name + " (" + T::staticMetaObject.className() + ")");
    return value;
}
static void waitUntil(const std::function<bool()> &done,
                      std::source_location at = std::source_location::current()) {
    QEventLoop loop; QTimer poll, timeout; bool expired = false;
    QObject::connect(&poll, &QTimer::timeout, &loop, [&] { if (done()) loop.quit(); });
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&] { expired = true; loop.quit(); });
    poll.start(1); timeout.setSingleShot(true); timeout.start(15000);
    if (!done()) loop.exec();
    check(!expired && done(), at);
}
static void run(MainWindow &window, const std::function<void()> &action,
                std::source_location at = std::source_location::current()) {
    action(); waitUntil([&] { return !window.searchTasks()->busy(); }, at);
}
static void hover(XpdViewWidget &plot) {
    for (double x : {27.9, 28., 30.061, 35., 35.1}) {
        const QPointF position(plot.xAxis->coordToPixel(x), plot.yAxis->coordToPixel(7));
        QMouseEvent event(QEvent::MouseMove, position, plot.mapToGlobal(position.toPoint()),
                          Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(&plot, &event);
    }
}
static void answer(QMessageBox::StandardButton value) {
    QTimer::singleShot(0, [value] {
        for (auto *w : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(w); box && box->isVisible()) box->button(value)->click();
    });
}
struct Background final : preprocessing::IBackgroundEstimator {
    mutable std::atomic<bool> block = false, entered = false;
    mutable std::atomic<int> calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.background.cpp", 1}; }
    Result<preprocessing::BackgroundResult> estimate(const Pattern &p, const preprocessing::BackgroundOptions &,
        WavelengthAngstrom, std::optional<std::span<const preprocessing::BackgroundPoint>>, const ExecutionContext &c) const override {
        ++calls; entered = true;
        while (block && !c.cancellation.stop_requested()) std::this_thread::yield();
        if (c.cancellation.stop_requested()) return Result<preprocessing::BackgroundResult>{Error{ErrorCode::cancelled,"test","Canceled"}};
        return Result<preprocessing::BackgroundResult>{preprocessing::BackgroundResult{std::vector<double>(p.observed.size(), p.processing.intensity_scaled == true ? 35 : 7)}};
    }
};
struct Initial final : preprocessing::IInitialPeakFinder {
    mutable int calls = 0;
    AlgorithmIdentity identity() const override { return {"qualpf.peaks.initial.cpp", 5}; }
    Result<preprocessing::InitialPeakResult> find(const AnalysisSpectrum &spectrum, WavelengthAngstrom,
        const preprocessing::InitialPeakOptions &, const ExecutionContext &) const override {
        ++calls; throw std::runtime_error("PFSM must skip Classical initial peaks");
    }
};
struct Record { int reads = 0; CandidateQuery query; std::thread::id execution; std::vector<ProductCard> extra; };
static ProductCard card(const char *id, const char *name, double center) {
    ProductCard c; c.id = id; c.chemicalName = name; c.chemicalFormula = "Si"; c.quality = "C";
    if (center) { c.d = {1 / (2 * std::sin(center * std::numbers::pi / 360))}; c.intensity = {1}; }
    return c;
}
struct Reader final : CandidateReader {
    std::shared_ptr<Record> record;
    explicit Reader(std::shared_ptr<Record> r) : record(std::move(r)) {}
    Result<std::vector<ProductCard>> read(const CandidateQuery &q, const ExecutionContext &, SearchProgress) override {
        ++record->reads; record->query = q; record->execution = std::this_thread::get_id();
        std::vector<ProductCard> cards{card("101","Strong Silicon",30), card("102","Weak Silicon",33), card("103","Wrong Silicon",31), card("104","Zero template",0)};
        cards.insert(cards.end(),record->extra.begin(),record->extra.end());
        return Result<std::vector<ProductCard>>{std::move(cards)};
    }
};
struct Factory final : CandidateReaderFactory {
    std::shared_ptr<Record> record;
    explicit Factory(std::shared_ptr<Record> r) : record(std::move(r)) {}
    Result<std::unique_ptr<CandidateReader>> open(const ReferenceDatabase &, std::uint64_t, const ExecutionContext &) override {
        return Result<std::unique_ptr<CandidateReader>>{std::make_unique<Reader>(record)};
    }
};
static ProductState state(AlgorithmHandle algorithms) {
    ProductState s; s.algorithms = std::move(algorithms); auto &p = s.pattern;
    p.id = {71}; p.source_id = "Independent analytic mixture";
    p.radiation.lines = {{WavelengthAngstrom{1},1}}; p.processing = {false,false,true};
    p.original_kalpha2_subtracted = false;
    s.original_signal_provenance = OriginalSignalProvenance{p.source_id,"analytic units",true,false};
    p.original_observed.emplace();
    const auto il = [](double x, double q) { return std::pow(1 + 4 * (std::pow(2., 2./3) - 1) * std::pow((x - q) / .2, 2), -1.5); };
    for (int i = 0; i <= 700; ++i) {
        const double x = 28 + i * .01, y = 7 + 100 * il(x,30.061) + 30 * il(x,32.964);
        p.two_theta.emplace_back(x); p.original_observed->push_back(y); p.observed.push_back(5 * y);
    }
    p.calculation_range = AngularRange{p.two_theta.front(),p.two_theta.back()};
    return s;
}
int main(int argc, char **argv) try {
    QApplication app(argc, argv); QTemporaryDir temporary; check(temporary.isValid());
    if (qEnvironmentVariable("QT_QPA_PLATFORM") == "offscreen") {
        const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        if (font >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).front(), 9));
    }
    QSettings::setDefaultFormat(QSettings::IniFormat); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    app.setOrganizationName("QualPF-tests"); app.setApplicationName("pfsm-gui");
    if (argc == 3 && std::string(argv[1]) == "--verify-isolated-preference") {
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,QString::fromLocal8Bit(argv[2]));
        const auto saved = qualpf::gui::savedPfsmSettings(AppState::context());
        check(saved.candidate_recall_enabled && saved.candidate_recall_limits == profile::PfsmCandidateRecallLimits{});
        return 0;
    }
    auto background = std::make_shared<Background>(); auto initial = std::make_shared<Initial>();
    AlgorithmModules modules; modules.background = background; modules.initial_peaks = initial;
    auto resolved = resolveAlgorithms(AlgorithmSpec{}, modules); check(bool(resolved));
    auto &session = AppState::workspace().session();
    QFile importedFile(temporary.path() + "/original.xy");
    check(importedFile.open(QIODevice::WriteOnly)); check(importedFile.write("20 -1\n20.01 0\n20.02 2\n") > 0); importedFile.close();
    infrastructure::PatternSourceReader importer;
    check(bool(openPatternInput(session,importer,importedFile.fileName().toStdString())));
    check(session.snapshot()->original_signal_provenance && session.snapshot()->pattern.original_observed->front() == -1);
    check(bool(makePfsmRequest(session.snapshot(),{})));
    auto unattested = prepareProductPatternOpen(session.snapshot(),session.snapshot()->pattern); check(bool(unattested));
    check(!unattested.value().original_signal_provenance && !makePfsmRequest(std::make_shared<const ProductState>(unattested.value()),{}));
    check(bool(session.commit(session.snapshot(), state(resolved.value()))));
    auto record = std::make_shared<Record>();
    MainWindow window(AppState::context(), nullptr, std::make_shared<Factory>(record));
    window.resize(1440,900); window.show(); window.enableActions(MainWindow::PatternAction);
    auto *restraints = widget<RestraintsDialog>(window);
    auto *csm = widget<QPushButton>(*restraints,"csmMethodButton");
    auto *pfsm = widget<QPushButton>(*restraints,"pfsmMethodButton");
    auto *formula = widget<QLineEdit>(*restraints,"compositionFormula");
    auto *search = widget<QPushButton>(*restraints,"searchButton");
    auto *minimumSpecies = widget<QSpinBox>(*restraints,"minSpecies");
    auto *tabs = widget<QTabWidget>(*restraints,"tabWidget");
    auto *options = widget<PfsmOptionsDialog>(window,"pfsmOptionsDialog");
    auto *results = widget<DbResultsWidget>(window);
    auto *table = widget<QTableView>(*results,"table");
    auto *quant = widget<QuantWidget>(window);
    QString capture;
    if (argc == 3 && std::string(argv[1]) == "--capture") { capture = QString::fromLocal8Bit(argv[2]); check(QDir().mkpath(capture)); }
    const auto snap = [&](QWidget *w, const char *name) {
        if (capture.isEmpty()) return;
        QEventLoop paint;
        QTimer::singleShot(30,&paint,&QEventLoop::quit);
        paint.exec(QEventLoop::ExcludeUserInputEvents);
        check(w->grab().save(capture + "/" + name));
        if (w == options) for (auto *text : w->findChildren<QLabel *>())
            if (text->wordWrap() && text->isVisible() && text->height() < text->heightForWidth(text->width()))
                throw std::runtime_error("PFSM GUI clipped wrapped label: " + text->objectName().toStdString());
    };

    widget<QAction>(window,"actionRestraints")->trigger();
    check(csm->isChecked() && !pfsm->isChecked() && !restraints->hasRestraints() && !search->isEnabled());
    pfsm->click(); check(pfsm->isChecked() && !csm->isChecked() && !search->isEnabled());
    check(pfsm->geometry().right() < minimumSpecies->geometry().left());
    tabs->setCurrentIndex(1); tabs->setCurrentIndex(0); check(pfsm->isChecked());
    minimumSpecies->setValue(1); check(search->isEnabled());
    widget<QPushButton>(*restraints,"cancelRestraintsButton")->click();
    check(pfsm->isChecked() && !restraints->hasRestraints() && !search->isEnabled());
    restraints->reject(); widget<QAction>(window,"actionRestraints")->trigger();
    check(csm->isChecked() && session.snapshot()->algorithms->spec.method() == SearchMethod::classic_sm);
    formula->setText("Si"); pfsm->click(); snap(restraints,"restraints-pfsm.png"); restraints->reject();

    widget<QAction>(window,"actionPFSMOptions")->trigger();
    check(options->isVisible() && widget<QLabel>(*options,"pfsmFwhmSource")->text().contains("Geometric"));
    check(widget<QLabel>(*options,"pfsmResolvedWeights")->text().contains("noise unknown"));
    check(widget<QSpinBox>(*options,"pfsmWorkers")->value() == 0 && widget<QCheckBox>(*options,"pfsmTemplateCache")->isChecked());
    snap(options,"pfsm-options.png");
    auto *optionTabs=widget<QTabWidget>(*options,"pfsmOptionsTabs");
    auto *recallOn=widget<QCheckBox>(*options,"pfsmRecallEnabled");
    auto *resourceFallback=widget<QComboBox>(*options,"pfsmResourceFallback");
    check(resourceFallback->count()==2 && resourceFallback->currentIndex()==0);
    auto *recallCount=widget<QSpinBox>(*options,"recallCandidates");
    auto *shortlist=widget<QSpinBox>(*options,"localShortlist");
    auto *channel=widget<QSpinBox>(*options,"protectedPerChannel");
    auto *review=widget<QSpinBox>(*options,"reviewLimit");
    auto *formal=widget<QSpinBox>(*options,"formalLimit");
    auto *optionButtons=widget<QDialogButtonBox>(*options,"pfsmOptionsButtons");
    auto *restore=optionButtons->button(QDialogButtonBox::RestoreDefaults);
    qualpf::gui::Preferences preferences(window.context().preferences());
    const auto untouched=session.snapshot();
    check(optionTabs->count()==2 && optionTabs->currentIndex()==0 && !widget<QToolButton>(*options,"pfsmAdvanced")->isChecked());
    check(recallOn->isChecked() && recallCount->value()==500 && shortlist->value()==20 && channel->value()==5 && review->value()==80 && formal->value()==80);
    QKeyEvent switchTab(QEvent::KeyPress,Qt::Key_Tab,Qt::ControlModifier);
    QApplication::sendEvent(optionTabs,&switchTab);check(optionTabs->currentIndex()==1);
    recallOn->setFocus();
    QKeyEvent nextField(QEvent::KeyPress,Qt::Key_Tab,Qt::NoModifier);
    QApplication::sendEvent(recallOn,&nextField);check(options->focusWidget()==resourceFallback);
    QApplication::sendEvent(resourceFallback,&nextField);check(options->focusWidget()==recallCount);
    snap(options,"pfsm-options-recall.png");
    if (!capture.isEmpty()) {
        const auto size=options->size();
        options->resize(400,420);
        snap(options,"pfsm-options-small-top.png");
        auto *scroll=widget<QScrollArea>(*options,"pfsmRecallScroll");
        check(scroll->verticalScrollBar()->maximum()>0);
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        snap(options,"pfsm-options-small-bottom.png");
        const auto *ok=optionButtons->button(QDialogButtonBox::Ok);
        check(options->rect().contains(ok->mapTo(options,ok->rect().bottomRight())));
        options->resize(size);scroll->verticalScrollBar()->setValue(0);
        optionTabs->setCurrentIndex(0);widget<QToolButton>(*options,"pfsmAdvanced")->click();
        snap(options,"pfsm-options-advanced.png");
        widget<QToolButton>(*options,"pfsmAdvanced")->click();optionTabs->setCurrentIndex(1);
    }
    recallOn->setChecked(false);check(!recallCount->isEnabled() && !resourceFallback->isEnabled() && recallCount->value()==500);
    recallOn->setChecked(true);recallCount->setValue(10);optionTabs->setCurrentIndex(0);options->accept();
    check(options->isVisible() && optionTabs->currentIndex()==1 && !widget<QLabel>(*options,"localShortlistError")->text().isEmpty());
    check(options->focusWidget()==shortlist);snap(options,"pfsm-options-validation.png");
    check(session.snapshot()==untouched && !preferences.contains("PFSMOptions/candidateRecall/enabled"));
    widget<QCheckBox>(*options,"pfsmTemplateCache")->setChecked(false);
    restore->click();check(recallCount->value()==500 && recallOn->isChecked() && widget<QCheckBox>(*options,"pfsmTemplateCache")->isChecked());options->reject();check(session.snapshot()==untouched);

    // Options belong to this project; another process retains the factory defaults.
    widget<QAction>(window,"actionPFSMOptions")->trigger();
    resourceFallback->setCurrentIndex(1);
    widget<QCheckBox>(*options,"pfsmTemplateCache")->setChecked(false);
    recallCount->setValue(600);shortlist->setValue(30);channel->setValue(6);review->setValue(90);formal->setValue(100);
    recallOn->setChecked(false);options->accept();check(!options->isVisible());
    check(!preferences.contains("PFSMOptions/candidateRecall/enabled"));
    QProcess restarted;
    restarted.start(QCoreApplication::applicationFilePath(), {"--verify-isolated-preference",temporary.path()});
    check(restarted.waitForFinished(10000) && restarted.exitStatus()==QProcess::NormalExit && restarted.exitCode()==0);
    {
        PfsmOptionsDialog reopened(&window);check(reopened.setOptions());
        check(!widget<QCheckBox>(reopened,"pfsmRecallEnabled")->isChecked() && widget<QSpinBox>(reopened,"recallCandidates")->value()==600);
        check(widget<QComboBox>(reopened,"pfsmResourceFallback")->currentIndex()==1);
        check(!widget<QCheckBox>(reopened,"pfsmTemplateCache")->isChecked());
    }
    auto clean=*session.snapshot();clean.searchOptions=untouched->searchOptions;
    check(bool(session.commit(session.snapshot(),clean)));
    {
        PfsmOptionsDialog reopened(&window);check(reopened.setOptions());
        check(widget<QCheckBox>(reopened,"pfsmRecallEnabled")->isChecked() && widget<QSpinBox>(reopened,"recallCandidates")->value()==500);
    }
    widget<QAction>(window,"actionPFSMOptions")->trigger();restore->click();options->reject();
    check(!preferences.contains("PFSMOptions/candidateRecall/enabled"));
    // Bad stored counts recover locally, retain the off choice and never write on read/cancel.
    preferences.setValue("PFSMOptions/candidateRecall/recallCandidates",qlonglong(2147483648LL));
    preferences.setValue("PFSMOptions/candidateRecall/future","retain unknown value");
    auto badProject=*session.snapshot();
    badProject.searchOptions["PFSMOptions/candidateRecall/enabled"]=false;
    badProject.searchOptions["PFSMOptions/candidateRecall/recallCandidates"]=std::int64_t(2147483648LL);
    check(bool(session.commit(session.snapshot(),std::move(badProject))));
    const auto beforeBad=session.snapshot();
    widget<QAction>(window,"actionPFSMOptions")->trigger();
    check(!recallOn->isChecked() && recallCount->value()==500 && shortlist->value()==20);
    check(widget<QLabel>(*options,"pfsmRecallWarning")->isVisible() && !widget<QLabel>(*options,"pfsmRecallWarning")->text().isEmpty());options->reject();
    check(session.snapshot()==beforeBad && preferences.value("PFSMOptions/candidateRecall/recallCandidates").toLongLong()==2147483648LL);
    widget<QAction>(window,"actionPFSMOptions")->trigger();restore->click();options->accept();
    check(preferences.value("PFSMOptions/candidateRecall/future").toString()=="retain unknown value");
    widget<QAction>(window,"actionPFSMOptions")->trigger();
    auto *lower = widget<QLineEdit>(*options,"lowerShift"), *upper = widget<QLineEdit>(*options,"upperShift");
    lower->setText("1"); upper->setText("-1"); options->accept(); check(options->isVisible());
    lower->setText("-0.500"); upper->setText("0.500"); widget<QSpinBox>(*options,"pfsmWorkers")->setValue(1); options->accept();
    check(!options->isVisible());
    const auto before = session.snapshot();
    widget<QAction>(window,"actionRestraints")->trigger(); pfsm->click();
    SearchMethod submitted = SearchMethod::classic_sm;
    QObject::connect(restraints,&RestraintsDialog::searchWithRestraintsRequested,&window,[&](SearchMethod m) { submitted = m; });
    run(window,[&] { search->click(); check(!pfsm->isEnabled() && !search->isEnabled()); });
    const auto analyzed = session.snapshot();
    check(submitted == SearchMethod::pfsm && analyzed->pfsm && analyzed != before && analyzed->peaks.tth.empty());
    check(initial->calls == 0 && record->reads == 1 && record->execution != std::this_thread::get_id());
    check(record->query.route == CandidateQuery::Route::constrained_browse && record->query.d.empty() && record->query.strictPairedReflections);
    check(!record->query.excludeDeleted); // CSM Options cannot change the PFSM universe.
    check(analyzed->candidates.front().id == "101" && analyzed->candidates.size() == 4);
    check(analyzed->pfsm->problem->candidate_recall.mode==PfsmCandidateRecallMode::enabled);
    check(analyzed->pfsm->candidate_recall_reports.back().fallback_reason=="small_pool");
    check(analyzed->pfsm->problem->physical_width->origin == FwhmOrigin::geometric && !analyzed->pfsm->problem->physical_width->manual);
    check(std::abs(analyzed->pfsm->problem->physical_width->width.value - .2) < .001);
    check(results->sourceModel->headerData(5,Qt::Horizontal).toString() == "Rwp(net) %");
    check(results->sourceModel->headerData(9,Qt::Horizontal).toString() == "ΔRwp pp");
    check(results->sourceModel->index(0,8).data().toDouble() == 1);
    check(qualpf::gui::productState(*AppState::searchSession().snapshot()).pfsm == analyzed->pfsm);
    check(CardType(analyzed->candidates.front()).productValue().pfsm == analyzed->candidates.front().pfsm);
    auto *plot = window.xpdViewer();
    check(plot->yAxis->label().contains(analyzed->pfsm->problem->source.intensity_units.c_str()) && plot->calc.graphIndex >= 0);
    check(plot->graph(plot->calc.graphIndex)->data()->size() == 701);
    check(plot->graph(plot->obs.front().graphIndex)->data()->begin()->value == analyzed->pattern.observed.front());
    auto *backgroundDisplay = widget<QAction>(window,"actionSubtract_Background");
    check(backgroundDisplay->isEnabled() && !backgroundDisplay->isChecked());
    const auto frozenFit = analyzed->pfsm;
    const auto totalFit = plot->graph(plot->calc.graphIndex)->data()->begin()->value;
    backgroundDisplay->trigger();
    check(session.snapshot() == analyzed && session.snapshot()->pfsm == frozenFit && backgroundDisplay->isChecked());
    check(plot->graph(plot->obs.front().graphIndex)->data()->begin()->value == analyzed->pattern.observed.front()-35);
    check(plot->graph(plot->calc.graphIndex)->data()->begin()->value == totalFit-35);
    backgroundDisplay->trigger();
    check(session.snapshot() == analyzed && plot->graph(plot->calc.graphIndex)->data()->begin()->value == totalFit);
    snap(results,"pfsm-candidates.png"); snap(window.xpdViewer(),"pfsm-joint-trial.png");

    // Confirmation cancellation leaves canonical evidence and method untouched.
    const auto preserved = session.snapshot(); answer(QMessageBox::No);
    widget<QAction>(window,"actionSearch_Match")->trigger(); check(session.snapshot() == preserved);
    restraints->reject();
    widget<QAction>(window,"actionPFSMOptions")->trigger();
    check(widget<QLabel>(*options,"pfsmCurrentRecall")->text().contains("Current analysis (frozen): On"));
    shortlist->setValue(5);channel->setValue(1);recallCount->setValue(30);review->setValue(10);formal->setValue(10);
    recallOn->setChecked(false);
    widget<QDoubleSpinBox>(*options,"minimumRelativeFom")->setValue(1); options->accept();
    check(table->model()->rowCount() == 1 && session.snapshot()->pfsm == analyzed->pfsm);
    widget<QAction>(window,"actionPFSMOptions")->trigger();
    widget<QDoubleSpinBox>(*options,"minimumRelativeFom")->setValue(0); options->accept();
    auto *pages = widget<PaginationModel>(*results); pages->setMaxRows(1); pages->setCurrentPage(1);
    check(session.snapshot()->pfsm->normalization_maximum == analyzed->pfsm->normalization_maximum);
    const auto id = table->model()->index(0,2).data().toString().toStdString();
    const auto found = std::find_if(analyzed->candidates.begin(),analyzed->candidates.end(),[&](const auto &c) { return c.id == id; });
    check(found != analyzed->candidates.end() && table->model()->index(0,8).data().toDouble() == pfsmCandidateMetrics(*analyzed,*found)->relative_fom);
    pages->setMaxRows(500);
    table->horizontalHeader()->resizeSection(5,137);
    table->horizontalHeader()->sectionClicked(6);
    check(session.snapshot()->pfsm == analyzed->pfsm && table->horizontalHeader()->sortIndicatorSection() == 6);
    run(window,[&] { widget<QAction>(window,"actionRecalculate_FOM")->trigger(); });
    check(session.snapshot()->pfsm->problem == analyzed->pfsm->problem && record->reads == 1);
    run(window,[&] { results->selectCard("101"); });
    run(window,[&] { results->acceptSelectedCards(); });
    check(session.snapshot()->phases.size() == 1 && session.snapshot()->phases[0].id == "101" && record->reads == 1);
    check(session.snapshot()->pfsm->problem==analyzed->pfsm->problem);
    check(session.snapshot()->phases[0].tth.empty() && plot->refSet.isEmpty());
    hover(*plot); // Original PFSM d/I-only phases produced an empty hover row.
    check(!widget<PieChartWidget>(*quant)->isVisible());
    auto *phases = widget<QTableView>(*quant,"tableView");
    check(phases->model()->columnCount() == 7 && phases->model()->headerData(3,Qt::Horizontal).toString() == "wt.%");
    snap(quant,"pfsm-selected-phases.png");
    phases->setCurrentIndex(phases->model()->index(0,1));
    run(window,[&] { check(QMetaObject::invokeMethod(quant,"removeSelectedPhase",Qt::DirectConnection)); });
    check(session.snapshot()->phases.empty() && session.snapshot()->candidates.size() == 4 && record->reads == 1);
    check(session.snapshot()->pfsm->problem==analyzed->pfsm->problem);

    // Repeat complete native GUI transactions against the independent analytic
    // fixture. Preview is ephemeral; accepting/removing are atomic publications.
    std::vector<double> refreshMilliseconds, previewMilliseconds;
    const int steadyGraphs = plot->graphCount(), steadyItems = plot->itemCount();
    for (int iteration = 0; iteration < 20; ++iteration) {
        answer(QMessageBox::Yes);
        run(window,[&] { widget<QAction>(window,"actionPFSM")->trigger(); });
        check(session.snapshot()->pfsm && session.snapshot()->candidates.size() == 4 && session.snapshot()->phases.empty());
        const auto source = session.snapshot();
        QElapsedTimer timer; timer.start();
        results->refresh();
        refreshMilliseconds.push_back(timer.nsecsElapsed() / 1.e6);
        waitUntil([&] { return !window.searchTasks()->busy(); });
        timer.restart();
        // All requests arrive before queued completion. Only the final request
        // may remain visible, even if the first worker already finished fitting.
        results->selectCard("101"); results->selectCard("102"); results->selectCard("103");
        if (iteration == 0) { backgroundDisplay->trigger(); check(backgroundDisplay->isChecked() && session.snapshot() == source); }
        waitUntil([&] { return !window.searchTasks()->busy(); });
        if (iteration == 0) {
            check(plot->graph(plot->obs.front().graphIndex)->data()->begin()->value == source->pattern.observed.front()-35);
            backgroundDisplay->trigger(); check(!backgroundDisplay->isChecked() && session.snapshot() == source);
        }
        previewMilliseconds.push_back(timer.nsecsElapsed() / 1.e6);
        check(plot->graph(plot->calc.graphIndex)->name().contains("103") && session.snapshot() == source);
        hover(*plot);
        run(window,[&] { results->selectCard("101"); });
        run(window,[&] { results->acceptSelectedCards(); });
        check(session.snapshot()->phases.size() == 1 && session.snapshot()->phases.front().id == "101");
        check(session.snapshot()->phases.front().tth.empty() && plot->refSet.isEmpty());
        if (iteration == 0) {
            const auto beforeRir = session.snapshot();
            const auto displayedName = plot->graph(plot->calc.graphIndex)->name();
            const auto revised = preparePfsmRirRevision(beforeRir, "101", PfsmRirRevision{4,"display-cache test"});
            check(bool(revised) && bool(session.commit(beforeRir, revised.value())));
            const auto afterRir = session.snapshot();
            backgroundDisplay->trigger(); backgroundDisplay->trigger();
            check(session.snapshot() == afterRir && afterRir->pfsm->generation == beforeRir->pfsm->generation &&
                plot->graph(plot->calc.graphIndex)->name() == displayedName);
        }
        hover(*plot);
        phases->setCurrentIndex(phases->model()->index(0,1));
        run(window,[&] { check(QMetaObject::invokeMethod(quant,"removeSelectedPhase",Qt::DirectConnection)); });
        check(session.snapshot()->phases.empty() && plot->refSet.isEmpty());
        hover(*plot);
        check(plot->graphCount() == steadyGraphs && plot->itemCount() == steadyItems);
    }
    const auto report = [](const char *name, std::vector<double> values) {
        std::sort(values.begin(), values.end());
        std::cout << name << " ms (20 cycles): median=" << values[values.size()/2]
                  << ", max=" << values.back() << '\n';
    };
    report("Result refresh GUI", refreshMilliseconds);
    report("Latest candidate preview including worker", previewMilliseconds);

    // A preview must also reject a replaced source, and close must discard a
    // pending latest selection without publishing it or accessing dead labels.
    run(window,[&] { results->selectCard("103"); });
    results->selectCard("102");
    check(window.searchTasks()->busy());
    const auto previewSource = session.snapshot();
    check(bool(session.commit(previewSource, *previewSource)));
    const auto replacement = session.snapshot();
    waitUntil([&] { return !window.searchTasks()->busy(); });
    check(session.snapshot() == replacement);
    if (argc == 2 && std::string(argv[1]) == "--close-preview") {
        results->selectCard("101"); results->selectCard("102");
        check(window.searchTasks()->busy());
        const auto source = session.snapshot();
        answer(QMessageBox::Yes); window.close();
        hover(*plot);
        waitUntil([&] { return !window.searchTasks()->busy(); });
        check(window.isClosing() && session.snapshot() == source);
        std::cout << "PASS: PFSM pending latest preview close/drain\n";
        return 0;
    }

    // A pool above the default recall target proves real GUI screening, not just small-pool fallback.
    widget<QAction>(window,"actionPFSMOptions")->trigger();restore->click();
    lower->setText("0");upper->setText("0");widget<QSpinBox>(*options,"pfsmWorkers")->setValue(2);options->accept();
    for (int i=0;i<600;++i) {
        const auto id=std::to_string(1000+i);
        record->extra.push_back(card(id.c_str(),"Independent recall decoy",28.05+.001*i));
    }
    answer(QMessageBox::Yes);run(window,[&] { widget<QAction>(window,"actionPFSM")->trigger(); });
    const auto screened=session.snapshot();
    const auto &screening=screened->pfsm->candidate_recall_reports.front();
    check(screened->pfsm->problem->candidate_recall.mode==PfsmCandidateRecallMode::enabled && screening.pool_size==604);
    check(screening.recalled_indices.size()<604 && screening.selection && screening.limits==profile::PfsmCandidateRecallLimits{});
    check(screened->candidates.size()<604 && !screened->pfsm->normalization_over_full_pool);
    widget<QAction>(window,"actionPFSMOptions")->trigger();optionTabs->setCurrentIndex(1);snap(options,"pfsm-options-frozen.png");
    recallOn->setChecked(false);snap(options,"pfsm-options-disabled.png");options->accept();
    check(session.snapshot()->pfsm==screened->pfsm);
    answer(QMessageBox::Yes);run(window,[&] { widget<QAction>(window,"actionPFSM")->trigger(); });
    check(session.snapshot()->pfsm->problem->candidate_recall.mode==PfsmCandidateRecallMode::off && session.snapshot()->candidates.size()==604);
    record->extra.clear();

    // A cooperative preparation worker must drain without publishing on close.
    auto fresh = state(resolved.value()); storePfsmSettings(fresh.searchOptions,PfsmSettings{});
    fresh.searchOptions["PFSMOptions/workers"] = 1;
    check(bool(session.commit(session.snapshot(),std::move(fresh))));
    results->refresh(); check(results->sourceModel->headerData(5,Qt::Horizontal).toString() == "Peakpos.");
    table->horizontalHeader()->resizeSection(5,139); table->horizontalHeader()->sectionClicked(9);
    auto noAnalysis = prepareSearchMethod(session.snapshot(),SearchMethod::pfsm,false); check(bool(noAnalysis));
    check(bool(session.commit(session.snapshot(),std::move(noAnalysis).value())));
    results->refresh();
    check(table->horizontalHeader()->sectionSize(5) == 137 && table->horizontalHeader()->sortIndicatorSection() == 6 &&
        table->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder);
    window.enableActions(MainWindow::PatternAction); background->entered = false; background->block = true;
    restraints->reject(); widget<QAction>(window,"actionRestraints")->trigger();
    check(csm->isChecked()); restraints->reject(); // No analysis defaults to CSM even after an unfinished PFSM entry.
    widget<QAction>(window,"actionPFSM")->trigger(); waitUntil([&] { return background->entered.load(); });
    const auto closingSource = session.snapshot(); answer(QMessageBox::Yes); window.close();
    hover(*plot); // Close/drain suppresses cursor publication while labels live.
    waitUntil([&] { return !window.searchTasks()->busy(); });
    check(window.isClosing() && session.snapshot() == closingSource && !session.snapshot()->pfsm);
    std::cout << "PASS: PFSM controls, 20 search/preview/accept/remove cycles, hover, latest selection and close/drain\n";
    return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
