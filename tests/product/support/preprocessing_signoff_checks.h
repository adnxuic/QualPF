#include "tests/support/project_peaks.h"
// Phase 9.6: current product contracts, independently of Legacy historical success.
// All calls below use the ordinary C++ product backend, never the scoped QA chain.
#include <QMessageBox>

namespace {
void backend96() {
    require(qualpf::gui::cppPreprocessingProduct(), "C++ product backend is required");
    require(!qEnvironmentVariableIsSet("QUALPF_CPP_PREPROCESSING_DIALOGS"), "QA chain must be unset");
    require(AppState::searchSession().snapshot()->backend == qualpf::application::SearchBackend::cpp,
            "C++ Search-Match backend is required");
}

// All authoritative numeric fields, including optional knowledge, are compared.
// Editing/revision are intentionally separate from numeric publication.
void sameState96(const qualpf::gui::SessionStateView &a, const qualpf::gui::SessionStateView &b) {
    const auto &x = a.pattern;
    const auto &y = b.pattern;
    auto signal = [](const auto &p, const auto &q) {
        return p.has_value() == q.has_value() && (!p || (p->values == q->values &&
            p->window.has_value() == q->window.has_value() &&
            (!p->window || p->window->first == q->window->first)));
    };
    require(x.id == y.id && x.source_id == y.source_id && x.two_theta == y.two_theta &&
                x.observed == y.observed && x.original_observed == y.original_observed &&
                x.corrected_two_theta == y.corrected_two_theta && x.weights == y.weights &&
                signal(x.background, y.background) && signal(x.calculated, y.calculated) &&
                signal(x.background_subtracted_signal, y.background_subtracted_signal) &&
                x.processing.background_subtracted == y.processing.background_subtracted &&
                x.processing.intensity_scaled == y.processing.intensity_scaled &&
                x.processing.kalpha2_subtracted == y.processing.kalpha2_subtracted &&
                x.intensity_scale == y.intensity_scale && x.source_precision == y.source_precision &&
                x.radiation.kind == y.radiation.kind && x.radiation.lines.size() == y.radiation.lines.size() &&
                x.calculation_range.has_value() == y.calculation_range.has_value(), "Pattern changed");
    if (x.calculation_range)
        require(x.calculation_range->minimum == y.calculation_range->minimum &&
                    x.calculation_range->maximum == y.calculation_range->maximum, "Range changed");
    for (std::size_t i = 0; i < x.radiation.lines.size(); ++i)
        require(x.radiation.lines[i].wavelength == y.radiation.lines[i].wavelength &&
                    x.radiation.lines[i].relative_weight == y.radiation.lines[i].relative_weight,
                "Radiation changed");
    require(qualpf::gui::encode_project_peaks(a.peaks) ==
                qualpf::gui::encode_project_peaks(b.peaks) &&
                qualpf::infrastructure::encodePreprocessing(a.preprocessing) ==
                qualpf::infrastructure::encodePreprocessing(b.preprocessing) &&
                a.automaticDelta == b.automaticDelta && a.peakTolerance == b.peakTolerance &&
                a.searchOptions == b.searchOptions && a.restraints == b.restraints &&
                scores(a.candidates) == scores(b.candidates) && scores(a.phases) == scores(b.phases) &&
                a.projectMetadata == b.projectMetadata && a.backend == b.backend,
            "Authoritative peaks/configuration/search state changed");
}

QJsonArray graphs96(MainWindow &window) {
    QJsonArray graphs;
    auto *plot = window.xpdViewer();
    for (int i = 0; i < plot->graphCount(); ++i) {
        auto *g = plot->graph(i);
        QJsonArray points;
        for (auto it = g->data()->constBegin(); it != g->data()->constEnd(); ++it)
            points.append(QJsonArray{it->key, it->value});
        graphs.append(QJsonObject{{"name", g->name()}, {"visible", g->visible()}, {"points", points}});
    }
    return graphs;
}

void noPreview96(MainWindow &window) {
    for (int i = 0; i < window.xpdViewer()->graphCount(); ++i)
        require(window.xpdViewer()->graph(i)->name() != "Preprocessing preview", "Stale preview overlay");
}

void apply96(QDialog &dialog) {
    require(dialog.property("preprocessingBackend") == "cpp", "Dialog did not use product C++");
    dialog.show();
    QApplication::processEvents();
    auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
    if (!ok->isEnabled())
        throw std::runtime_error("Apply disabled: " + dialog.property("preprocessingReason").toString().toStdString());
    ok->setFocus();
    ok->click();
    require(dialog.result() == QDialog::Accepted && !AppState::searchSession().snapshot()->editing,
            "Dialog did not commit");
}

void write96(const QString &path, const QJsonObject &value) {
    QFile file(path);
    require(file.open(QIODevice::WriteOnly), "Cannot write Phase 9.6 report");
    const auto bytes = QJsonDocument(value).toJson();
    require(file.write(bytes) == bytes.size(), "Incomplete Phase 9.6 report");
}

QJsonObject read96(const QString &path) {
    QFile file(path);
    require(file.open(QIODevice::ReadOnly), "Cannot read Phase 9.6 artifact");
    QJsonParseError error;
    auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    require(error.error == QJsonParseError::NoError && doc.isObject(), "Invalid Phase 9.6 artifact");
    return doc.object();
}

void close96(MainWindow &window) {
    QTimer driver;
    bool confirmed = false;
    QObject::connect(&driver, &QTimer::timeout, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            driver.stop();
            confirmed = true;
            box->button(QMessageBox::Yes)->click();
        }
    });
    driver.start(10);
    require(window.close() && confirmed, "Actual close confirmation was not completed");
}

