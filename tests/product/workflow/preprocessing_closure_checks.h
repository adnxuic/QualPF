// Phase 9.4 product transactions, real widget actions and project round trips.
void phase94ProductChecks(MainWindow &window, const QString &output, qualpf::gui::SessionViewHandle raw,
                          bool preserveBackend = false) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    auto &session = AppState::searchSession();
    const auto reference = session.snapshot();
    if (!preserveBackend) qputenv("QUALPF_PREPROCESSING_BACKEND", "cpp");
    const auto restore = [&](SessionViewHandle state) {
        require(bool(session.commit(session.snapshot(), *state)), "Restore source failed");
        window.refreshPatternPresentation();
        window.refreshPreprocessingDisplay();
    };
    auto json = [](const SessionStateView &s) { return gui::encode_project_peaks(s.peaks); };
    QJsonArray checks;
    auto passed = [&](const char *name) { checks.append(name); };
    for (auto action : {PreprocessingAction::automatic_peaks, PreprocessingAction::prepare_search}) {
        qualpf::application::Workspace isolatedOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView isolated(isolatedOwner.session());
        require(bool(isolated.commit(isolated.snapshot(), *raw)), "Raw fixture failed");
        const auto before = isolated.snapshot();
        auto status = applyPreprocessing(isolated, action);
        require(bool(status) && isolated.snapshot() != before && isolated.snapshot()->peaks.valid &&
                    !isolated.snapshot()->preprocessing.rejected_candidates.empty(),
                "Default product chain lost measurable peaks or diagnostics");
    }
    passed("default_chains_measurable_candidates");
    restore(reference);
    PreprocessingRequest request;
    request.wavelength = WavelengthAngstrom{reference->peaks.wave};
    request.tolerance = AngularWidthDegrees{reference->peakTolerance.value_or(.15)};
    request.manual = ManualPeakChange{};
    request.manual->two_theta = reference->peaks.tth.front() + .002;
    request.manual->intensity = 37.25;
    request.manual->fwhm = .12;
    if (reference->peaks.integratedIntensity)
        request.manual->integrated_intensity = 1.25;
    const auto before = session.snapshot();
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(request)) && session.snapshot() == before, "Manual preview published");
        auto invalid = request;
        invalid.manual->fwhm = std::numeric_limits<double>::quiet_NaN();
        require(!edit.preview(invalid) && !edit.commit() && session.snapshot() == before,
                "Invalid manual values retained an applyable draft");
        require(bool(edit.preview(request)), "Manual preview could not recover");
        edit.cancel();
        require(session.snapshot() == before && !edit.commit(), "Manual cancel published");
    }
    {
        PreprocessingEdit stale(session), valid(session);
        require(bool(stale.preview(request)) && bool(valid.preview(request)), "Manual previews failed");
        require(bool(valid.commit()) && !valid.commit() && !stale.commit(),
                "Repeated/stale manual commit accepted");
    }
    const auto added = session.snapshot();
    require(added->peaks.tth.size() == reference->peaks.tth.size() + 1 &&
                !added->preprocessing.peak_candidates && !added->preprocessing.candidate_failure &&
                added->candidates.empty() && added->phases.empty(),
            "Manual ownership/invalidation failed");
    auto removed = added->peaks;
    const int at = removed.tth.indexOf(request.manual->two_theta);
    require(at >= 0, "Manual position changed");
    for (auto *v : {&removed.tth, &removed.d, &removed.deltaD, &removed.intensity, &removed.intensityOrig,
                    &removed.fwhm})
        v->removeAt(at);
    if (removed.integratedIntensity)
        removed.integratedIntensity->removeAt(at);
    require(gui::encode_project_peaks(removed) == json(*reference),
            "Manual add modified untouched rows");
    PreprocessingRequest reselect;
    reselect.peak_operation = PeakOperation::reselect;
    reselect.selection = PeakSelection{};
    PreprocessingEdit selection(session);
    auto rejected = selection.preview(reselect);
    require(!rejected && rejected.error().reason == "candidate_pool_unavailable",
            "Manual selected peaks reconstructed a pool");
    selection.cancel();
    passed("manual_preview_cancel_invalid_stale_repeated_and_pool_invalidation");
    ProductWorkflowProbe::save(window, output + "-manual.qxp");
    ProductWorkflowProbe::load(window, output + "-manual.qxp");
    require(json(*session.snapshot()) == json(*added) &&
                session.snapshot()->projectMetadata["peak_snapshot_origin"] == "manual",
            "Manual project round trip failed");
    auto *table = window.findChild<PeakListWidget *>();
    require(table && table->peakListModel->rowCount() == added->peaks.tth.size(),
            "Peak table not Session-owned");
    table->peakListModel->setData(table->peakListModel->index(at, 2), 42.5);
    require(session.snapshot()->peaks.intensity[at] == 42.5 &&
                session.snapshot()->peaks.intensityOrig[at] == 42.5,
            "Table modification did not commit complete peak values");
    const auto modified = session.snapshot();
    auto *plot = window.xpdViewer();
    require(plot->peaks.graphIndex >= 0 &&
                plot->graph(plot->peaks.graphIndex)->dataCount() == modified->peaks.tth.size(),
            "Peak display is stale");
    for (auto abscissa :
         {xpdutils::DVALUE, xpdutils::ONE_OVER_DVALUE, xpdutils::ONE_OVER_DVALUE2, xpdutils::TTHETA}) {
        auto settings = plot->pSettings();
        settings.setAbscissa(abscissa);
        plot->setPlotSettings(settings);
        // A real edit mirrors the committed Session through the native redraw.
        // Merely replacing graph data misses stale 2theta viewport bounds.
        table->peakListModel->setData(table->peakListModel->index(at, 2),
                                     session.snapshot()->peaks.intensity[at] + 1.0);
        const auto row = abscissa == xpdutils::DVALUE ? modified->peaks.tth.size() - 1 : 0;
        double x = modified->peaks.tth[row];
        if (abscissa != xpdutils::TTHETA)
            x = xpdutils::dvalue(x, modified->peaks.wave);
        if (abscissa == xpdutils::ONE_OVER_DVALUE)
            x = 1 / x;
        if (abscissa == xpdutils::ONE_OVER_DVALUE2)
            x = 1 / (x * x);
        close(plot->graph(plot->peaks.graphIndex)->data()->at(0)->key, x, 1e-12, 1e-12,
              "Peak display coordinate");
        require(plot->xAxis->range().contains(x), "Committed peak is outside the active coordinate viewport");
        require(plot->xAxis->rangeReversed() == (abscissa == xpdutils::DVALUE),
                "Committed coordinate direction changed");
        plot->drawSelectedPeaks({0});
    }
    require(window.grab().save(output + "-manual.png"), "Cannot save rendered manual view");
    ProductWorkflowProbe::deletePeaks(window, {at});
    require(json(*session.snapshot()) == json(*reference), "Manual delete did not preserve other rows");
    auto *clear = window.findChild<QAction *>("actionDeleteAllPeaks");
    require(clear != nullptr, "Delete all action missing");
    clear->trigger();
    require(session.snapshot()->peaks.tth.empty() && table->peakListModel->rowCount() == 0,
            "Delete all did not clear Session/table");
    passed("manual_table_modify_delete_delete_all_project_and_plot");
    restore(reference);
    QTimer driver;
    bool visited = false;
    QObject::connect(&driver, &QTimer::timeout, [&] {
        auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
        if (!dialog || dialog->objectName() != "manualPeakDialog")
            return;
        visited = true;
        dialog->findChild<QLineEdit *>("manualPosition")
            ->setText(QString::number(request.manual->two_theta, 'g', 17));
        dialog->findChild<QLineEdit *>("manualIntensity")->setText("37.25");
        dialog->findChild<QLineEdit *>("manualFwhm")->setText("0.12");
        if (reference->peaks.integratedIntensity)
            dialog->findChild<QLineEdit *>("manualArea")->setText("1.25");
        auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        require(ok->isEnabled(), "Measured peak dialog Apply disabled");
        driver.stop();
        ok->click();
    });
    driver.start(10);
    require(gui::editManualPeak(window) && visited, "Measured peak dialog did not commit");
    require(json(*session.snapshot()) == json(*added), "GUI/manual service disagree");
    passed("actual_manual_dialog_apply");
    auto partial = *raw;
    const std::size_t first = 100, count = 500;
    require(partial.pattern.observed.size() > first + count, "Fixture too short");
    preprocessing::BackgroundOptions partialOptions;
    partialOptions.method = preprocessing::BackgroundMethod::bruckner;
    auto fullCurve = preprocessing::background(raw->pattern, partialOptions, request.wavelength);
    require(bool(fullCurve), "Partial curve source failed");
    partial.preprocessing.background = partialOptions;
    partial.pattern.background =
        SampledSignal{std::vector<double>(fullCurve.value().values.begin() + first,
                                          fullCurve.value().values.begin() + first + count),
                      SampleWindow{first}};
    partial.pattern.calculation_range =
        AngularRange{partial.pattern.two_theta[first], partial.pattern.two_theta[first + count - 1]};
    partial.peaks.clear();
    partial.preprocessing.peak_candidates.reset();
    partial.preprocessing.candidate_failure.reset();
    require(bool(session.commit(session.snapshot(), partial)), "Partial background setup rejected");
    const auto partialSource = session.snapshot();
    window.refreshPatternPresentation();
    window.refreshPreprocessingDisplay();
    require(plot->back.graphIndex >= 0 && plot->graph(plot->back.graphIndex)->dataCount() == count &&
                plot->graph(plot->back.graphIndex)->data()->at(0)->key ==
                    partial.pattern.two_theta[first].value,
            "Partial background plotted at the wrong offset");
    require(window.grab().save(output + "-partial.png"), "Cannot save rendered partial background");
    ProductWorkflowProbe::save(window, output + "-partial.qxp");
    ProductWorkflowProbe::load(window, output + "-partial.qxp");
    require(session.snapshot()->pattern.background->window->first == first &&
                session.snapshot()->pattern.background->values == partial.pattern.background->values &&
                session.snapshot()->pattern.calculation_range->minimum.value ==
                    partial.pattern.calculation_range->minimum.value,
            "Partial background/window project round trip failed");
    {
        const auto original = session.snapshot();
        BackgroundDialog dialog(&window);
        dialog.show();
        dialog.setBackground();
        const auto opened = session.snapshot();
        require(dialog.findChild<QLabel *>("partialBackgroundNotice") != nullptr,
                "Partial background notice missing");
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        require(ok->isEnabled(), "Restored partial background cannot be applied");
        ok->setFocus();
        ok->click();
        require(session.snapshot()->pattern.background->values == original->pattern.background->values &&
                    session.snapshot()->pattern.background->window->first == first,
                "Background dialog replaced partial payload on open");
    }
    {
        const auto original = session.snapshot();
        BackgroundDialog dialog(&window);
        dialog.show();
        dialog.setBackground();
        const auto opened = session.snapshot();
        auto *filter = dialog.findChild<QPushButton *>("applyFilterButton");
        require(filter && filter->isVisible(), "Partial background filter not accessible");
        filter->click();
        require(session.snapshot() == opened, "Partial filter preview published");
        auto *ok = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        require(ok->isEnabled(), "Partial background filter failed");
        ok->setFocus();
        ok->click();
        require(session.snapshot()->pattern.background->window->first == first &&
                    session.snapshot()->pattern.background->values.size() == count,
                "Partial filter widened background window");
        require(bool(session.commit(session.snapshot(), *original)),
                "Cannot restore unfiltered partial fixture");
    }
    const auto beforePartial = session.snapshot();
    require(bool(gui::exporting::PatternExportService(session.snapshot(), AppState::context().files().exports).save(
        output + "-partial.xy", qualpf::exporting::PatternFormat::xy_background)),
        "Partial background export failed");
    QFile exported(output + "-partial.xy");
    require(exported.open(QIODevice::ReadOnly), "Missing partial export");
    const auto lines = exported.readAll().trimmed().split('\n');
    require(lines.size() == int(count) + 1, "Partial export row count differs");
    const auto columns = QString::fromLatin1(lines[1]).simplified().split(' ');
    close(columns[0].toDouble(), partial.pattern.two_theta[first].value, 0.00000051, 0,
          "Partial export coordinate");
    close(columns[1].toDouble(), partial.pattern.background->values.front(), 1e-7, 1e-7,
          "Partial export intensity");
    window.setBackgroundSubtractionDisplay(true);
    require(!window.context().presentation().snapshot().subtract_background && session.snapshot() == beforePartial,
            "Partial subtraction invented outside values");
    PreprocessingRequest crop;
    crop.crop_range = AngularRange{partial.pattern.two_theta[200], partial.pattern.two_theta[400]};
    crop.tolerance = AngularWidthDegrees{.15};
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(crop)) && session.snapshot() == beforePartial, "Crop preview published");
        require(edit.preview()->pattern.observed.size() == 201 &&
                    edit.preview()->pattern.background->window->first == 0 &&
                    edit.preview()->pattern.background->values.size() == 201 &&
                    edit.preview()->pattern.original_observed->front() ==
                        (*raw->pattern.original_observed)[200],
                "Crop alignment failed");
        require(bool(edit.commit()), "Crop commit failed");
    }
    ProductWorkflowProbe::save(window, output + "-crop.qxp");
    ProductWorkflowProbe::load(window, output + "-crop.qxp");
    require(session.snapshot()->pattern.observed.size() == 201, "Cropped project failed");
    passed("partial_background_plot_codec_export_atomic_subtraction_rejection_and_crop");
    restore(raw);
    {
        RangeDialog dialog(&window);
        dialog.show();
        dialog.setRange(plot);
        const auto opened = session.snapshot();
        dialog.findChild<QDoubleSpinBox *>("minSpinBox")->setValue(partial.pattern.two_theta[200].value);
        dialog.findChild<QDoubleSpinBox *>("maxSpinBox")->setValue(partial.pattern.two_theta[400].value);
        dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
        require(session.snapshot() == opened, "Range Apply preview published");
        require(QMetaObject::invokeMethod(&dialog, "reject", Qt::DirectConnection), "Range cancel failed");
        require(session.snapshot()->pattern.observed == raw->pattern.observed,
                "Range cancel changed observed data");
    }
    PreprocessingRequest restart;
    restart.restart_from_original = true;
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(restart)), "Stored original recovery failed");
        require(edit.preview()->pattern.observed == *raw->pattern.original_observed &&
                    edit.preview()->pattern.processing.intensity_scaled == false &&
                    !edit.preview()->pattern.background && !edit.preview()->preprocessing.peak_candidates &&
                    edit.preview()->peaks.tth.empty(),
                "Recovery invented history");
        require(bool(edit.commit()), "Stored original recovery commit failed");
    }
    auto historical = *session.snapshot();
    historical.pattern.original_observed.reset();
    historical.pattern.processing.intensity_scaled.reset();
    historical.pattern.processing.background_subtracted.reset();
    historical.preprocessing.smoothing_applied.reset();
    historical.pattern.calculation_range.reset();
    historical.projectMetadata.remove("has_back");
    require(bool(session.commit(session.snapshot(), historical)), "Historical setup failed");
    ProductWorkflowProbe::save(window, output + "-unknown.qxp");
    ProductWorkflowProbe::load(window, output + "-unknown.qxp");
    const auto unknown = session.snapshot();
    require(!unknown->pattern.original_observed && !unknown->pattern.processing.intensity_scaled &&
                !unknown->pattern.processing.background_subtracted && !unknown->pattern.calculation_range &&
                !unknown->projectMetadata.contains("has_back"),
            "Project invented missing historical state");
    PreprocessingEdit recovery(session);
    auto missing = recovery.preview(restart);
    require(!missing && missing.error().reason == "raw_intensity_unavailable" &&
                session.snapshot() == unknown,
            "Recovery invented historical raw intensity");
    passed("range_dialog_cancel_raw_recovery_and_unknown_history_round_trip");
    restore(added);
    auto builder = buildSearchMatchQuery(AppState::context(), AppState::peaks());
    QVector<CardType> cards;
    AppState::searches().makeQueryStrongest(builder, cards);
    require(AppState::searches().searchError().isEmpty(), "Search-Match after manual recovery failed");
    compareCpp(session.snapshot()->pattern, session.snapshot()->peaks, cards);
    const auto searchResults = scores(cards);
    passed("search_match_after_explicit_manual_edit");
    restore(reference);
    QJsonObject report{{"sample", QFileInfo(QString::fromStdString(raw->pattern.source_id)).baseName()},
                       {"checks", checks},
                       {"manual_search_results", searchResults},
                       {"default_without_environment", preserveBackend},
                       {"human_visual_acceptance", false}};
    QFile reportFile(output + "-closure.json");
    require(reportFile.open(QIODevice::WriteOnly), "Cannot write closure report");
    reportFile.write(QJsonDocument(report).toJson(QJsonDocument::Indented));
    if (!preserveBackend) qunsetenv("QUALPF_PREPROCESSING_BACKEND");
    std::cout << "PASS: Phase 9.4 product transactions, dialogs and project windows\n";
}
