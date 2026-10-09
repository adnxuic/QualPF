// Phase 9.3 checks run against real product dialogs and the native pool oracle.
void phase93ProductChecks(MainWindow &window, const QString &output, qualpf::gui::SessionViewHandle raw) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    auto &session = AppState::searchSession();
    const auto legacySnapshot = session.snapshot();
    require(raw->pattern.original_observed && raw->pattern.processing.intensity_scaled == true &&
                raw->preprocessing.smoothing_applied == false,
            "Fresh native history missing");
    require(legacySnapshot->pattern.processing.background_subtracted == true &&
                legacySnapshot->pattern.background->window->first == 0,
            "Native background alignment/history lost");
    require(legacySnapshot->preprocessing.candidate_failure == "nonfinite_candidate_measurement" &&
                !legacySnapshot->preprocessing.peak_candidates,
            "Undefined native candidate pool entered Session");
    bool scaled = false;
    int first, bg[5], sm[3], pk[5], n, np;
    float numbers[7];
    ::get_preprocessing_metadata(&scaled, &first, bg, sm, pk, numbers, &n, &np);
    QVector<float> values(n * 5);
    ::get_preprocessing_candidates(n, values.data());
    QJsonArray invalid;
    for (int i = 0; i < n; ++i) {
        QJsonArray columns;
        for (int j = 0; j < 5; ++j)
            if (!std::isfinite(values[5 * i + j]))
                columns.append(j);
        if (!columns.empty())
            invalid.append(
                QJsonObject{{"candidate", i}, {"angle", values[5 * i]}, {"nonfinite_columns", columns}});
    }
    require(!invalid.empty(), "Native undefined fixture no longer contains invalid candidates");
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
        require(infrastructure::encode_project_peaks(session.snapshot()->peaks) ==
                    infrastructure::encode_project_peaks(legacySnapshot->peaks),
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
        const auto &a = session.snapshot()->pattern.observed;
        const auto &b = legacySnapshot->pattern.observed;
        require(a.size() == b.size(), "Subtraction size changed");
        for (std::size_t i = 0; i < a.size(); ++i)
            close(a[i], b[i], 1e-5, 1e-5, "Product subtraction vs native");
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
    require(encodePreprocessing(session.snapshot()->preprocessing) ==
                    encodePreprocessing(filtered->preprocessing) &&
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
        require(dialog.property("preprocessingReason") == "partial_brueckner_unassigned_output" &&
                    !dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled() &&
                    session.snapshot() == before,
                "Partial new Brueckner did not reject atomically");
        cancel(dialog);
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
    if (finiteChain) {
        const auto full = session.snapshot();
        require(full->preprocessing.peak_candidates &&
                    full->preprocessing.peak_candidates->peaks.size() > std::size_t(full->peaks.tth.size()),
                "Candidate pool was reduced to selected peaks");
        ProductWorkflowProbe::save(window, output + "-pool.qxp");
        ProductWorkflowProbe::load(window, output + "-pool.qxp");
        require(encodePreprocessing(session.snapshot()->preprocessing) ==
                    encodePreprocessing(full->preprocessing),
                "Pool project round trip failed");
        const auto restored = session.snapshot();
        for (int action : {3, 4, 5, 8, 9}) {
            require(bool(session.commit(session.snapshot(), *restored)), "Cannot reset oracle source");
            window.refreshPatternPresentation(); legacy::mirror_search_input();
            peakSearchSettings settings{};
            ::peak_search_action(1, &settings);
            if (action >= 8) {
                settings.append = true;
                ::peak_search_action(3, &settings);
            }
            const double minimum = restored->pattern.two_theta[restored->pattern.two_theta.size() / 4].value;
            const double maximum =
                restored->pattern.two_theta[3 * restored->pattern.two_theta.size() / 4].value;
            if (action == 3 || action >= 8) {
                settings.minSearch = std::round(minimum * 100) / 100;
                ::peak_search_action(3, &settings);
                settings.maxSearch = std::round(maximum * 100) / 100;
                ::peak_search_action(3, &settings);
            }
            if (action == 4 || action == 8) {
                settings.threshold = 20;
                ::peak_search_action(4, &settings);
            }
            if (action == 5 || action == 9) {
                settings.numPeaks = 7;
                ::peak_search_action(5, &settings);
            }
            const int count = ::peak_number();
            QVector<float> d(count), dd(count), angle(count), height(count), width(count);
            double wave;
            ::get_d_delta_values(d.data(), dd.data(), angle.data(), height.data(), width.data(), &wave, .15);
            ::peak_search_action(0, &settings);
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
                    dialog.findChild<QDoubleSpinBox *>("intensitySpinBox")->setValue(20);
                if (action == 5 || action == 9)
                    dialog.findChild<QSpinBox *>("numPeaksSpinBox")->setValue(7);
                require(session.snapshot() == before, "Reselection preview published");
                apply(dialog);
            }
            const auto ep = session.snapshot()->peaks;
            require(ep.tth.size() == count, "Reselection count differs from Legacy");
            for (int i = 0; i < count; ++i) {
                close(ep.tth[i], angle[i], 1e-4, 0, "Reselection angle");
                close(ep.d[i], d[i], 1e-5, 1e-6, "Reselection d");
                close(ep.deltaD[i], dd[i], 1e-5, 1e-6, "Reselection delta d");
                close(ep.intensity[i], height[i], 1e-5, 1e-5, "Reselection height");
                close(ep.fwhm[i], width[i], 1e-5, 1e-5, "Reselection width");
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
            selections.append(QJsonObject{{"legacy_action", action}, {"selected", count}, {"passed", true}});
        }
        require(bool(session.commit(session.snapshot(), *restored)), "Cannot restore sensitivity source");
        window.refreshPatternPresentation(); legacy::mirror_search_input();
        peakSearchSettings sensitivitySettings{};
        ::peak_search_action(1, &sensitivitySettings);
        const int sensitivity = sensitivitySettings.sensitivity > 96 ? sensitivitySettings.sensitivity - 3
                                                                     : sensitivitySettings.sensitivity + 3;
        sensitivitySettings.sensitivity = sensitivity;
        ::peak_search_action(7, &sensitivitySettings);
        int sensitivityCount = -1;
        ::get_preprocessing_metadata(&scaled, &first, bg, sm, pk, numbers, &sensitivityCount, &np);
        QVector<float> sensitivityPool(sensitivityCount * 5);
        ::get_preprocessing_candidates(sensitivityCount, sensitivityPool.data());
        const int invalidSensitivity = int(std::count_if(sensitivityPool.begin(), sensitivityPool.end(),
                                                         [](float v) { return !std::isfinite(v); }));
        require(invalidSensitivity > 0, "Sensitivity undefined fixture changed");
        ::peak_search_action(0, &sensitivitySettings);
        {
            PeakSearchDialog dialog(&window);
            dialog.setOptions();
            const auto before = session.snapshot();
            dialog.findChild<QSpinBox *>("sensitivitySpinBox")->setValue(sensitivity);
            require(dialog.property("preprocessingReason") == "coincident_baseline_endpoints" &&
                        !dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled() &&
                        session.snapshot() == before,
                    "Sensitivity failed detection published/reused old peaks");
            cancel(dialog);
        }
        selections.append(QJsonObject{{"legacy_action", 7},
                                      {"native_nonfinite_fields", invalidSensitivity},
                                      {"expected_atomic_rejection", true}});
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
    report.write(QJsonDocument(QJsonObject{{"native_pool_count", n},
                                           {"native_invalid_candidates", invalid},
                                           {"legacy_finite_selected", legacySnapshot->peaks.tth.size()},
                                           {"candidate_contract", "nonfinite_candidate_measurement"},
                                           {"cpp_product_actions", actions},
                                           {"raw_scaling_smoothing_history", true},
                                           {"subtraction_menu", true},
                                           {"advanced_filter_manual_gui", true},
                                           {"pool_reselection_project_roundtrip", selections},
                                           {"default_switch", false}})
                     .toJson());
    qunsetenv("QUALPF_PREPROCESSING_BACKEND");
    std::cout << "PASS: Phase 9.3 native history, explicit undefined pool, menu "
                 "atomicity and pool GUI round trip\n";
}