// Actual GUI Search-Match -> SQL -> C++ FOM -> Session -> results widget.
// Deliberately does not call compareCpp(), which also executes the Legacy oracle.
QJsonArray search96(MainWindow &window) {
    backend96();
    auto input = AppState::searchSession().snapshot();
    require(input->peaks.valid && !input->peaks.tth.empty(), "Finite nonempty input peaks required");
    std::optional<qualpf::Error> failure = qualpf::Error{qualpf::ErrorCode::undefined_preprocessing,
        "previous", "previous failure", "coincident_baseline_endpoints"};
    require(ensurePeaksFound(AppState::context(), &failure) == input->peaks.tth.size() && !failure &&
                AppState::searchSession().snapshot() == input, "Search reran preprocessing on saved peaks");
    ProductWorkflowProbe::search(window);
    require(AppState::searches().searchError().isEmpty() && !AppState::searches().isCanceled(), "GUI Search-Match failed");
    const auto result = AppState::searchSession().snapshot();
    require(!result->candidates.empty(), "Successful E2E scenario must return real database cards");
    require(qualpf::gui::encode_project_peaks(input->peaks) ==
                qualpf::gui::encode_project_peaks(result->peaks) &&
                input->pattern.observed == result->pattern.observed &&
                qualpf::infrastructure::encodePreprocessing(input->preprocessing) ==
                qualpf::infrastructure::encodePreprocessing(result->preprocessing), "Search changed input");
    auto *widget = window.findChild<DbResultsWidget *>();
    require(widget && scores(widget->allCards()) == scores(result->candidates), "GUI result table differs");
    for (const auto &card : result->candidates) {
        auto score = qualpf::gui::scoreCard(*input, card, currentSearchParameters(AppState::context()));
        require(bool(score), "Retained card failed C++ scoring");
        close(card.getFom(), score.value().total, 5e-7, 2e-6, "E2E FOM");
        close(card.getScale(), score.value().scale, 1e-7, 2e-6, "E2E scale");
    }
    return scores(result->candidates);
}
} // namespace

