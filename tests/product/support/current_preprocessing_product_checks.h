// Current successor of the frozen Phase9.3 Native oracle; existing product contracts only.
void phase93ProductChecks(MainWindow &window, const QString &output, qualpf::gui::SessionViewHandle raw) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    auto &session = AppState::searchSession();
    require(raw->pattern.original_observed && raw->pattern.processing.intensity_scaled == true &&
                raw->preprocessing.smoothing_applied == false, "Imported current history missing");
    // Defined represented failure plus an independent finite manual measurement.
    // The historical nonfinite Native pool is frozen outside current execution.
    auto represented = *raw;
    represented.peaks.valid = true;
    represented.peaks.wave = raw->pattern.radiation.lines.front().wavelength.value;
    represented.peaks.tth = {60}; represented.peaks.d = {represented.peaks.wave};
    represented.peaks.deltaD = {.01}; represented.peaks.intensity = {100};
    represented.peaks.intensityOrig = {100}; represented.peaks.fwhm = {.12};
    represented.peaks.integratedIntensity.reset();
    represented.preprocessing.peak_candidates.reset();
    represented.preprocessing.candidate_failure = "nonfinite_candidate_measurement";
    require(bool(session.commit(session.snapshot(), represented)), "Represented missing-pool fixture rejected");
    const auto savedInvalidPool = session.snapshot();
    auto cancel = [](QDialog &d) {
        require(QMetaObject::invokeMethod(&d, "reject", Qt::DirectConnection), "Cancel unavailable");
    };
    auto apply = [](QDialog &d) {
        d.show();
        QApplication::processEvents();
        auto *b = d.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        require(b->isEnabled(), "Apply disabled");
        b->setFocus();
        b->click();
    };
    qputenv("QUALPF_PREPROCESSING_BACKEND", "cpp");
    require(cppPreprocessingProduct(), "Explicit C++ backend unavailable");
    {
        PeakSearchDialog dialog(&window);
        dialog.setOptions();
        const auto before = session.snapshot();
        require(dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled(),
                "Saved finite snapshot could not be restored");
        dialog.findChild<QDoubleSpinBox *>("intensitySpinBox")->setValue(20);
        require(dialog.property("preprocessingReason") == "nonfinite_candidate_measurement" &&
                    !dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled() &&
                    session.snapshot() == before,
                "Undefined pool reselection silently succeeded");
        cancel(dialog);
        require(gui::encode_project_peaks(session.snapshot()->peaks) ==
                    gui::encode_project_peaks(savedInvalidPool->peaks),
                "Cancel changed saved finite selected peaks");
    }
    QJsonArray actions;
    for (auto action : {PreprocessingAction::automatic_peaks, PreprocessingAction::prepare_search}) {
        qualpf::application::Workspace isolatedOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView isolated(isolatedOwner.session());
        require(bool(isolated.commit(isolated.snapshot(), *raw)), "Raw setup failed");
        const auto before = isolated.snapshot();
        auto result = applyPreprocessing(isolated, action);
        require(!result && result.error().code == ErrorCode::undefined_preprocessing &&
                    result.error().reason == "coincident_baseline_endpoints" && isolated.snapshot() == before,
                "Default product chain undefined contract changed");
        actions.append(QJsonObject{{"action", int(action)},
                                   {"reason", QString::fromStdString(result.error().reason)},
                                   {"message", QString::fromStdString(result.error().message)},
                                   {"zero_publication", true}});
    }
    require(bool(session.commit(session.snapshot(), *raw)), "Cannot restore raw owner");
    {
        const auto before = session.snapshot();
        ProductWorkflowProbe::automaticPeaks(window);
        require(session.snapshot() == before, "Automatic menu published rejected chain");
        ProductWorkflowProbe::subtract(window);
        require(session.snapshot() != before &&
                    session.snapshot()->pattern.processing.background_subtracted == true &&
                    session.snapshot()->pattern.original_observed == raw->pattern.original_observed,
                "C++ subtraction menu/history failed");
        const auto &observed = session.snapshot()->pattern.observed;
        require(observed.size() == raw->pattern.observed.size() &&
                    session.snapshot()->pattern.background &&
                    std::all_of(session.snapshot()->pattern.background->values.begin(),
                                session.snapshot()->pattern.background->values.end(), [](double v) { return v == 0; }) &&
                    std::all_of(observed.begin(), observed.end(), [](double v) { return std::isfinite(v) && v >= 0; }) &&
                    bool(validateSearchState(*session.snapshot())),
                "Current subtraction alignment/finite-state contract failed");
        // Exact numerical subtraction authority remains the frozen, independently
        // qualified preprocessing reference; this caller checks publication/history.
    }
    require(bool(session.commit(session.snapshot(), *raw)), "Advanced background source failed");
    {
        BackgroundDialog dialog(&window);
        dialog.setBackground();
        const auto before = session.snapshot();
        dialog.findChild<QComboBox *>("typeComboBox")->setCurrentIndex(5);
        auto *lo = dialog.findChild<QDoubleSpinBox *>("minSpinBox"),
             *hi = dialog.findChild<QDoubleSpinBox *>("maxSpinBox");
        lo->setValue(raw->pattern.two_theta[raw->pattern.two_theta.size() / 4].value);
        hi->setValue(raw->pattern.two_theta[3 * raw->pattern.two_theta.size() / 4].value);
        require(QMetaObject::invokeMethod(&dialog, "onApplyFilterButtonClicked", Qt::DirectConnection),
                "Filter action missing");
        require(QMetaObject::invokeMethod(&dialog, "onApplyFilterButtonClicked", Qt::DirectConnection),
                "Repeated filter action missing");
        require(session.snapshot() == before && dialog.property("preprocessingReason").toString().isEmpty(),
                "Existing interval filter failed/published");
        apply(dialog);
        require(session.snapshot()->preprocessing.background_filters.size() == 2,
                "Repeated filter history lost");
    }
    ProductWorkflowProbe::save(window, output + "-advanced.qxp");
    const auto filtered = session.snapshot();
    ProductWorkflowProbe::load(window, output + "-advanced.qxp");
    require(qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) ==
                    qualpf::infrastructure::encodePreprocessing(filtered->preprocessing) &&
                session.snapshot()->pattern.background->values == filtered->pattern.background->values,
            "Advanced background reload failed");
    {
        BackgroundDialog dialog(&window);
        dialog.setBackground();
        const auto before = session.snapshot();
        dialog.findChild<QDoubleSpinBox *>("minSpinBox")
            ->setValue(raw->pattern.two_theta[raw->pattern.two_theta.size() / 4].value);
        dialog.findChild<QDoubleSpinBox *>("maxSpinBox")
            ->setValue(raw->pattern.two_theta[3 * raw->pattern.two_theta.size() / 4].value);
        require(QMetaObject::invokeMethod(&dialog, "onApplyFilterButtonClicked", Qt::DirectConnection),
                "Interval action missing");
        require(QMetaObject::invokeMethod(&dialog, "onResetFilterButtonClicked", Qt::DirectConnection),
                "Reset filter missing");
        // Approved T3 Reset estimates the full canonical spectrum; displayed
        // interval controls belong to Filter and cannot make a fresh Reset partial.
        require(dialog.property("preprocessingReason").toString().isEmpty() &&
                    dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled() &&
                    session.snapshot() == before,
                "Full canonical Reset failed or published a preview");
        const auto resetCurve = window.xpdViewer()->graph(window.xpdViewer()->graphCount() - 1)->data();
        require(std::size_t(resetCurve->size()) == raw->pattern.observed.size() &&
                    resetCurve->constBegin()->key == raw->pattern.two_theta.front().value &&
                    (resetCurve->constEnd() - 1)->key == raw->pattern.two_theta.back().value,
                "Reset did not estimate the full exact canonical interval");
        cancel(dialog);
        require(qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) == qualpf::infrastructure::encodePreprocessing(before->preprocessing) &&
                    session.snapshot()->pattern.background->values == before->pattern.background->values &&
                    gui::encode_project_peaks(session.snapshot()->peaks) == gui::encode_project_peaks(before->peaks),
                "Reset Cancel changed canonical background/history/peaks");
    }
    require(bool(session.commit(session.snapshot(), *raw)), "Manual point source failed");
    window.xpdViewer()->peaks.visible = false;
    double pointX = raw->pattern.two_theta[raw->pattern.two_theta.size() / 2].value;
    double pointY = raw->pattern.observed[raw->pattern.observed.size() / 2];
    require(ProductWorkflowProbe::backgroundPoint(window, true, pointX, pointY) == 0,
            "Manual background add failed");
    const auto added = session.snapshot();
    require(added->preprocessing.background_points && added->pattern.background, "Manual points not owned");
    require(ProductWorkflowProbe::backgroundPoint(window, false, pointX, pointY) == 0 &&
                session.snapshot()->preprocessing.background_points->size() + 1 ==
                    added->preprocessing.background_points->size(),
            "Manual background delete failed");
    window.xpdViewer()->peaks.visible = true;
    // A defined C++ chain retains the full pool independently of the selected
    // peaks.
    require(bool(session.commit(session.snapshot(), *raw)), "Cannot restore finite-chain source");
    PreprocessingRequest request;
    request.wavelength = raw->pattern.radiation.lines.front().wavelength;
    request.tolerance = AngularWidthDegrees{.15};
    request.background = preprocessing::BackgroundOptions{};
    request.background->method = preprocessing::BackgroundMethod::spline;
    request.smoothing = preprocessing::SmoothingOptions{};
    request.peaks = preprocessing::PeakOptions{};
    request.peaks->search_range = -1;
    bool finiteChain = false;
    {
        PreprocessingEdit edit(session);
        auto prepared = edit.preview(request);
        if (output.contains("CPD2")) {
            require(!prepared && prepared.error().reason == "coincident_baseline_endpoints" && !edit.commit(),
                    "CPD2 frozen chain changed");
        } else {
            require(bool(prepared) && bool(edit.commit()), "Defined chain no longer prepares");
            finiteChain = true;
        }
    }
    QJsonArray selections;
    {
        ProductWorkflowProbe::load(window, QString::fromUtf8(QUALPF_CURRENT_POOL_FIXTURE));
        auto fixed = *session.snapshot();
        require(fixed.preprocessing.peak_candidates && fixed.preprocessing.peak_candidates->peaks.size() == 2 &&
                    fixed.peaks.tth == QVector<double>({14, 20}), "Accepted fixed two-row input changed");
        fixed.peaks.tth = fixed.peaks.tth.mid(1, 1); fixed.peaks.d = fixed.peaks.d.mid(1, 1);
        fixed.peaks.deltaD = fixed.peaks.deltaD.mid(1, 1); fixed.peaks.intensity = fixed.peaks.intensity.mid(1, 1);
        fixed.peaks.intensityOrig = fixed.peaks.intensityOrig.mid(1, 1); fixed.peaks.fwhm = fixed.peaks.fwhm.mid(1, 1);
        if (fixed.peaks.integratedIntensity) *fixed.peaks.integratedIntensity = fixed.peaks.integratedIntensity->mid(1, 1);
        require(bool(session.commit(session.snapshot(), fixed)), "Fixed selected row setup failed");
        const auto full = session.snapshot();
        require(full->preprocessing.peak_candidates->peaks.size() > std::size_t(full->peaks.tth.size()),
                "Candidate pool was reduced to selected peaks");
        ProductWorkflowProbe::save(window, output + "-pool.qxp");
        ProductWorkflowProbe::load(window, output + "-pool.qxp");
        require(qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) == qualpf::infrastructure::encodePreprocessing(full->preprocessing),
                "Pool project round trip failed");
        const auto restored = session.snapshot();
        for (int action : {3, 4, 5, 8, 9}) {
            require(bool(session.commit(session.snapshot(), *restored)), "Cannot reset oracle source");
            const double minimum = 13, maximum = 19;
            {
                PeakSearchDialog dialog(&window);
                dialog.setOptions();
                const auto before = session.snapshot();
                require(dialog.property("preprocessingReason").toString().isEmpty(),
                        "Reopen redetected saved snapshot");
                if (action >= 8)
                    dialog.findChild<QCheckBox *>("appendCheckBox")->click();
                if (action == 3 || action >= 8) {
                    dialog.findChild<QDoubleSpinBox *>("minSpinBox")->setValue(minimum);
                    dialog.findChild<QDoubleSpinBox *>("maxSpinBox")->setValue(maximum);
                }
                if (action == 4 || action == 8)
                    dialog.findChild<QDoubleSpinBox *>("intensitySpinBox")->setValue(75);
                if (action == 5 || action == 9) {
                    auto *count = dialog.findChild<QSpinBox *>("numPeaksSpinBox");
                    count->setValue(2); // Force a real count edit before the one-row cap.
                    count->setValue(1);
                }
                require(session.snapshot() == before, "Reselection preview published");
                apply(dialog);
            }
            const auto ep = session.snapshot()->peaks;
            const QVector<double> expectedAngles = action >= 8 ? QVector<double>{14, 20} : QVector<double>{14};
            require(ep.tth == expectedAngles, "Fixed-input range/75-percent/top1/Append selection changed");
            for (qsizetype i = 0; i < ep.tth.size(); ++i) {
                const auto &input = restored->preprocessing.peak_candidates->peaks[ep.tth[i] == 14 ? 0 : 1];
                require(ep.intensity[i] == input.intensity && ep.intensityOrig[i] == input.original_intensity.value() &&
                            ep.fwhm[i] == input.fwhm->value && std::isfinite(ep.d[i]) && std::isfinite(ep.deltaD[i]),
                        "Selection changed represented peak measurements");
            }
            require(session.snapshot()->preprocessing.peak_candidates->peaks.size() ==
                        full->preprocessing.peak_candidates->peaks.size(),
                    "Reselection destroyed pool");
            if (action == 9) {
                PeakSearchDialog reopened(&window);
                reopened.setOptions();
                require(!reopened.findChild<QCheckBox *>("appendCheckBox")->isChecked() &&
                            reopened.findChild<QDoubleSpinBox *>("intensitySpinBox")->value() == 0,
                        "Reopened append/count edit did not restore Legacy active conditions");
                cancel(reopened);
            }
            selections.append(QJsonObject{{"current_action", action}, {"selected", ep.tth.size()}, {"passed", true}});
        }
        require(bool(session.commit(session.snapshot(), *raw)), "Cannot restore real sensitivity source");
        int sensitivity = 0;
        {
            PeakSearchDialog dialog(&window);
            dialog.setOptions();
            const auto before = session.snapshot();
            auto *spin = dialog.findChild<QSpinBox *>("sensitivitySpinBox");
            sensitivity = spin->value() > 96 ? spin->value() - 3 : spin->value() + 3;
            spin->setValue(sensitivity);
            if (output.contains("CPD2")) {
                require(raw->peaks.tth.empty() && !raw->preprocessing.peak_candidates &&
                            dialog.property("preprocessingReason").toString().isEmpty() &&
                            dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled() &&
                            session.snapshot() == before,
                        "Defined current CPD2 sensitivity preview reused stale state or published");
                const auto detected = window.xpdViewer()->graph(window.xpdViewer()->graphCount() - 1)->data();
                require(!detected->isEmpty(), "Defined CPD2 sensitivity did not produce a new private detection");
                for (auto peak = detected->constBegin(); peak != detected->constEnd(); ++peak)
                    require(std::isfinite(peak->key) && std::isfinite(peak->value) && peak->value >= 0 &&
                                peak->key >= raw->pattern.two_theta.front().value &&
                                peak->key <= raw->pattern.two_theta.back().value,
                            "Defined sensitivity preview contains invalid measurements");
            } else {
                require(dialog.property("preprocessingReason") == "coincident_baseline_endpoints" &&
                            !dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled() &&
                            session.snapshot() == before,
                        "Sensitivity failed detection published/reused old peaks");
            }
            cancel(dialog);
            require(qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) == qualpf::infrastructure::encodePreprocessing(before->preprocessing) &&
                        gui::encode_project_peaks(session.snapshot()->peaks) == gui::encode_project_peaks(before->peaks) &&
                        session.snapshot()->pattern.observed == before->pattern.observed,
                    "Sensitivity Cancel changed canonical state");
        }
        selections.append(QJsonObject{{"current_action", 7}, {"expected_atomic_rejection", !output.contains("CPD2")}, {"zero_publication", true}});
        require(bool(session.commit(session.snapshot(), *restored)), "Cannot restore fixed pool failure source");
        // A failed new detection cannot expose an older pool as a successful
        // preview.
        {
            PreprocessingEdit edit(session);
            PreprocessingRequest bad;
            bad.peaks = preprocessing::PeakOptions{};
            bad.peaks->polynomial_degree = 2;
            bad.wavelength = request.wavelength;
            bad.tolerance = request.tolerance;
            require(!edit.preview(bad), "Malformed detection unexpectedly succeeded");
            bad.peaks.reset();
            bad.peak_operation = PeakOperation::reselect;
            bad.selection = PeakSelection{};
            auto failed = edit.preview(bad);
            require(!failed && failed.error().reason == "candidate_pool_invalidated" && !edit.commit(),
                    "Failed detection reused an older pool");
        }
        auto staleSource = session.snapshot();
        PreprocessingEdit stale(session);
        PreprocessingRequest reselect;
        reselect.peak_operation = PeakOperation::reselect;
        reselect.selection = PeakSelection{};
        reselect.wavelength = request.wavelength;
        reselect.tolerance = request.tolerance;
        require(bool(stale.preview(reselect)), "Finite pool could not reselect");
        require(bool(session.commit(staleSource, *staleSource)) && !stale.commit(),
                "Stale reselection committed");
        stale.cancel();
        auto missing = *session.snapshot();
        missing.preprocessing.peak_candidates.reset();
        missing.preprocessing.candidate_failure.reset();
        require(bool(session.commit(session.snapshot(), missing)), "Missing-pool setup failed");
        PreprocessingEdit unavailable(session);
        auto status = unavailable.preview(reselect);
        require(!status && status.error().reason == "candidate_pool_unavailable" && !unavailable.commit(),
                "Missing pool silently redetected/reused selected peaks");
    }
    QFile report(output + ".json");
    require(report.open(QIODevice::WriteOnly), "Cannot save Phase 9.3 report");
    report.write(QJsonDocument(QJsonObject{{"historical_native_observations", "docs/refactoring/phase-9.3/product93-*.json"},
        {"represented_invalid_pool_fixture", true}, {"candidate_contract", "nonfinite_candidate_measurement"},
        {"cpp_product_actions", actions}, {"raw_scaling_smoothing_history", true}, {"subtraction_menu", true},
        {"advanced_filter_manual_gui", true}, {"real_input_defined_chain", finiteChain},
        {"pool_reselection_project_roundtrip", selections}, {"current_fixed_pool_rows", 2}}).toJson());
    qunsetenv("QUALPF_PREPROCESSING_BACKEND");
    std::cout << "PASS: current history, represented invalid pool, menu "
                 "atomicity and pool GUI round trip\n";
}
