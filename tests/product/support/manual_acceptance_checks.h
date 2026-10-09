#include "infrastructure/serialization/metadata_values.h"
#include "tests/support/project_peaks.h"
#include "gui/dialogs/plotstyledialog.h"
#include <QRadioButton>

namespace {
void searchSnapshot96(MainWindow &window, const QString &path, const QString &output) {
    ProductWorkflowProbe::load(window, path);
    const auto before = AppState::searchSession().snapshot();
    require(before->peaks.valid && !before->peaks.tth.empty(), "Snapshot needs valid saved peaks");
    ProductWorkflowProbe::search(window);
    require(AppState::searches().searchError().isEmpty(), "Snapshot search failed");
    const auto after = AppState::searchSession().snapshot();
    require(before->peaks.tth == after->peaks.tth && before->peaks.intensity == after->peaks.intensity,
            "Search changed peak inputs");
    require(qualpf::gui::encode_project_peaks(before->peaks) ==
                qualpf::gui::encode_project_peaks(after->peaks),
            "Search rewrote the saved peak snapshot");
    int retainedWithStoredWindows = 0;
    QStringList searchConnections;
    for (const auto &name : QSqlDatabase::connectionNames())
        if (name.endsWith(".sq.search")) searchConnections.append(name);
    require(searchConnections.size() == 1, "Expected one active strongest index");
    QSqlQuery storedTop(QSqlDatabase::database(searchConnections.front()));
    for (const auto &card : after->candidates) {
        storedTop.prepare("SELECT n, dval FROM top WHERE id = :id");
        storedTop.bindValue(":id", card.getId());
        require(storedTop.exec() && storedTop.next(), "Stored top index unavailable");
        const auto strongest = SearchUtil::extractNumbers(storedTop.value(1).toString(), storedTop.value(0).toInt(), 3);
        if (SearchUtil::checkStrongValuesWithTolerance(before->peaks.d, before->peaks.deltaD, strongest))
            ++retainedWithStoredWindows;
    }
    write96(output+".json", {{"source", path}, {"backend", after->backend == qualpf::application::SearchBackend::cpp ? "cpp" : "legacy"},
        {"peaks", qualpf::gui::encode_project_peaks(after->peaks)},
        {"search_options", qualpf::infrastructure::jsonValue(after->searchOptions)}, {"delta", SearchOptionsDialog::savedDelta2theta(AppState::workspace().session().snapshot(), AppState::context().settings())},
        {"results", scores(after->candidates)}, {"count", after->candidates.size()},
        {"retained_with_stored_windows", retainedWithStoredWindows}});
    std::cout << "Snapshot search count: " << after->candidates.size() << std::endl;
    const double effectiveDelta = SearchOptionsDialog::savedDelta2theta(AppState::workspace().session().snapshot(), AppState::context().settings());
    SearchOptionsDialog options(&window);
    options.show();
    options.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    require(SearchOptionsDialog::savedDelta2theta(AppState::workspace().session().snapshot(), AppState::context().settings()) == effectiveDelta, "Confirm changed effective tolerance");
    ProductWorkflowProbe::search(window);
    require(AppState::searches().searchError().isEmpty(), "Search after options confirmation failed");
    const auto confirmed = AppState::searchSession().snapshot();
    require(before->peaks.tth == confirmed->peaks.tth && before->peaks.intensity == confirmed->peaks.intensity,
            "Confirmation changed measured peaks");
    write96(output+"-confirmed.json", {{"delta", effectiveDelta},
        {"count_before", after->candidates.size()}, {"count_after", confirmed->candidates.size()},
        {"same_delta_d", after->peaks.deltaD == confirmed->peaks.deltaD},
        {"same_results", scores(after->candidates) == scores(confirmed->candidates)},
        {"results", scores(confirmed->candidates)}});
    std::cout << "Unchanged options confirmation search count: " << confirmed->candidates.size() << std::endl;
    require(scores(after->candidates) == scores(confirmed->candidates),
            "Unchanged effective search options changed candidates or scores");
}

void toleranceAcceptance96(MainWindow &window, const QString &path, const QString &output) {
    for (bool automatic : {false, true}) {
        auto project = read96(path);
        auto options = project["search_options"].toObject();
        options["SearchOptions/delta2thetaAuto"] = automatic;
        project["search_options"] = options;
        const auto prefix = output + (automatic ? "-auto" : "-fixed");
        write96(prefix+"-input.qxp", project);
        searchSnapshot96(window, prefix+"-input.qxp", prefix);
        const auto expected = scores(AppState::searchSession().snapshot()->candidates);
        require(!expected.empty(), "Tolerance acceptance requires a successful search");
        ProductWorkflowProbe::save(window, prefix+"-saved.qxp");
        ProductWorkflowProbe::load(window, prefix+"-saved.qxp");
        ProductWorkflowProbe::search(window);
        require(scores(AppState::searchSession().snapshot()->candidates) == expected,
                "Reload changed tolerance search results");
    }
    const auto source = AppState::searchSession().snapshot();
    auto invalid = buildSearchMatchQuery(AppState::context(), source->peaks);
    invalid.setDelta2theta(0);
    QVector<CardType> unpublished;
    AppState::searches().makeQueryStrongest(invalid, unpublished);
    require(!AppState::searches().searchError().isEmpty() && unpublished.empty() &&
                AppState::searchSession().snapshot() == source, "Invalid tolerance was not rejected atomically");
}

void manualAcceptance96(MainWindow &window, const QString &path, const QString &output) {
    ProductWorkflowProbe::load(window, path);
    auto *plot = window.xpdViewer();
    auto *results = window.findChild<DbResultsWidget *>();
    require(results && results->hasResults(), "Acceptance fixture needs saved results");
    QJsonArray checks;
    auto check = [&](const char *name, bool ok) {
        checks.append(QJsonObject{{"check", name}, {"passed", ok}});
        std::cout << name << ": " << (ok ? "PASS" : "FAIL") << std::endl;
    };
    const auto original = AppState::searchSession().snapshot();
    const auto card = original->candidates.front();
    auto overlayPresent = [&] {
        for (int i=0; i<plot->graphCount(); ++i)
            if (plot->graph(i)->name() == card.getId()) return true;
        return false;
    };
    results->selectFirstCard();
    check("selected card overlay drawn", overlayPresent());
    results->refresh();
    check("table refresh clears selection overlay", !overlayPresent());

    AppState::searchSession().replacePhases({card});
    for (int i=0; i<4; ++i) { results->selectFirstCard(); results->refresh(); }
    results->selectFirstCard();
    plot->drawSelectedComparePoints({card.getTth().front()}, {10.0}, {Qt::red});
    check("overlay graph wavelengths remain aligned", plot->graphCount() == plot->plotWave.size());

    for (auto radio : {"dRadioButton", "oneOverDRadioButton", "oneOverD2RadioButton", "tthetaRadioButton"}) {
        PlotStyleDialog dialog(&window);
        dialog.setOptions(plot);
        dialog.findChild<QRadioButton *>(radio)->click();
        dialog.apply(plot);
        bool found = false;
        const auto full = plot->graph(plot->obs[0].graphIndex)->getKeyRange(found);
        require(found, "Observed range missing");
        plot->xAxis->setRange(full.lower + .2*full.size(), full.lower + .5*full.size());
        window.findChild<QAction *>("actionReset_Zoom")->trigger();
        const auto reset = plot->xAxis->range();
        std::cout << radio << " expected " << full.lower << ',' << full.upper
                  << " reset " << reset.lower << ',' << reset.upper << std::endl;
        check(radio, std::abs(reset.lower-full.lower)<1e-6 && std::abs(reset.upper-full.upper)<1e-6);
        check("coordinate changes preserve wavelength mapping", plot->graphCount() == plot->plotWave.size());
        const auto &bar = plot->refSet.front();
        for (int i=bar.itemIndexStart; i<=bar.itemIndexEnd; ++i) {
            const auto *line = qobject_cast<QCPItemLine *>(plot->item(i));
            require(line && std::isfinite(line->start->key()), "Invalid accepted phase marker");
        }
    }
    // Accepted bars and selected overlays must follow the same Session reset.
    AppState::searchSession().replacePhases({card});
    plot->addPhaseReflections(card, cardColor(card.getId()));
    results->selectFirstCard();
    if (qualpf::gui::cppPreprocessingProduct()) {
        bool restarted = false;
        QTimer driver;
        QObject::connect(&driver, &QTimer::timeout, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->objectName() != "preprocessingRecoveryDialog") return;
            driver.stop(); restarted = true;
            dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        });
        driver.start(10);
        window.findChild<QAction *>("actionRestartPreprocessing")->trigger();
        check("restart exercised", restarted);
        check("restart clears selected overlay and accepted bars", !overlayPresent() && plot->refSet.empty());
    }
    ProductWorkflowProbe::load(window, path);
    results->selectFirstCard();
    AppState::searchSession().replacePhases({card});
    plot->addPhaseReflections(card, cardColor(card.getId()));
    // Load a distinct saved project with no result or phase data.
    auto empty = read96(path);
    empty["results"] = QJsonArray{};
    empty["phases"] = QJsonArray{};
    write96(output+"-empty.qxp", empty);
    ProductWorkflowProbe::load(window, output+"-empty.qxp");
    check("open different project clears old overlays", !overlayPresent() && plot->refSet.empty());
    check("open different project clears reflection items", plot->itemCount() == 0);
    ProductWorkflowProbe::load(window, path);
    results->selectFirstCard();
    AppState::searchSession().replacePhases({card});
    plot->addPhaseReflections(card, cardColor(card.getId()));
    QTimer discard;
    QObject::connect(&discard, &QTimer::timeout, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget()))
            if (auto *button = box->button(QMessageBox::Discard)) button->click();
    });
    discard.start(10);
    window.findChild<QAction *>("actionNew")->trigger();
    discard.stop();
    check("new project clears old overlays", !overlayPresent() && plot->refSet.empty());
    check("new project clears reflection items", plot->itemCount() == 0);
    check("new project clears authoritative state", AppState::searchSession().snapshot()->pattern.two_theta.empty() &&
        AppState::searchSession().snapshot()->candidates.empty() && AppState::searchSession().snapshot()->phases.empty());
    write96(output+".json", {{"checks", checks}});
    for (const auto &entry : checks) require(entry.toObject()["passed"].toBool(), "Manual acceptance regression failed");
}
}