void phase96Contract(MainWindow &window, const QString &output) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    backend96();
    auto &session = AppState::searchSession();
    const auto imported = session.snapshot();
    require(!imported->peaks.valid && imported->peaks.tth.empty() &&
                imported->pattern.original_observed && imported->pattern.processing.intensity_scaled == true,
            "Contract must start before any Legacy peak generation");
    const auto graphs = graphs96(window);
    int publications = 0;
    auto connection = QObject::connect(&session, &SessionView::changed, [&] { ++publications; });
    for (auto action : {PreprocessingAction::automatic_peaks, PreprocessingAction::prepare_search}) {
        auto status = applyPreprocessing(session, action);
        require(!status && status.error().code == ErrorCode::undefined_preprocessing &&
                    status.error().reason == "coincident_baseline_endpoints", "Frozen undefined reason changed");
        require(session.snapshot() == imported && publications == 0, "Rejected request published a snapshot");
        sameState96(*session.snapshot(), *imported);
    }
    ProductWorkflowProbe::automaticPeaks(window);
    QString statusText;
    for (auto *label : window.statusBar()->findChildren<QLabel *>()) statusText += label->text();
    require(statusText.contains("coincident_baseline_endpoints"),
            "Automatic menu lost structured reason");
    std::optional<Error> failure;
    require(ensurePeaksFound(AppState::context(), &failure) == 0 && failure &&
                failure->code == ErrorCode::undefined_preprocessing &&
                failure->reason == "coincident_baseline_endpoints" &&
                session.snapshot() == imported && publications == 0,
            "Search preparation silently fell back or published partial peaks");
    QString searchMessage;
    QTimer driver;
    QObject::connect(&driver, &QTimer::timeout, [&] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            searchMessage = box->text();
            driver.stop();
            box->accept();
        }
    });
    driver.start(10);
    ProductWorkflowProbe::search(window);
    driver.stop();
    require(searchMessage.contains("coincident_baseline_endpoints") && searchMessage.contains("Restart preprocessing") &&
                searchMessage.contains("Add Peak"), "Search GUI hides undefined reason/recovery instructions");
    require(session.snapshot() == imported && publications == 0 && graphs96(window) == graphs,
            "Failure changed Session or GUI curves");
    QObject::disconnect(connection);
    {
        PeakSearchDialog dialog(&window);
        dialog.setOptions();
        const auto opened = session.snapshot(); // editing barrier, not numerical publication
        sameState96(*opened, *imported);
        auto *diagnostic = dialog.findChild<QLabel *>("preprocessingDiagnostic");
        require(dialog.property("preprocessingBackend") == "cpp" &&
                    dialog.property("preprocessingReason") == "coincident_baseline_endpoints" && diagnostic &&
                    diagnostic->text().contains("Nothing was applied") && diagnostic->text().contains("Cancel") &&
                    diagnostic->text().contains("Restart preprocessing") && diagnostic->text().contains("Add Peak"),
                "Peak conditions lack explicit error/recovery");
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        require(!buttons->button(QDialogButtonBox::Ok)->isEnabled() &&
                    buttons->button(QDialogButtonBox::Cancel)->isEnabled(), "Invalid preview is committable");
        buttons->button(QDialogButtonBox::Ok)->click();
        require(session.snapshot() == opened, "Disabled Apply published");
        noPreview96(window);
        buttons->button(QDialogButtonBox::Cancel)->click();
        sameState96(*session.snapshot(), *imported);
        require(!session.snapshot()->editing && graphs96(window) == graphs, "Cancel left stale GUI state");
    }
    // An ordinary valid edit is available immediately after rejection; its Cancel is atomic.
    {
        SmoothingDialog dialog(&window);
        dialog.setSmoothing();
        require(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled(),
                "Failure made the next valid edit unusable");
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Cancel)->click();
        sameState96(*session.snapshot(), *imported);
        noPreview96(window);
    }
    auto *table = window.findChild<PeakListWidget *>();
    require(table && table->peakListModel->rowCount() == 0 && !session.snapshot()->preprocessing.peak_candidates &&
                window.findChild<QAction *>("actionRestartPreprocessing"), "Rejected candidate/old table leaked");
    backend96();
    write96(output + "-contract.json", {{"status", "passed_current_product_contract"},
        {"error", "undefined_preprocessing"}, {"reason", "coincident_baseline_endpoints"},
        {"request_publications", publications}, {"partial_peaks_published", false},
        {"gui_error", searchMessage}, {"cancel_and_next_edit", "passed"},
        {"preprocessing_backend", "cpp"}, {"qa_flag_set", false},
        {"legacy_historical_equivalence", false}, {"human_signoff", "pending"}});
    std::cout << "PASS: current product undefined contract, zero publication, GUI recovery\n";
}

