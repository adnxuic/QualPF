// Included in product probe after its private GUI access and assertion helpers.
void kalpha2Lifecycle12(const QString &tables, const QString &output) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    const bool fallback = qEnvironmentVariable("QUALPF_KALPHA2_BACKEND") == "legacy";
    if (!fallback)
        qunsetenv("QUALPF_KALPHA2_BACKEND"); // Exercise actual default.
    auto &session = AppState::searchSession();
    MainWindow window(AppState::context());
    MainWindow::setPathDataFiles(tables);
    qualpf::application::StartupOptions options{0, 1, 0, -1};
    const auto directory = QFile::encodeName(QDir(tables).absolutePath() + "/");
    int error = 0;
    legacy::initialize_native_workspace(&options, directory.constData(), directory.size(), &error);
    require(error == 0, "Kalpha initialization");
    QTemporaryDir files;
    const QString input = files.path() + "/doublet.xy";
    QFile f(input);
    require(f.open(QIODevice::WriteOnly), "Kalpha input open");
    QByteArray bytes;
    for (int i = 0; i <= 4750; ++i) {
        const double x = 5 + i * .02;
        const double y = 3 + 100 * std::exp(-.5 * std::pow((x - 30) / .12, 2)) +
                         70 * std::exp(-.5 * std::pow((x - 60) / .2, 2));
        bytes += QByteArray::number(x, 'g', 17) + " " + QByteArray::number(y, 'g', 17) + "\n";
    }
    f.write(bytes);
    f.close();
    require(window.loadDiffractionPatterns({input}, false), "Kalpha GUI load");
    auto initial = session.snapshot();
    auto dirty = *initial;
    const auto n = dirty.pattern.observed.size();
    dirty.pattern.background = SampledSignal{std::vector<double>(n, 1), SampleWindow{0}};
    dirty.pattern.calculated = dirty.pattern.background;
    dirty.pattern.background_subtracted_signal = dirty.pattern.background;
    dirty.pattern.weights = std::vector<double>(n, 1);
    dirty.preprocessing.background_points = std::vector<preprocessing::BackgroundPoint>{{5, 1}, {100, 1}};
    dirty.preprocessing.background_filters.push_back({5, 100, 2, 1});
    dirty.preprocessing.peak_candidates = PeakSet{};
    dirty.preprocessing.peak_candidates->dataset = {1};
    dirty.preprocessing.peak_candidates->effective_wavelength = radiation::default_wavelength;
    dirty.preprocessing.selection = PeakSelection{};
    dirty.peaks.valid = true;
    dirty.peaks.tth = {30};
    dirty.peaks.d = {3};
    dirty.peaks.deltaD = {.1};
    dirty.peaks.intensity = {50};
    dirty.peaks.intensityOrig = {100};
    dirty.peaks.fwhm = {.1};
    CardType card;
    card.setId("previous");
    dirty.candidates.append(card);
    dirty.phases.append(card);
    dirty.peakTolerance = .2;
    dirty.automaticDelta = .37;
    const auto seeded = session.commit(initial, std::move(dirty));
    if (!seeded)
        throw std::runtime_error("Kalpha seed: " + seeded.error().field + ": " + seeded.error().message);
    initial = session.snapshot();
    PreprocessingRequest strip;
    strip.strip_kalpha2 = true;
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(strip)) && edit.preview(), "Kalpha private preview");
        require(session.snapshot() == initial, "Kalpha preview cannot publish");
        auto mixed = strip;
        mixed.smoothing = preprocessing::SmoothingOptions{};
        require(!edit.preview(mixed) && !edit.preview(), "Failed preview clears old draft");
        require(!edit.commit() && session.snapshot() == initial, "Failed apply atomic");
    }
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(strip)), "Kalpha cancel preview");
        edit.cancel();
        require(!edit.commit() && session.snapshot() == initial, "Kalpha cancel atomic");
    }
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(strip)), "Kalpha stale preview");
        auto changed = *initial;
        changed.projectMetadata["test_revision"] = 1;
        require(bool(session.commit(initial, std::move(changed))), "Intervening edit");
        auto current = session.snapshot();
        require(!edit.commit() && session.snapshot() == current, "Kalpha stale apply atomic");
    }
    initial = session.snapshot();
    const auto expected = preprocessing::strip_kalpha2(initial->pattern);
    require(bool(expected), "Expected core result");
    {
        // A deliberately unrelated native dataset must not become stripping input.
        legacy::ScopedSearchImportPause pause;
        float x[]{15, 16, 17}, y[]{91, 82, 73}, wave = 1.54059307f, ratio = 1, unused = 0;
        ::set_diffraction_data(x, y, 3, &unused, 0, false, false, &wave, &ratio, 1, 0, "poison", 6, false,
                               false);
    }
    int notifications = 0;
    const auto connection = QObject::connect(&session, &SessionView::changed, [&] { ++notifications; });
    ProductWorkflowProbe::strip(window);
    QObject::disconnect(connection);
    auto stripped = session.snapshot();
    require(notifications == 1 && stripped->revision == initial->revision + 1,
            "Single atomic GUI publication");
    require(stripped->pattern.observed == expected.value(), "GUI actual stripped intensities");
    require(stripped->pattern.original_observed == initial->pattern.original_observed &&
                stripped->pattern.original_kalpha2_subtracted == false,
            "Raw intensity and provenance preserved");
    require(stripped->pattern.two_theta == initial->pattern.two_theta &&
                stripped->pattern.processing.kalpha2_subtracted == true,
            "Grid and success flag");
    require(!stripped->pattern.background && !stripped->pattern.calculated &&
                !stripped->pattern.background_subtracted_signal && !stripped->pattern.weights &&
                !stripped->preprocessing.background_points &&
                stripped->preprocessing.background_filters.empty() &&
                !stripped->preprocessing.peak_candidates && !stripped->preprocessing.candidate_failure &&
                !stripped->preprocessing.selection && stripped->peaks.tth.empty() &&
                stripped->candidates.empty() && stripped->phases.empty() && !stripped->peakTolerance &&
                stripped->automaticDelta == .5,
            "All stale derived state invalidated");
    ProductWorkflowProbe::strip(window);
    require(session.snapshot() == stripped, "Repeated GUI stripping is atomic rejection");
    const auto project = files.path() + "/stripped.qxp";
    ProductWorkflowProbe::save(window, project);
    ProductWorkflowProbe::load(window, project);
    auto loaded = session.snapshot();
    require(loaded->pattern.observed == stripped->pattern.observed &&
                loaded->pattern.original_observed == stripped->pattern.original_observed &&
                loaded->pattern.processing.kalpha2_subtracted == true &&
                loaded->pattern.original_kalpha2_subtracted == false,
            "Project round-trip stripping and raw history");
    legacy::mirror_search_input();
    legacy::import_search_input();
    require(session.snapshot()->pattern.processing.kalpha2_subtracted == true &&
                session.snapshot()->pattern.original_kalpha2_subtracted == false,
            "Native transport preserves known current/raw history");
    ProductWorkflowProbe::load(window, project);
    PreprocessingRequest restart;
    restart.restart_from_original = true;
    {
        qualpf::application::Workspace isolatedOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView isolated(isolatedOwner.session());
        auto knownStrippedRaw = *stripped;
        knownStrippedRaw.pattern.original_kalpha2_subtracted = true;
        require(bool(isolated.commit(isolated.snapshot(), knownStrippedRaw)), "Known stripped raw fixture");
        PreprocessingEdit recovery(isolated);
        require(bool(recovery.preview(restart)) && bool(recovery.commit()) &&
                    isolated.snapshot()->pattern.processing.kalpha2_subtracted == true,
                "Restart respects already-stripped raw provenance");
        auto failedPool = *initial;
        failedPool.preprocessing.peak_candidates.reset();
        failedPool.preprocessing.candidate_failure = "nonfinite_candidate_measurement";
        require(bool(isolated.commit(isolated.snapshot(), std::move(failedPool))),
                "Historical failed pool fixture");
        PreprocessingEdit clearFailure(isolated);
        require(bool(clearFailure.preview(strip)) && !clearFailure.preview()->preprocessing.candidate_failure,
                "Stripping invalidates prior candidate failure");
    }
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(restart)) && bool(edit.commit()), "Restart known raw");
        require(session.snapshot()->pattern.processing.kalpha2_subtracted == false &&
                    session.snapshot()->pattern.observed == *stripped->pattern.original_observed,
                "Known raw restores unstripped state");
        require(!edit.commit(), "Double apply closed");
    }
    ProductWorkflowProbe::load(window, project);
    // Existing C++ preprocessing continues after invalidation.
    PreprocessingRequest continuation;
    continuation.wavelength = radiation::default_wavelength;
    continuation.background = preprocessing::BackgroundOptions{};
    continuation.smoothing = preprocessing::SmoothingOptions{};
    {
        PreprocessingEdit edit(session);
        auto result = edit.preview(continuation);
        require(bool(result) && bool(edit.commit()), "Background/smoothing continuation");
    }
    ProductWorkflowProbe::automaticPeaks(window);
    require(session.snapshot()->pattern.processing.kalpha2_subtracted == true,
            "Peaks preserve stripping flag");
    auto historical = *session.snapshot();
    historical.pattern.processing.kalpha2_subtracted.reset();
    historical.pattern.original_kalpha2_subtracted.reset();
    require(bool(session.commit(session.snapshot(), std::move(historical))), "Unknown history fixture");
    ProductWorkflowProbe::save(window, files.path() + "/unknown.qxp");
    ProductWorkflowProbe::load(window, files.path() + "/unknown.qxp");
    auto unknown = session.snapshot();
    require(!unknown->pattern.processing.kalpha2_subtracted && !unknown->pattern.original_kalpha2_subtracted,
            "Missing project flags remain unknown");
    ProductWorkflowProbe::strip(window);
    require(session.snapshot() == unknown, "Unknown history rejection");
    {
        PreprocessingEdit edit(session);
        require(bool(edit.preview(restart)) && bool(edit.commit()), "Unknown raw restart");
        require(!session.snapshot()->pattern.processing.kalpha2_subtracted,
                "Raw existence does not invent stripping history");
    }
    // Validate that native mirroring/import preserves unknown state as well.
    legacy::mirror_search_input();
    legacy::import_search_input();
    require(!session.snapshot()->pattern.processing.kalpha2_subtracted &&
                !session.snapshot()->pattern.original_kalpha2_subtracted,
            "Native transport preserves unknown history");
    require(window.loadDiffractionPatterns({input}, false), "Fresh replacement for radiation tests");
    auto unsupported = *session.snapshot();
    unsupported.pattern.radiation.lines[0].wavelength = WavelengthAngstrom{.71073};
    require(bool(session.commit(session.snapshot(), std::move(unsupported))), "Non-Cu fixture");
    auto before = session.snapshot();
    ProductWorkflowProbe::strip(window);
    require(session.snapshot() == before, "Non-Cu GUI rejection");
    unsupported = *before;
    unsupported.pattern.radiation.lines[0].wavelength = radiation::default_wavelength;
    unsupported.projectMetadata["synchrotron"] = true;
    require(bool(session.commit(before, std::move(unsupported))), "Synchrotron fixture");
    before = session.snapshot();
    ProductWorkflowProbe::strip(window);
    require(session.snapshot() == before, "Synchrotron GUI rejection");
#ifdef QUALPF_PATTERN_LINK_GUARD
    require(qualpf_test_native_strips() == 0,
            "Default and selected compatibility Kalpha2 execute only C++ values");
#endif
    QJsonObject report{{"passed", true},
                       {"backend", fallback ? "legacy" : "default_cpp"},
                       {"atomic_preview_cancel_failure_stale", true},
                       {"single_commit_invalidation", true},
                       {"project_restart_known_unknown", true},
                       {"preprocessing_continuation", true},
                       {"radiation_repeated_rejections", true},
                       {"native_numeric_calls", 0}};
    QFile out(output);
    require(out.open(QIODevice::WriteOnly), "Kalpha report");
    out.write(QJsonDocument(report).toJson());
}
