#include "gui/presentation/product_plot_refresh.h"
#include "gui/export/pattern_export_service.h"
// Real product actions and SaveDialog; no replacement format implementation.
void phase95FormatChecks(MainWindow &window, const QString &output, bool preserveBackend = false) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    auto &session = AppState::searchSession();
    if (!preserveBackend) qputenv("QUALPF_PREPROCESSING_BACKEND", "cpp");
    auto messages = [&] {
        QString text;
        for (auto *label : window.statusBar()->findChildren<QLabel *>()) text += label->text();
        return text;
    };
    auto save = [&](const char *slot, const QString &filter, const QString &path) {
        window.clearStatusMessage();
        bool visited = false;
        QTimer driver;
        QObject::connect(&driver, &QTimer::timeout, [&] {
            if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                visited = true;
                driver.stop();
                dialog->setOption(QFileDialog::DontConfirmOverwrite);
                dialog->selectNameFilter(filter);
                dialog->selectFile(path);
                QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
            }
        });
        driver.start(10);
        require(QMetaObject::invokeMethod(&window, slot, Qt::DirectConnection), "Export action missing");
        require(visited, "Real SaveDialog was not exercised");
    };
    auto bytes = [](const QString &path) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "Export missing");
        return file.readAll();
    };
    auto source = session.snapshot();
    auto checkCif = [&](const QString &path, bool hasBackground) {
        std::size_t row = 0;
        bool inPattern = false;
        for (const auto &line : bytes(path).split('\n')) {
            const auto fields = QString::fromLatin1(line).simplified().split(' ');
            if (fields.front() == "_pd_meas_2theta_scan") inPattern = true;
            if (!inPattern || fields.front().isEmpty() || fields.front().startsWith('_')) continue;
            require(fields.size() == (hasBackground ? 5 : 3) && row < source->pattern.observed.size(),
                    "CIF pattern row shape differs");
            QVector<double> values;
            for (const auto &field : fields) {
                bool valid = false;
                const double value = field.toDouble(&valid);
                require(valid && std::isfinite(value), "CIF contains invalid numeric output");
                values.append(value);
            }
            // Retained writer emits binary32 transport with six decimal places.
            close(values[0], float(source->pattern.two_theta[row].value), 5.1e-7, 0, "CIF theta");
            close(values[2], float(source->pattern.observed[row]), 5.1e-7, 0, "CIF observed");
            if (hasBackground) {
                const float bg = float(source->pattern.background->values[row]);
                close(values[4], bg, 5.1e-7, 0, "CIF background");
                close(values[3], float(source->pattern.observed[row]) - bg, 5.1e-7, 0, "CIF net");
            }
            ++row;
        }
        require(row == source->pattern.observed.size(), "CIF point count differs");
    };
    QJsonArray checks;
    const QStringList backends = preserveBackend ? QStringList{"default"} : QStringList{"cpp", "legacy"};
    for (const auto &backend : backends) {
        if (!preserveBackend) qputenv("QUALPF_PREPROCESSING_BACKEND", backend.toLatin1());
        for (const auto name : {"DICVOL", "McMaille"}) {
            const auto path = output + "-" + backend + "-" + name + ".dat";
            save("onActionSavePeaksTriggered", QString("Input File for %1 (*.dat)").arg(name), path);
            require(!QFileInfo::exists(path) && messages().contains(QString(name) + " export is unavailable"),
                    "Unavailable format silently succeeded");
            require(session.snapshot() == source, "Rejected format changed Session");
        }
    }
    checks.append(preserveBackend ? "DICVOL/McMaille: explicit rejection under default backend" :
                                   "DICVOL/McMaille: explicit rejection under cpp and legacy");
    if (!preserveBackend) qputenv("QUALPF_PREPROCESSING_BACKEND", "cpp");
    save("onActionExportDiffractionPattern", "XY files (*.xy)", output + "-pattern.xy");
    require(!bytes(output + "-pattern.xy").isEmpty(), "Empty pattern XY");
    save("onActionExportDiffractionPattern", "CIF files (*.cif)", output + "-pattern.cif");
    require(bytes(output + "-pattern.cif").contains("_pd_meas_counts_total"), "CIF bridge wrote no pattern");
    checkCif(output + "-pattern.cif", false);
    require(session.snapshot() == source, "CIF mirror changed authoritative state");
    checks.append("pattern XY and CIF real entry exports");

    PreprocessingRequest request;
    request.wavelength = source->pattern.radiation.lines.front().wavelength;
    request.background = preprocessing::BackgroundOptions{};
    PreprocessingEdit background(session);
    require(bool(background.preview(request)) && bool(background.commit()), "Background setup failed");
    source = session.snapshot();
    save("onActionBackgroundExportTriggered", "XY files (*.xy)", output + "-background.xy");
    require(!bytes(output + "-background.xy").isEmpty(), "Empty background XY");
    save("onActionExportDiffractionPattern", "CIF files (*.cif)", output + "-background.cif");
    require(bytes(output + "-background.cif").contains("_pd_proc_intensity_bkg_calc"), "CIF lost background");
    checkCif(output + "-background.cif", true);
    save("onActionBackgroundExportTriggered", "CIF files (*.cif)", output + "-unavailable-background.cif");
    require(!QFileInfo::exists(output + "-unavailable-background.cif") && messages().contains("unavailable"),
            "Background-only CIF was not explicitly rejected");
    // Explicit TEST/ORACLE_ONLY display observation formerly hidden in the native helper.
    // The export service itself has no presentation or Session side effect.
    require(bool(qualpf::gui::presentation::request_product_plot_refresh(AppState::context())),
            "Missing-destination control presentation observation");
    require(!bool(qualpf::gui::exporting::PatternExportService(session.snapshot(), AppState::context().files().exports).save(output + "-missing/destination.cif", qualpf::exporting::PatternFormat::powder_cif)), "CIF write failure was ignored");
    require(session.snapshot() == source, "Export mutated background Session");
    checks.append("full background CIF/XY, background-only CIF rejection, failed destination");

    auto partial = *source;
    const auto &values = source->pattern.background->values;
    partial.pattern.background->values = std::vector<double>(values.begin() + 100, values.begin() + 300);
    partial.pattern.background->window = SampleWindow{100};
    require(bool(session.commit(source, partial)), "Partial setup failed");
    source = session.snapshot();
    save("onActionBackgroundExportTriggered", "XY files (*.xy)", output + "-partial.xy");
    const auto lines = bytes(output + "-partial.xy").trimmed().split('\n');
    require(lines.size() == 201, "Partial XY point count changed");
    const auto columns = QString::fromLatin1(lines[1]).simplified().split(' ');
    close(columns[0].toDouble(), partial.pattern.two_theta[100].value, 1e-5, 0, "Partial XY offset");
    save("onActionExportDiffractionPattern", "CIF files (*.cif)", output + "-partial.cif");
    require(!QFileInfo::exists(output + "-partial.cif") && messages().contains("complete aligned background"),
            "Partial CIF was not rejected");
    require(session.snapshot() == source, "Partial export changed Session");
    checks.append("partial XY alignment and partial CIF rejection");

    PreprocessingRequest manualRequest;
    manualRequest.wavelength = source->pattern.radiation.lines.front().wavelength;
    manualRequest.tolerance = AngularWidthDegrees{.15};
    manualRequest.manual = ManualPeakChange{};
    manualRequest.manual->two_theta = source->pattern.two_theta[200].value;
    manualRequest.manual->intensity = 37.25;
    manualRequest.manual->fwhm = .12;
    PreprocessingEdit manual(session);
    require(bool(manual.preview(manualRequest)) && bool(manual.commit()), "Explicit manual test input failed");
    source = session.snapshot();
    auto *plot = window.xpdViewer();
    for (auto axis : {xpdutils::TTHETA, xpdutils::DVALUE, xpdutils::ONE_OVER_DVALUE, xpdutils::ONE_OVER_DVALUE2}) {
        auto settings = plot->pSettings();
        settings.setAbscissa(axis);
        plot->setPlotSettings(settings);
        window.refreshPatternPresentation();
        window.refreshPreprocessingDisplay();
        CardPeakData card;
        card.id = "phase95-measured-overlay";
        card.wave = source->peaks.wave;
        card.color = Qt::red;
        card.tth = source->peaks.tth;
        card.d = source->peaks.d;
        card.intensity = source->peaks.intensity;
        plot->setCardPeaks({card});
        double expected = axis == xpdutils::TTHETA ? card.tth[0] : card.d[0];
        if (axis == xpdutils::ONE_OVER_DVALUE) expected = 1 / expected;
        if (axis == xpdutils::ONE_OVER_DVALUE2) expected = 1 / (expected * expected);
        close(plot->graph(plot->graphCount()-1)->data()->at(0)->key, expected, 1e-12, 1e-12, "Card overlay coordinate");
        plot->setCardPeaks({});
        plot->drawSelectedComparePoints(card.tth, card.intensity, {Qt::red});
        if (axis != xpdutils::TTHETA) expected = xpdutils::dvalue(card.tth[0], card.wave);
        if (axis == xpdutils::ONE_OVER_DVALUE) expected = 1 / expected;
        if (axis == xpdutils::ONE_OVER_DVALUE2) expected = 1 / (expected * expected);
        close(plot->graph(plot->graphCount()-1)->data()->at(0)->key, expected, 1e-12, 1e-12, "Compare overlay coordinate");
        plot->drawSelectedComparePoints({}, {}, {});
        ProductWorkflowProbe::deletePeaks(window, {0});
        require(session.snapshot()->peaks.tth.empty() && plot->peaks.graphIndex < 0, "Axis delete left stale peaks");
        PreprocessingEdit restoreManual(session);
        require(bool(restoreManual.preview(manualRequest)) && bool(restoreManual.commit()), "Manual fixture restore failed");
        source = session.snapshot();
    }
    checks.append("four coordinate modes: card/compare overlays and real peak deletion");
    const QStringList formats{"2 theta values (*.dat *.txt *.pea)", "d values (*.dat *.txt *.pea)", "Peak list (*.txt)"};
    const auto peaks = searchPeaks(source->peaks);
    require(bool(peaks), "Peak snapshot missing");
    for (int i = 0; i < formats.size(); ++i) {
        const auto path = output + QString("-peaks-%1.txt").arg(i);
        save("onActionSavePeaksTriggered", formats[i], path);
        const auto expected = preprocessing::export_peak_text(peaks.value(),
            i == 0 ? preprocessing::PeakTextFormat::two_theta :
            i == 1 ? preprocessing::PeakTextFormat::d_spacing : preprocessing::PeakTextFormat::details);
        if (expected && bytes(path).replace("\r\n", "\n") != QByteArray::fromStdString(expected.value())) {
            std::cerr << "Export mismatch format " << i << "\nactual:\n" << bytes(path).constData()
                      << "expected:\n" << expected.value()
                      << "snapshot unchanged: " << (session.snapshot() == source) << '\n';
        }
        require(bool(expected) && bytes(path).replace("\r\n", "\n") == QByteArray::fromStdString(expected.value()),
                "Real peak export differs from frozen formatter");
    }
    require(session.snapshot() == source, "Peak export changed Session");
    checks.append("three peak text formats from explicit finite manual input");
    QFile report(output + "-formats.json");
    require(report.open(QIODevice::WriteOnly), "Cannot write format report");
    report.write(QJsonDocument(QJsonObject{{"checks", checks}, {"status", "passed"},
        {"human_visual_acceptance", false}, {"default_without_environment", preserveBackend}}).toJson());
    std::cout << "PASS: Phase 9.5 actual format export actions\n";
}