void phase96Finite(MainWindow &window, const QString &output) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    backend96();
    auto &session = AppState::searchSession();
    const auto imported = session.snapshot();
    QTimer restartDriver;
    bool restarted = false;
    QObject::connect(&restartDriver, &QTimer::timeout, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != "preprocessingRecoveryDialog") return;
        restarted = true;
        restartDriver.stop();
        apply96(*dialog);
    });
    restartDriver.start(10);
    window.findChild<QAction *>("actionRestartPreprocessing")->trigger();
    require(restarted && session.snapshot()->pattern.observed == *imported->pattern.original_observed &&
                !session.snapshot()->peaks.valid, "Explicit Restart did not restore stored raw intensity");
    {
        BackgroundDialog dialog(&window);
        dialog.setBackground();
        dialog.findChild<QComboBox *>("typeComboBox")->setCurrentIndex(6); // explicit product 'None'
        apply96(dialog);
    }
    require(session.snapshot()->preprocessing.background->method == preprocessing::BackgroundMethod::none &&
                std::all_of(session.snapshot()->pattern.background->values.begin(),
                    session.snapshot()->pattern.background->values.end(), [](double v) { return v == 0; }),
            "Explicit None background did not preserve its frozen definition");
    {
        SmoothingDialog dialog(&window);
        dialog.setSmoothing();
        dialog.findChild<QComboBox *>("methodComboBox")->setCurrentIndex(0);
        dialog.findChild<QSpinBox *>("pointsSpinBox")->setValue(4);
        dialog.findChild<QSpinBox *>("polynomialSpinBox")->setValue(5);
        apply96(dialog);
    }
    ProductWorkflowProbe::automaticPeaks(window);
    const auto finite = session.snapshot();
    require(finite->peaks.valid && !finite->peaks.tth.empty() && finite->preprocessing.peak_candidates &&
                !finite->preprocessing.candidate_failure && bool(validateSearchState(*finite)),
            "Full C++ candidate pool and selected snapshot must be finite");
    noPreview96(window);
    const auto direct = search96(window);
    ProductWorkflowProbe::save(window, output + "-finite.qxp");
    const auto saved = read96(output + "-finite.qxp");
    // Load only saved selected peaks: missing pool remains unknown, never reconstructed.
    auto selectedOnly = saved;
    auto config = selectedOnly["preprocessing"].toObject();
    config.remove("candidate_pool");
    selectedOnly["preprocessing"] = config;
    write96(output + "-selected.qxp", selectedOnly);
    ProductWorkflowProbe::load(window, output + "-selected.qxp");
    require(!session.snapshot()->preprocessing.peak_candidates && session.snapshot()->peaks.valid,
            "Loading a finite selected snapshot reconstructed the missing pool");
    require(gui::encode_project_peaks(session.snapshot()->peaks) == saved["peaks"].toObject(),
            "Restored finite snapshot changed");
    const auto restored = search96(window);
    require(restored == direct, "Saved finite snapshot Search-Match differs");
    // Independent explicit manual input, not values inferred from any undefined candidate.
    window.findChild<QAction *>("actionDeleteAllPeaks")->trigger();
    const double positions[] = {20.86, 26.64, 36.54};
    const double intensities[] = {20., 100., 10.};
    for (int i = 0; i < 3; ++i) {
        QTimer driver;
        bool entered = false;
        QObject::connect(&driver, &QTimer::timeout, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            if (!dialog || dialog->objectName() != "manualPeakDialog") return;
            driver.stop();
            entered = true;
            dialog->findChild<QLineEdit *>("manualPosition")->setText(QString::number(positions[i]));
            dialog->findChild<QLineEdit *>("manualIntensity")->setText(QString::number(intensities[i]));
            dialog->findChild<QLineEdit *>("manualFwhm")->setText("0.12");
            apply96(*dialog);
        });
        driver.start(10);
        require(gui::editManualPeak(window) && entered, "Explicit manual input failed");
    }
    require(session.snapshot()->peaks.tth == QVector<double>({20.86, 26.64, 36.54}) &&
                session.snapshot()->peaks.intensity == QVector<double>({20., 100., 10.}) &&
                !session.snapshot()->preprocessing.peak_candidates, "Manual input changed or invented a pool");
    const auto manual = search96(window);
    ProductWorkflowProbe::save(window, output + "-manual.qxp");
    write96(output + "-e2e.json", {{"status", "passed"}, {"preprocessing_backend", "cpp"},
        {"search_backend", "cpp"}, {"qa_flag_set", false},
        {"finite_workflow", "Restart stored raw -> explicit None background -> SG(4,5) -> automatic peaks"},
        {"finite_peak_count", finite->peaks.tth.size()},
        {"finite_candidate_count", int(finite->preprocessing.peak_candidates->peaks.size())},
        {"direct_search", direct}, {"restored_selected_search", restored}, {"manual_search", manual},
        {"reopen_process_test", "separate CTest fixture"}, {"human_signoff", "pending"}});
    close96(window);
    std::cout << "PASS: finite C++ preprocessing, restored snapshot and explicit manual peaks -> GUI Search-Match\n";
}

void phase96Reopen(MainWindow &window, const QString &source, const QString &output) {
    backend96();
    auto &session = AppState::searchSession();
    require(session.snapshot()->pattern.observed.empty(), "Reopen test must start in a fresh process");
    const auto saved = read96(source);
    ProductWorkflowProbe::load(window, source);
    require(qualpf::gui::encode_project_peaks(session.snapshot()->peaks) == saved["peaks"].toObject() &&
                qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) ==
                    saved["preprocessing"].toObject(), "Fresh-process project restoration changed peaks/config");
    const auto restored = scores(session.snapshot()->candidates);
    require(!restored.empty(), "Saved project contains no completed search results");
    const auto searched = search96(window);
    require(searched == restored, "Search after process restart differs from saved results");
    ProductWorkflowProbe::save(window, output + "-resaved.qxp");
    const auto resaved = read96(output + "-resaved.qxp");
    for (const char *field : {"peaks", "preprocessing", "search_options", "restraints", "automatic_delta", "peak_tolerance"})
        require(resaved[field] == saved[field], "Reopened project persistence differs");
    write96(output + ".json", {{"status", "passed_fresh_process_reopen_and_search"},
        {"source", source}, {"search", searched}, {"preprocessing_backend", "cpp"}, {"qa_flag_set", false}});
    close96(window);
    std::cout << "PASS: fresh process restores saved project and continues GUI Search-Match\n";
}
