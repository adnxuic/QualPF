void kalpha2Workflow12(MainWindow &window, const QString &output) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    backend96();
    auto &session = AppState::searchSession();
    const auto raw = session.snapshot();
    ProductWorkflowProbe::strip(window);
    require(session.snapshot()->pattern.processing.kalpha2_subtracted == true &&
                session.snapshot()->pattern.original_observed == raw->pattern.original_observed,
            "Measured GUI stripping");
    const auto stripped = session.snapshot();
    PreprocessingRequest request;
    request.background = preprocessing::BackgroundOptions{};
    request.background->method = preprocessing::BackgroundMethod::none;
    request.smoothing = preprocessing::SmoothingOptions{};
    request.smoothing->half_window = 4;
    request.smoothing->polynomial_degree = 5;
    request.wavelength = stripped->pattern.radiation.lines.front().wavelength;
    request.tolerance = AngularWidthDegrees{.15};
    PreprocessingEdit edit(session);
    auto prepared = edit.preview(request);
    if (!prepared)
        throw std::runtime_error("Poststrip preprocessing: " + prepared.error().reason);
    require(bool(edit.commit()), "Poststrip preprocessing commit");
    const auto beforePeaks = session.snapshot();
    ProductWorkflowProbe::automaticPeaks(window);
    require(session.snapshot() != beforePeaks && session.snapshot()->peaks.valid &&
                !session.snapshot()->preprocessing.rejected_candidates.empty(), "Poststrip measurable automatic peaks");
    const auto rejectedCount = int(session.snapshot()->preprocessing.rejected_candidates.size());
    auto diagnostic = applyPreprocessing(session, PreprocessingAction::automatic_peaks);
    require(bool(diagnostic) && session.snapshot()->peaks.valid, "Repeat poststrip automatic peaks");
    // Keep the separate independent manual measurements for search continuation.
    PreprocessingRequest clear;
    clear.manual = ManualPeakChange{};
    clear.manual->operation = ManualPeakChange::Operation::clear;
    clear.wavelength = request.wavelength;
    clear.tolerance = request.tolerance;
    PreprocessingEdit clearEdit(session);
    require(bool(clearEdit.preview(clear)) && bool(clearEdit.commit()), "Clear detected peaks before manual fixture");
    const double positions[] = {20.86, 26.64, 36.54}, heights[] = {20, 100, 10};
    for (int i = 0; i < 3; ++i) {
        PreprocessingRequest manual;
        manual.manual = ManualPeakChange{};
        manual.manual->two_theta = positions[i];
        manual.manual->intensity = heights[i];
        manual.manual->fwhm = .12;
        manual.wavelength = request.wavelength;
        manual.tolerance = request.tolerance;
        PreprocessingEdit manualEdit(session);
        require(bool(manualEdit.preview(manual)) && bool(manualEdit.commit()),
                "Explicit finite manual input after stripping");
    }
    window.refreshPatternPresentation();
    window.refreshPreprocessingDisplay();
    const auto finite = session.snapshot();
    require(finite->peaks.valid && finite->peaks.tth.size() == 3 && !finite->preprocessing.peak_candidates &&
                finite->pattern.processing.kalpha2_subtracted == true,
            "Finite explicit manual snapshot");
    const auto direct = search96(window);
    require(!direct.empty(), "Poststrip search has real candidates");
    compareCpp(finite->pattern, finite->peaks, session.snapshot()->candidates);
    ProductWorkflowProbe::save(window, output + ".qxp");
    ProductWorkflowProbe::load(window, output + ".qxp");
    require(session.snapshot()->pattern.processing.kalpha2_subtracted == true &&
                session.snapshot()->pattern.original_kalpha2_subtracted == false,
            "Poststrip project history");
    const auto restored = search96(window);
    require(direct == restored, "Poststrip project Search-Match continuation");
#ifdef QUALPF_PATTERN_LINK_GUARD
    require(qualpf_test_native_strips() == 0, "Measured default C++ action calls no native stripping");
#endif
    write96(output + ".json", {{"passed", true},
                               {"workflow", "C++ import -> Kalpha2 -> None background -> SG(4,5) -> "
                                            "measurable automatic peaks; separate explicit manual "
                                            "peaks -> GUI Search-Match -> save/load -> same search"},
                               {"automatic_peak_result", "measurable_candidates"},
                               {"rejected_candidates", rejectedCount},
                               {"selected_peaks", finite->peaks.tth.size()},
                               {"candidate_pool", "unavailable; never reconstructed"},
                               {"direct_search", direct},
                               {"restored_search", restored},
                               {"native_stripping_calls", 0}});
    close96(window);
}
