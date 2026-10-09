#include "qualpf/preprocessing/kalpha2.h"
#include "gui/search/search_request.h"
#include "infrastructure/settings/database_settings.h"
#include "tests/product/support/await_search.h"
#include "tests/support/project_peaks.h"
#include "tests/product/support/product_startup.h"
#include "gui/state/pattern_input.h"
#include "gui/database/databasebuilder.h"
#include "gui/export/pattern_export_service.h"
#include "qualpf/domain/radiation.h"
#include "gui/adapters/experimental_peaks.h"
#include "gui/search/searchmatch.h"
#include "infrastructure/database/searchutil.h"
#include "tests/support/workspace_fixture.h"
#include "gui/state/preprocessing_edit.h"
#include "gui/state/preprocessing_product.h"
#include "gui/dialogs/backgrounddialog.h"
#include "gui/dialogs/peaksearchdialog.h"
#include "gui/dialogs/restraintsdialog.h"
#include "gui/dialogs/searchoptionsdialog.h"
#include "gui/dialogs/smoothingdialog.h"
#include "gui/dialogs/manual_peak_dialog.h"
#include "gui/widgets/peaklistwidget.h"
#include <QLineEdit>
#include <QFileDialog>
#include <QStatusBar>
#include <QLabel>
#include "gui/dialogs/wavedialog.h"
#include "gui/mainwindow.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/widgets/dbresultswidget.h"
#include "gui/widgets/quantwidget.h"
#include "gui/widgets/wavelengthswidget.h"
#include "gui/adapters/card_projection.h"
#include "infrastructure/io/project_peaks.h"
#include "qualpf/search/classic.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QInputDialog>
#include <QSpinBox>
#include <QTableView>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QProgressDialog>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <initializer_list>
#include <utility>
#include <stdexcept>
#ifdef QUALPF_PATTERN_LINK_GUARD
extern "C" int qualpf_test_native_pattern_reads();
extern "C" int qualpf_test_native_strips();
#endif

class ProductWorkflowProbe {
  public:
    static void save(MainWindow &window, const QString &path) { window.saveProject(path); }
    static void load(MainWindow &window, const QString &path) { window.loadProject(path); }
    static void residual(MainWindow &window, const CardType &card) { window.performResidualSearch(card); awaitSearch(window); }
    static void rescore(MainWindow &window) { window.recalculateFOMs(); awaitSearch(window); }
    static void restraints(MainWindow &window, qualpf::gui::SearchRequest &query) { window.applyDialogRestraints(query); }
    static int backgroundPoint(MainWindow &window, bool add, double x, double y) {
        int error=0;window.addDeleteSelectedPoint(add ? MainWindow::AddBackgroundPoint : MainWindow::DeleteBackgroundPoint,x,y,error);return error;
    }
    static int pointAction(MainWindow &window, int action) {
        int error=0;window.addDeleteSelectedPoint(action,11,4,error);return error;
    }
    static void automaticPeaks(MainWindow &window) { window.onActionPeakSearchTriggered(); }
    static void search(MainWindow &window) { window.onActionSearchMatchTriggered(); awaitSearch(window); }
    static void deletePeaks(MainWindow &window, const QVector<int> &rows) { window.deleteSelectedPeaks(rows); }
    static void subtract(MainWindow &window) { window.onActionSubtractBackgroundTriggered(); }
    static void strip(MainWindow &window) { window.onActionKAlpha2StrippingTriggered(); }
    static void manual(MainWindow &window) { window.onActionLoadAddTriggered(); awaitSearch(window); }
    static void fresh(MainWindow &window) { window.projectFileSaved = true; window.onActionNewTriggered(); }
};

namespace {
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}

#include "kalpha2_lifecycle.h"

void pdf2Lifecycle34(const QString &output) {
    QTemporaryDir files;
    require(files.isValid(), "PDF2 temporary directory");
    const auto before = AppState::searchSession().snapshot();
    auto record = [](char kind, std::initializer_list<std::pair<int, QByteArray>> fields) {
        QByteArray bytes(80, ' ');
        bytes.replace(72, 6, "990001");
        bytes[79] = kind;
        for (const auto &[offset, value] : fields) bytes.replace(offset, value.size(), value);
        return bytes;
    };
    // Exact integer inputs give an independent, literal expected serialized row.
    const auto card = record('1', {{0,"  4.00000"},{9,"  4.00000"},{18,"  4.00000"},
                                  {27,"  90.000"},{35,"  90.000"},{43,"  90.000"},{78,"C"}}) +
                      record('6', {{0,"PDF2 owner regression"}}) +
                      record('7', {{0,"Co O"}}) +
                      record('I', {{0,"4.00000"},{7,"100"},{23,"2.00000"},{30," 80"},
                                   {46,"1.00000"},{53," 60"}}) + record('K', {});
    auto write = [&](const QString &name, const QByteArray &bytes) {
        const auto path = files.filePath(name);
        QFile file(path);
        require(file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size(), "PDF2 fixture write");
        return path;
    };
    const auto source = write("cards.dat", card);
    auto stores = [&](const QString &prefix) {
        QJsonObject result;
        for (const auto &suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"}) {
            const auto connection = QString("pdf2_probe_%1").arg(suffix);
            {
                auto db = QSqlDatabase::addDatabase("QSQLITE", connection);
                db.setDatabaseName(prefix + suffix);
                require(db.open(), "PDF2 store reopen");
                QJsonObject tables;
                QSqlQuery names(db);
                require(names.exec("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"), "PDF2 table names");
                while (names.next()) {
                    QSqlQuery rows(db);
                    require(rows.exec("SELECT * FROM \"" + names.value(0).toString() + "\" ORDER BY rowid"), "PDF2 rows");
                    QJsonArray values;
                    while (rows.next()) {
                        QJsonArray row;
                        for (int i=0; i<rows.record().count(); ++i) row.append(QJsonValue::fromVariant(rows.value(i)));
                        values.append(row);
                    }
                    tables[names.value(0).toString()] = values;
                }
                result[suffix] = tables;
                db.close();
            }
            QSqlDatabase::removeDatabase(connection);
        }
        return result;
    };
    const auto plain = files.filePath("plain");
    require(DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), plain, source, DatabaseBuilder::ProgressFn{}), "PDF2 callback entry success without native resources");
    require(qualpf::infrastructure::ReferenceDatabaseInfo::queryEntries(plain)==1 && qualpf::infrastructure::ReferenceDatabaseInfo::queryContentType(plain)=="PDF2", "PDF2 count/type");
    const auto expected = stores(plain);
    const auto row = expected[".sq"].toObject()["id"].toArray().at(0).toArray();
    // Schema order: name1, nd9, dvalue10, intensita11.
    require(row.at(0).toInt()==990001 && row.at(1).toString()=="PDF2 owner regression" &&
            row.at(10).toString()=="4.000000,2.000000,1.000000," &&
            row.at(11).toString()=="100.000000,80.000000,60.000000," && row.at(9).toInt()==3,
            "PDF2 exact independently specified row");
    require(DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), plain, source, DatabaseBuilder::ProgressFn{}) && stores(plain)==expected,
            "PDF2 repeated callback build exact");
    QWidget parent;
    bool cancelled = false;
    const auto gui = files.filePath("gui");
    require(DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), gui, source, &parent, &cancelled) && !cancelled && stores(gui)==expected,
            "PDF2 real GUI entry exact four stores without native resources");
    require(!DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), files.filePath("missing-input"), files.filePath("absent.dat"), DatabaseBuilder::ProgressFn{}),
            "PDF2 missing input callback error");
    require(!DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), files.filePath("missing-input-gui"), files.filePath("absent.dat"), &parent, &cancelled) && !cancelled,
            "PDF2 missing input GUI error");
    const auto invalidOutput = files.filePath("absent-parent/database");
    require(!DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), invalidOutput, source, DatabaseBuilder::ProgressFn{}), "PDF2 callback output-create error");
    require(!DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), invalidOutput, source, &parent, &cancelled) && !cancelled, "PDF2 GUI output-create error");
    QByteArray many;
    for (int i=0; i<10001; ++i) {
        auto next = card;
        const auto id = QByteArray::number(700000+i);
        for (int at=72; at<next.size(); at+=80) next.replace(at, 6, id);
        many += next;
    }
    const auto manySource = write("many.dat", many);
    bool cancelDelivered = false;
    QTimer::singleShot(0, [&] {
        for (auto *widget : QApplication::topLevelWidgets()) {
            if (auto *dialog = qobject_cast<QProgressDialog *>(widget)) {
                dialog->cancel();
                cancelDelivered = true;
            }
        }
    });
    const auto partial = files.filePath("cancelled");
    require(!DatabaseBuilder::buildPdfDatabase(AppState::context().databaseBuild(), partial, manySource, &parent, &cancelled) && cancelled && cancelDelivered,
            "PDF2 actual GUI cancellation");
    require(qualpf::infrastructure::ReferenceDatabaseInfo::queryEntries(partial)==10000, "PDF2 existing partial-commit cancellation contract");
    require(AppState::searchSession().snapshot()==before, "PDF2 preserves complete measurement/Search state");
    QFile report(output);
    require(report.open(QIODevice::WriteOnly), "PDF2 report open");
    report.write(QJsonDocument(QJsonObject{{"status","PASS"},{"stores",expected},{"cancelled_cards",10000},
                                         {"same_session",true},{"native_tables_required",false}}).toJson());
    std::cout << "PASS: PDF2 callback/GUI exact stores, repeat/error/cancel, resource independence\n";
}

void patternLifecycle11(const QString &tables, const QString &output) {
    using namespace qualpf;
    auto &session = AppState::searchSession();
    MainWindow window(AppState::context());
    MainWindow::setPathDataFiles(tables);
    qualpf::application::StartupOptions options{0,1,0,-1};
    const auto directory=QFile::encodeName(QDir(tables).absolutePath()+"/");
    int error=0;
    legacy::initialize_product_presentation_for_probe(&options,directory.constData(),directory.size(),&error);
    require(error==0,"Pattern lifecycle native initialization");
    QTemporaryDir files;
    auto write = [&](const QString &name, const QByteArray &contents) {
        auto path=files.path()+"/"+name;
        QFile file(path); require(file.open(QIODevice::WriteOnly),"Fixture write");
        require(file.write(contents)==contents.size(),"Fixture complete"); return path;
    };
    const auto first=write(QString::fromUtf8("实验 first.xy"),"10 2\n11 4\n12 8\n");
    const auto second=write("second.xy","0.71073\n20 3\n21 6\n22 9\n");
    const auto cwd=QDir::currentPath();
    require(window.loadDiffractionPatterns({first},false),"First GUI C++ pattern open");
    const auto initial=session.snapshot();
    for (int action : {-1, 0, std::numeric_limits<int>::max()}) {
        require(ProductWorkflowProbe::pointAction(window,action)==1,"Unsupported point action rejected");
        require(session.snapshot()==initial,"Unsupported point action preserves complete state and source handle");
    }
    require(initial->pattern.radiation.lines[0].wavelength==radiation::default_wavelength,"Canonical double default");
    require(initial->pattern.source_id==first.toStdString(),"Unicode source identity");
    require(initial->pattern.original_observed==std::vector<double>({2,4,8}) &&
            initial->pattern.observed==std::vector<double>({250,500,1000}),"Raw/current separation");
    require(initial->pattern.processing.intensity_scaled==true && initial->pattern.processing.background_subtracted==false &&
            initial->preprocessing.smoothing_applied==false,"Known fresh processing flags");
    auto dirty=*initial;
    dirty.pattern.background=SampledSignal{{1,1,1},SampleWindow{0}};
    dirty.pattern.calculated=SampledSignal{{2,2,2},SampleWindow{0}};
    dirty.pattern.background_subtracted_signal=SampledSignal{{249,499,999},SampleWindow{0}};
    dirty.pattern.weights=std::vector<double>{1,1,1};
    dirty.preprocessing.smoothing_applied=true;
    dirty.preprocessing.background_points=std::vector<preprocessing::BackgroundPoint>{{10,1},{12,1}};
    dirty.preprocessing.background_filters.push_back({10,12,2,1});
    dirty.preprocessing.peak_candidates=PeakSet{};
    dirty.preprocessing.peak_candidates->dataset={1};
    dirty.preprocessing.peak_candidates->effective_wavelength=radiation::default_wavelength;
    dirty.peaks.valid=true; dirty.peaks.tth={11}; dirty.peaks.d={8}; dirty.peaks.deltaD={.1};
    dirty.peaks.intensity={50}; dirty.peaks.intensityOrig={100}; dirty.peaks.fwhm={.1};
    CardType card; card.setId("previous"); dirty.candidates.append(card); dirty.phases.append(card);
    dirty.automaticDelta=.37; dirty.peakTolerance=.2;
    const auto seeded=session.commit(initial,std::move(dirty));
    if (!seeded) throw std::runtime_error("Seed previous state: "+seeded.error().field+": "+seeded.error().message);
    const auto old=session.snapshot();
    require(!session.openPattern(initial,initial->pattern) && session.snapshot()==old,"Stale open committed");
    const auto title=window.windowTitle();
    const std::vector<QByteArray> invalid={"", "10 1\n", "10 1\n10 2\n", "11 1\n10 2\n", "10 1\n11 NaN\n", "10 1\n11 Inf\n", "0\n10 1\n11 2\n", "10 1\n11 text\n"};
    for (std::size_t i=0;i<invalid.size();++i) {
        auto path=write(QString("bad%1.xy").arg(i),invalid[i]);
        require(!window.loadDiffractionPatterns({path},false),"Invalid GUI file accepted");
        require(session.snapshot()==old && window.windowTitle()==title,"Failed open published partial state/UI");
    }
    require(!window.loadDiffractionPatterns({files.path()+"/missing.xy"},false) && session.snapshot()==old,"Inaccessible file changed Session");
    QTimer cancel;
    QObject::connect(&cancel,&QTimer::timeout,[] {
        if(auto *dialog=qobject_cast<WaveDialog *>(QApplication::activeModalWidget())) dialog->reject();
    });
    cancel.start(10);
    require(!window.loadDiffractionPatterns({first}) && session.snapshot()==old,"Cancelled radiation changed Session");
    cancel.stop();
    // Validate that replacement uses the new file and preserves its authoritative radiation.
    // Former Native-only poison setup is frozen in the historical probe.
    require(window.loadDiffractionPatterns({second},true),"Explicit file wavelength should bypass dialog");
    const auto replaced=session.snapshot();
    require(replaced->revision==old->revision+1,"Replacement is not a single transaction");
    require(replaced->pattern.original_observed==std::vector<double>({3,6,9}),"Poisoned native state became authoritative");
    require(replaced->pattern.radiation.lines[0].wavelength.value!=radiation::default_wavelength_angstrom,"File wavelength overwritten");
    require(!replaced->pattern.background && !replaced->pattern.calculated && !replaced->pattern.background_subtracted_signal &&
            !replaced->pattern.weights && !replaced->preprocessing.peak_candidates && !replaced->preprocessing.candidate_failure &&
            !replaced->preprocessing.background_points && replaced->preprocessing.background_filters.empty() &&
            replaced->preprocessing.smoothing_applied==false && !replaced->peaks.valid && replaced->peaks.tth.empty() &&
            replaced->candidates.empty() && replaced->phases.empty() && !replaced->peakTolerance && replaced->automaticDelta==.5,
            "Replacement retained previous preprocessing/search/residual state");
    const auto saved=files.path()+"/saved.qxp";
    ProductWorkflowProbe::save(window,saved);
    require(window.loadDiffractionPatterns({first},false),"Replace before project restore");
    ProductWorkflowProbe::load(window,saved);
    const auto restored=session.snapshot();
    require(restored->pattern.corrected_two_theta==replaced->pattern.corrected_two_theta &&
            restored->pattern.intensity_scale==replaced->pattern.intensity_scale &&
            restored->pattern.radiation.kind==replaced->pattern.radiation.kind &&
            restored->pattern.observed==replaced->pattern.observed && restored->pattern.original_observed==replaced->pattern.original_observed &&
            restored->pattern.radiation.lines[0].wavelength==replaced->pattern.radiation.lines[0].wavelength,"Project failed to preserve pattern/radiation");
    auto failedPool=*restored;
    failedPool.preprocessing.candidate_failure="nonfinite_candidate_measurement";
    require(bool(session.commit(restored,std::move(failedPool))),"Seed unavailable candidate pool");
    require(window.loadDiffractionPatterns({first,second},false),"Multiple input transaction");
    require(!session.snapshot()->preprocessing.candidate_failure && session.snapshot()->pattern.source_id==first.toStdString() && session.snapshot()->comparisonPatterns.size()==1 &&
            session.snapshot()->comparisonPatterns[0].observed==std::vector<double>({3,6,9}),"First pattern/comparison contract");
    auto multi=session.snapshot();
    require(!window.loadDiffractionPatterns({second,files.path()+"/missing.xy"},false) && session.snapshot()==multi,"Batch open partially published");
    ProductWorkflowProbe::fresh(window);
    require(session.snapshot()->pattern.two_theta.empty() && session.snapshot()->comparisonPatterns.empty() &&
            session.snapshot()->candidates.empty() && session.snapshot()->phases.empty(),"New retained derived state");
    require(session.snapshot()->pattern.observed.empty() && session.snapshot()->peaks.tth.empty(),
            "New did not clear current pattern and peak workspace");
    require(QDir::currentPath()==cwd,"Reader changed working directory");
    require(QDir(files.path()).entryList({"*.out","fort.*"},QDir::Files).empty(),"Reader wrote historical side effects");
    int defaultNativeReads=-1;
#ifdef QUALPF_PATTERN_LINK_GUARD
    defaultNativeReads=qualpf_test_native_pattern_reads();
    require(defaultNativeReads==0,"Default lifecycle called the Fortran file reader");
#endif
    const auto cif=write("powder.cif","data_test\n_diffrn_radiation_wavelength 1.2\nloop_\n_pd_meas_2theta_scan\n_pd_meas_counts_total\n10 2\n11 4\n12 8\n");
    auto beforeCif=session.snapshot();
    require(!window.loadDiffractionPatterns({cif},false) && session.snapshot()==beforeCif,"Default reader silently fell back to CIF");
    require(window.loadDiffractionPatterns({cif},false,true),"Explicit retained CIF importer failed");
    require(session.snapshot()->revision==beforeCif->revision+1 && session.snapshot()->pattern.original_observed==std::vector<double>({2,4,8}),"CIF draft was not a single validated transaction");
#ifdef QUALPF_PATTERN_LINK_GUARD
    require(qualpf_test_native_pattern_reads()==0,"C++ powder import reached the native reader");
#endif
    int radiationChoice=0;
    QTimer radiationDriver;
    QObject::connect(&radiationDriver,&QTimer::timeout,[&] {
        if (auto *dialog=qobject_cast<WaveDialog *>(QApplication::activeModalWidget())) {
            auto *widget=dialog->findChild<WavelengthsWidget *>();
            auto *combo=widget->findChild<QComboBox *>("radiationComboBox");
            require(combo!=nullptr,"Radiation selector unavailable");
            combo->setCurrentIndex(radiationChoice);
            widget->setComboAsUserDefined(.81234567);
            dialog->accept();
        }
    });
    radiationDriver.start(10);
    for (radiationChoice=0;radiationChoice<3;++radiationChoice) {
        require(window.loadDiffractionPatterns({first}),"GUI radiation import failed");
        auto state=session.snapshot();
        require(state->pattern.radiation.lines[0].wavelength.value==.81234567,"Mirror quantized explicit GUI wavelength");
        require(state->pattern.radiation.kind==(radiationChoice==2 ? RadiationKind::neutron : RadiationKind::x_ray) &&
                state->projectMetadata["synchrotron"].toBool()==(radiationChoice==1),"Radiation kind/synchrotron lost");
        ProductWorkflowProbe::save(window,saved);
        ProductWorkflowProbe::load(window,saved);
        require(session.snapshot()->pattern.radiation.lines[0].wavelength.value==.81234567 &&
                session.snapshot()->pattern.radiation.kind==state->pattern.radiation.kind &&
                session.snapshot()->projectMetadata["synchrotron"]==state->projectMetadata["synchrotron"],"Saved radiation metadata changed");
    }
    radiationDriver.stop();
    QSettings cliSettings(files.path()+"/IC/qualpf.ini",QSettings::IniFormat);
    cliSettings.setValue("databases/defaultDir",files.path()); cliSettings.sync();
    QString executable=QString::fromUtf8(QUALPF_PRODUCT_EXECUTABLE);
#ifdef Q_OS_WIN
    executable+=".exe";
#endif
    for (bool bad : {false,true}) {
        QProcess cli;
        QStringList arguments{"--nogui","--auto","--settings-dir",files.path()};
        if (bad) arguments<<"--search";
        arguments<<(bad ? files.path()+"/missing.xy" : second);
        cli.start(executable,arguments);
        require(cli.waitForFinished(10000) && cli.exitStatus()==QProcess::NormalExit,"Actual CLI did not finish");
        require((cli.exitCode()!=0)==bad,"Actual CLI file status");
        const auto messages=cli.readAllStandardOutput()+cli.readAllStandardError();
        if (bad) require(messages.contains("Cannot open pattern") && !messages.contains("Searching database"),"Failed CLI fell through to database search");
    }
    QFile report(output); require(report.open(QIODevice::WriteOnly),"Lifecycle report");
    report.write(QJsonDocument(QJsonObject{{"passed",true},{"invalid_atomic_cases",int(invalid.size()+1)},
        {"cancel_atomic",true},{"replacement_invalidates_all",true},{"project_restore",true},{"replacement_uses_current_file",true},
        {"unicode_path",true},{"canonical_default",true},{"file_wavelength_preserved",true},{"multi_file_atomic",true},
        {"new_clears_session",true},{"no_out_or_cwd_side_effect",true},
        {"default_native_pattern_reads",defaultNativeReads},{"explicit_legacy_cif",true},
        {"radiation_gui_project_variants",3},{"actual_cli_valid_and_failed",true}}).toJson());
}

void close(double actual, double expected, double absolute, double relative, const char *field) {
    if (!std::isfinite(actual) || !std::isfinite(expected) ||
        std::abs(actual - expected) > absolute + relative * std::abs(expected)) {
        std::cerr << field << ": actual=" << actual << " reference=" << expected << '\n';
        throw std::runtime_error(field);
    }
}

qualpf::SearchParameters parameters() {
    qualpf::SearchParameters p;
    p.delta_two_theta.value = SearchOptionsDialog::savedDelta2theta(AppState::workspace().session().snapshot(), AppState::context().settings());
    p.weight_position = SearchOptionsDialog::savedWeight2thetaD(AppState::workspace().session().snapshot(), AppState::context().settings());
    p.weight_intensity = SearchOptionsDialog::savedWeightIntensity(AppState::workspace().session().snapshot(), AppState::context().settings());
    p.weight_phases = SearchOptionsDialog::savedWeightPhases(AppState::workspace().session().snapshot(), AppState::context().settings());
    p.minimum_score = SearchOptionsDialog::savedMinFom(AppState::workspace().session().snapshot(), AppState::context().settings());
    p.maximum_entries = SearchOptionsDialog::savedMaxEntries(AppState::workspace().session().snapshot(), AppState::context().settings());
    return p;
}

qualpf::PeakSet project(const ExperimentalPeaks &ep) {
    std::vector<qualpf::PeakId> ids;
    for (qsizetype i = 0; i < ep.tth.size(); ++i)
        ids.push_back({std::uint64_t(i + 1)});
    auto result = qualpf::gui::copy_experimental_peaks(ep, {1}, ids);
    require(bool(result), "Qt peak projection failed");
    return std::move(result).value();
}

qualpf::ReferenceCard project(const CardType &card) {
    std::vector<qualpf::ReflectionId> ids;
    for (qsizetype i = 0; i < card.getTth().size(); ++i)
        ids.push_back({std::uint64_t(i + 1)});
    auto result = qualpf::gui::copy_reference_card(card, "cod_inorg", ids, std::nullopt);
    require(bool(result), "Qt card projection failed");
    return std::move(result).value();
}

qualpf::Pattern pattern() {
    const auto snapshot = AppState::searchSession().snapshot();
    require(!snapshot->pattern.observed.empty(), "Missing working pattern");
    return snapshot->pattern;
}

QJsonArray scores(const QVector<CardType> &cards) {
    QJsonArray values;
    for (const auto &c : cards)
        values.append(QJsonObject{{"id", c.getId()},
                                  {"total", c.getFom()},
                                  {"position", c.getFomPeakPos()},
                                  {"intensity", c.getFomIntensity()},
                                  {"scale", c.getScale()}});
    return values;
}

void compareCards(QVector<CardType> a, QVector<CardType> b) {
    const auto byId = [](const CardType &x, const CardType &y) { return x.getId() < y.getId(); };
    std::sort(a.begin(), a.end(), byId);
    std::sort(b.begin(), b.end(), byId);
    if (scores(a) != scores(b)) {
        std::cerr << "Compared card counts: " << a.size() << " vs " << b.size() << "\n";
        for (qsizetype i = 0; i < std::min(a.size(), b.size()); ++i)
            if (scores({a[i]}) != scores({b[i]})) {
                std::cerr << QJsonDocument(scores({a[i], b[i]})).toJson().constData(); break;
            }
    }
    require(scores(a) == scores(b), "GUI/query candidate set or score differs");
}

void compareCpp(const qualpf::Pattern &working, const ExperimentalPeaks &ep, const QVector<CardType> &cards) {
    const auto peaks = project(ep);
    const auto p = parameters();
    qualpf::ExecutionContext context{{}, qualpf::NumericalPolicy::legacy_mixed_precision, "s7-real-db"};
    for (const auto &card : cards) {
        const auto c = project(card);
        auto output = qualpf::search::score_classic({working, peaks, c, p}, context);
        require(bool(output), "C++ scoring failure on retained real candidate");
        require(output.value().disposition == qualpf::search::ScoreDisposition::scored,
                "Undefined legacy candidate cannot count as equivalent");
        const auto &s = output.value().score;
        close(s.total, card.getFom(), 5e-7, 2e-6, "total");
        close(s.position, card.getFomPeakPos(), 5e-7, 2e-6, "position");
        close(s.intensity, card.getFomIntensity(), 5e-7, 2e-6, "intensity");
        close(s.scale, card.getScale(), 1e-7, 2e-6, "scale");
    }
}

QJsonArray compareRescore(const qualpf::Pattern &working, const ExperimentalPeaks &ep,
                          const QVector<CardType> &input, const QVector<CardType> &reference) {
    std::vector<qualpf::search::CandidateInput> candidates;
    for (const auto &card : input)
        candidates.push_back({project(card), std::nullopt, card.getScale()});
    qualpf::ExecutionContext context{{}, qualpf::NumericalPolicy::legacy_mixed_precision, "s7-real-rescore"};
    auto output = qualpf::search::evaluate_candidates(working, project(ep), candidates, parameters(),
                                                      qualpf::search::SearchPass::rescore, context);
    require(bool(output), "C++ residual batch failed");
    const auto &ranked = output.value().results.candidates;
    require(ranked.size() == std::size_t(reference.size()), "C++ residual candidate count differs");
    for (qsizetype i = 0; i < reference.size(); ++i) {
        require(ranked[i].card_id == reference[i].getId().toStdString(), "C++ residual order differs");
        const auto &score = ranked[i].outcome.value();
        close(score.total, reference[i].getFom(), 5e-7, 2e-6, "rescore total");
        close(score.position, reference[i].getFomPeakPos(), 5e-7, 2e-6, "rescore position");
        close(score.intensity, reference[i].getFomIntensity(), 5e-7, 2e-6, "rescore intensity");
        require(score.scale == reference[i].getScale(), "rescore previous scale changed");
    }
    // Count legacy-undefined dispositions as well as errors. ClassicScorer's
    // deterministic zero for no associations is not a normal equivalence pass.
    QJsonArray excluded;
    const auto peaks = project(ep);
    const auto p = parameters();
    for (const auto &candidate : candidates) {
        auto trace = qualpf::search::score_classic({working, peaks, candidate.card, p}, context);
        require(bool(trace), "C++ residual diagnostic error requires investigation");
        if (trace.value().disposition != qualpf::search::ScoreDisposition::scored)
            excluded.append(QJsonObject{
                {"card_id", QString::fromStdString(candidate.card.card_id)},
                {"reason", trace.value().disposition == qualpf::search::ScoreDisposition::no_associations
                               ? "legacy_no_associations_undefined_auxiliary_outputs"
                               : "legacy_no_reflections_in_range_unassigned_outputs"}});
    }
    return excluded;
}

void codecTests() {
    ExperimentalPeaks p;
    p.tth = {20, 40};
    p.d = {4, 2};
    p.deltaD = {.1, .1};
    p.fwhm = {.2, .3};
    p.intensityOrig = {100, 80};
    p.intensity = {50, 12.5};
    p.wave = 1.54056;
    p.valid = true;
    const auto bytes = QJsonDocument(qualpf::gui::encode_project_peaks(p)).toJson();
    const auto object = QJsonDocument::fromJson(bytes).object();
    QString error;
    auto v2 = qualpf::gui::decode_project_peaks(object, 2, error);
    require(v2 && v2->intensity == p.intensity && v2->intensityOrig == p.intensityOrig, "v2 residual lost");
    auto v1 = qualpf::gui::decode_project_peaks(object, 1, error);
    require(v1 && v1->intensity == p.intensityOrig, "v1 historical semantics changed");
    auto invalid = object;
    invalid.remove("current_intensity");
    require(!qualpf::gui::decode_project_peaks(invalid, 2, error), "missing residual accepted");
    invalid = object;
    invalid["fwhm"] = QJsonArray{1};
    require(!qualpf::gui::decode_project_peaks(invalid, 2, error), "short array accepted");
    invalid = object;
    invalid["current_intensity"] = QJsonArray{QJsonValue(), 1};
    require(!qualpf::gui::decode_project_peaks(invalid, 2, error), "null intensity accepted");
    require(!qualpf::gui::decode_project_peaks(object, 3, error), "future version accepted");
    {
        using namespace qualpf;
        using namespace qualpf::application; using namespace qualpf::gui;
        qualpf::application::Workspace sessionOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView session(sessionOwner.session());
        auto initial = session.snapshot();
        auto next = *initial;
        next.peaks = p;
        require(bool(session.commit(initial, next)), "Finite snapshot rejected");
        const auto base = session.snapshot();
        for (int column = 0; column < 7; ++column)
            for (double invalidValue :
                 {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
                auto bad = *base;
                QVector<double> *values[] = {&bad.peaks.tth,           &bad.peaks.d,
                                             &bad.peaks.deltaD,        &bad.peaks.intensity,
                                             &bad.peaks.intensityOrig, &bad.peaks.fwhm};
                if (column < 6)
                    (*values[column])[0] = invalidValue;
                else
                    bad.peaks.wave = invalidValue;
                require(!session.commit(base, bad) && session.snapshot() == base,
                        "Nonfinite peak field published");
            }
        auto bad = *base;
        bad.peaks = {};
        bad.peaks.intensity = {std::numeric_limits<double>::infinity()};
        require(!session.commit(base, bad) && session.snapshot() == base,
                "Invalid-flag peak payload bypassed validation");
        auto unknown = qualpf::infrastructure::decodePreprocessing({});
        require(bool(unknown) && !unknown.value().background && !unknown.value().peaks,
                "Old project invented configuration");
        PreprocessingConfiguration config;
        config.background = preprocessing::BackgroundOptions{};
        config.smoothing = preprocessing::SmoothingOptions{};
        config.peaks = preprocessing::PeakOptions{};
        auto encoded = qualpf::infrastructure::encodePreprocessing(config);
        auto restored = qualpf::infrastructure::decodePreprocessing(encoded);
        require(bool(restored) && qualpf::infrastructure::encodePreprocessing(restored.value()) == encoded,
                "Configuration round trip failed");
        auto broken = encoded;
        broken["version"] = 99;
        require(!qualpf::infrastructure::decodePreprocessing(broken), "Future configuration accepted");
        broken = encoded;
        auto smoothing = broken["smoothing"].toObject();
        smoothing["half_window"] = QJsonValue();
        broken["smoothing"] = smoothing;
        require(!qualpf::infrastructure::decodePreprocessing(broken), "Null preprocessing option accepted");
        auto areaPeaks = p;
        areaPeaks.integratedIntensity = QVector<double>{1.25, 2.5};
        auto area = qualpf::gui::decode_project_peaks(
            qualpf::gui::encode_project_peaks(areaPeaks), 2, error);
        require(area && area->integratedIntensity == areaPeaks.integratedIntensity,
                "Integrated peak intensity lost");
        for (double invalidValue :
             {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
            auto badArea = *base;
            badArea.peaks.integratedIntensity = QVector<double>{invalidValue, 1};
            require(!session.commit(base, badArea) && session.snapshot() == base, "Nonfinite area published");
        }
        auto emptyBad = *base;
        emptyBad.peaks = {};
        emptyBad.peaks.wave = std::numeric_limits<double>::quiet_NaN();
        require(!session.commit(base, emptyBad) && session.snapshot() == base,
                "Empty peak snapshot accepted nonfinite wavelength");
        PreprocessingEdit staleInteractive(session, true);
        auto newer = *session.snapshot();
        newer.peaks.intensity[0] = 25;
        require(bool(session.commit(session.snapshot(), newer)), "Cannot prepare concurrent change");
        require(!staleInteractive.commit(), "Stale interactive commit accepted");
        staleInteractive.cancel();
        require(!session.snapshot()->editing && session.snapshot()->peaks.intensity[0] == 25,
                "Stale cancel rolled back newer state");
    }
    p.tth.clear();
    p.d.clear();
    p.deltaD.clear();
    p.fwhm.clear();
    p.intensity.clear();
    p.intensityOrig.clear();
    v2 = qualpf::gui::decode_project_peaks(qualpf::gui::encode_project_peaks(p), 2,
                                                      error);
    require(v2 && v2->valid && v2->intensity.isEmpty(), "exhausted residual lost");
    std::cout << "PASS: project codec v1/v2, current/original, exhausted, malformed, version\n";
}

void archivalProject(MainWindow &window, const QString &source, const QString &output) {
    QFile input(source);
    require(input.open(QIODevice::ReadOnly), "Cannot read supplied project");
    const auto root = QJsonDocument::fromJson(input.readAll()).object();
    QString error;
    const auto expected = qualpf::gui::decode_project_peaks(root["peaks"].toObject(),
                                                                       root["version"].toInt(), error);
    require(bool(expected), "Supplied project peak contract rejected");
    ProductWorkflowProbe::load(window, source);
    const auto restored = qualpf::gui::encode_project_peaks(AppState::peaks());
    require(restored == qualpf::gui::encode_project_peaks(*expected),
            "Supplied project restored different peak columns");
    {
        const auto owned=AppState::searchSession().snapshot();
        require(!owned->preprocessing.peak_candidates && !owned->pattern.processing.intensity_scaled,
                "Historical project invented preprocessing history");
        const auto backend=qgetenv("QUALPF_PREPROCESSING_BACKEND");
        if (!qualpf::gui::cppPreprocessingProduct()) qputenv("QUALPF_PREPROCESSING_BACKEND","cpp");
        PeakSearchDialog dialog(&window);dialog.setOptions();
        const auto before=AppState::searchSession().snapshot();
        require(dialog.findChild<QSpinBox*>("numPeaksSpinBox")->value()==owned->peaks.tth.size(),
                "Historical snapshot restore displayed a fabricated peak count");
        dialog.findChild<QDoubleSpinBox*>("intensitySpinBox")->setValue(20);
        require(dialog.property("preprocessingReason")=="candidate_pool_unavailable" &&
                AppState::searchSession().snapshot()==before,"Historical edit silently invented candidates");
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::RestoreDefaults)->click();
        require(dialog.property("preprocessingReason")=="subtraction_state_unknown" &&
                !dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->isEnabled(),
                "Historical redetection guessed subtraction history");
        require(QMetaObject::invokeMethod(&dialog,"reject",Qt::DirectConnection),"Historical Cancel unavailable");
        require(qualpf::gui::encode_project_peaks(AppState::peaks())==restored,
                "Historical rejected edit changed the selected snapshot");
        if (qgetenv("QUALPF_PREPROCESSING_BACKEND") != backend) {
            if(backend.isNull()) qunsetenv("QUALPF_PREPROCESSING_BACKEND");else qputenv("QUALPF_PREPROCESSING_BACKEND",backend);
        }
    }
    auto *results = window.findChild<DbResultsWidget *>();
    auto *quant = window.findChild<QuantWidget *>();
    require(results && quant, "Project widgets missing");
    const auto savedPhases = root["phases"].toArray();
    const auto phases = quant->phases();
    require(phases.size() == savedPhases.size(), "Supplied project lost accepted phases");
    for (qsizetype i = 0; i < phases.size(); ++i) {
        const auto saved = savedPhases[i].toObject();
        require(phases[i].getId() == saved["id"].toString() &&
                    phases[i].getScale() == saved["scale"].toDouble() &&
                    phases[i].getFom() == saved["fom"].toDouble() &&
                    phases[i].getFomPeakPos() == saved["fom_peakpos"].toDouble() &&
                    phases[i].getFomIntensity() == saved["fom_intensity"].toDouble(),
                "Supplied accepted phase ID/score/previous scale changed");
        if (saved.contains("weight_percent"))
            close(quant->quantPercentages()[i], saved["weight_percent"].toDouble(), 1e-12, 1e-12,
                  "supplied phase percentage");
        require(AppState::db().queryCard(phases[i].getId()).valid,
                "Supplied accepted card missing from actual database");
    }
    const auto working = pattern();
    const auto dd = root["diffraction_data"].toObject();
    require(working.observed.size() == std::size_t(dd["y"].toArray().size()), "Supplied pattern length");
    for (qsizetype i = 0; i < dd["y"].toArray().size(); ++i) {
        require(working.observed[i] == dd["y"].toArray()[i].toDouble() &&
                    working.two_theta[i].value == dd["x"].toArray()[i].toDouble(),
                "Supplied measured pattern changed on load");
    }
    const auto query = [&] {
        auto builder = buildSearchMatchQuery(AppState::context(), AppState::peaks());
        builder.enableDeleted(SearchOptionsDialog::savedCheckDeleted(AppState::workspace().session().snapshot(), AppState::context().settings()));
        QVector<CardType> cards;
        AppState::searches().makeQueryStrongest(builder, cards);
        require(!AppState::searches().isCanceled() && AppState::searches().searchError().isEmpty(),
                "Supplied project continuation SQL failed");
        return cards;
    };
    const auto before = query();
    compareCpp(working, AppState::peaks(), before);
    results->setResults(before);
    const QString upgraded = output + "-upgraded.qxp";
    ProductWorkflowProbe::save(window, upgraded);
    QFile upgradedFile(upgraded);
    require(upgradedFile.open(QIODevice::ReadOnly), "Supplied project upgrade not saved");
    const auto upgradedRoot = QJsonDocument::fromJson(upgradedFile.readAll()).object();
    require(upgradedRoot["version"].toInt() == 2, "Supplied project not upgraded to v2");
    require(!AppState::searchSession().snapshot()->pattern.processing.background_subtracted.has_value() &&
            !upgradedRoot["diffraction_data"].toObject().contains("back_subtracted") &&
            !upgradedRoot["diffraction_data"].toObject()["residual_current_known"].toBool(),
            "Historical project fabricated missing residual/background knowledge");
    ProductWorkflowProbe::load(window, upgraded);
    require(qualpf::gui::encode_project_peaks(AppState::peaks()) == restored &&
                scores(quant->phases()) == scores(phases),
            "Supplied project upgrade lost saved state");
    require(scores(query()) == scores(before), "Supplied project query differs after v2 reload");
    require(pattern().observed == working.observed, "Supplied project upgrade changed working spectrum");

    // These files have accepted cards but no saved candidate list. Explicitly
    // re-evaluate those real cards as a separate diagnostic batch; do not call
    // them candidates returned by the continuation SQL query.
    results->setResults(phases);
    ProductWorkflowProbe::rescore(window);
    const auto phaseRescore = results->allCards();
    const auto phaseRescoreUndefined = compareRescore(working, AppState::peaks(), phases, phaseRescore);
    ProductWorkflowProbe::load(window, upgraded);

    bool residualExercised = false;
    QString acceptedId;
    for (const auto &card : before) {
        if (std::any_of(phases.begin(), phases.end(),
                        [&](const auto &p) { return p.getId() == card.getId(); }))
            continue;
        acceptedId = card.getId();
        quant->addPhase(card);
        ProductWorkflowProbe::residual(window, card);
        const auto continued = qualpf::gui::encode_project_peaks(AppState::peaks());
        const auto continuedCards = scores(results->allCards());
        const auto continuedPhases = scores(quant->phases());
        ProductWorkflowProbe::save(window, output + "-continued.qxp");
        ProductWorkflowProbe::load(window, output + "-continued.qxp");
        require(qualpf::gui::encode_project_peaks(AppState::peaks()) == continued &&
                    scores(results->allCards()) == continuedCards &&
                    scores(quant->phases()) == continuedPhases,
                "Continued supplied residual did not persist");
        ProductWorkflowProbe::load(window, upgraded);
        quant->addPhase(card);
        ProductWorkflowProbe::residual(window, card);
        require(qualpf::gui::encode_project_peaks(AppState::peaks()) == continued &&
                    scores(results->allCards()) == continuedCards &&
                    scores(quant->phases()) == continuedPhases,
                "Continued supplied residual differs across reload");
        residualExercised = true;
        break;
    }
    QJsonObject report{
        {"source", source},
        {"source_version", root["version"]},
        {"restored_peaks", expected->tth.size()},
        {"accepted_phases", phases.size()},
        {"supplied_phases", scores(phases)},
        {"saved_phase_rescore", scores(phaseRescore)},
        {"saved_phase_rescore_legacy_undefined", phaseRescoreUndefined},
        {"continued_query", scores(before)},
        {"continued_residual_exercised", residualExercised},
        {"continued_card_id", acceptedId},
        {"missing_current_intensity", !root["peaks"].toObject().contains("current_intensity")},
        {"missing_has_back", !dd.contains("has_back")},
        {"missing_back_subtracted", !dd.contains("back_subtracted")},
        {"upgraded_background_payload_present", upgradedRoot["diffraction_data"].toObject().contains("yb")},
        {"pre_save_residual_reconstructed", false},
        {"product_backend", AppState::searchSession().snapshot()->backend == qualpf::application::SearchBackend::cpp ? "cpp" : "legacy_fallback"},
        {"status", "passed_supplied_project_scoped"}};
    QFile reportFile(output + ".json");
    require(reportFile.open(QIODevice::WriteOnly), "Cannot write supplied project report");
    reportFile.write(QJsonDocument(report).toJson());
    std::cout << "PASS: supplied project load/accepted phases/SQL/v2 upgrade/continuation; residual="
              << residualExercised << '\n';
}
} // namespace

#include "current_preprocessing_product_checks.h"
#include "preprocessing_closure_checks.h"
#include "preprocessing_format_checks.h"
#include "preprocessing_signoff_checks.h"
#include "kalpha2_workflow.h"
#include "manual_acceptance_checks.h"

void cppPreprocessingDialogs(MainWindow &window, const QString &output,
                             qualpf::gui::SessionViewHandle raw) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    auto &session = AppState::searchSession();
    require(bool(session.commit(session.snapshot(), *raw)), "Cannot initialize C++ GUI raw input");
    window.refreshPatternPresentation();
    auto cancel = [](QDialog &dialog) {
        require(QMetaObject::invokeMethod(&dialog, "reject", Qt::DirectConnection), "Missing Cancel");
    };
    auto apply = [](QDialog &dialog) {
        dialog.show();
        QApplication::processEvents();
        auto *button = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
        if (!button->isEnabled())
            throw std::runtime_error("C++ GUI Apply disabled: " +
                                     dialog.property("preprocessingReason").toString().toStdString());
        button->setFocus();
        button->click();
    };
    {
        const auto original = session.snapshot();
        BackgroundDialog dialog(&window);
        dialog.setBackground();
        require(dialog.property("preprocessingBackend") == "cpp", "Background did not use C++");
        const auto before = session.snapshot();
        auto *type = dialog.findChild<QComboBox *>("typeComboBox");
        type->setCurrentIndex(6);
        type->setCurrentIndex(3);
        require(session.snapshot() == before, "Background preview published");
        cancel(dialog);
        require(session.snapshot()->pattern.observed == original->pattern.observed &&
                    qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) ==
                        qualpf::infrastructure::encodePreprocessing(original->preprocessing),
                "Background cancel changed input/configuration");
    }
    {
        BackgroundDialog dialog(&window);
        dialog.setBackground();
        const auto before = session.snapshot();
        dialog.findChild<QComboBox *>("typeComboBox")->setCurrentIndex(3);
        apply(dialog);
        require(!session.snapshot()->editing && session.snapshot()->revision == before->revision + 1 &&
                    session.snapshot()->preprocessing.background->method ==
                        preprocessing::BackgroundMethod::spline,
                "C++ background Apply failed");
    }
    {
        const auto original = session.snapshot();
        SmoothingDialog dialog(&window);
        dialog.setSmoothing();
        require(dialog.property("preprocessingBackend") == "cpp", "Smoothing did not use C++");
        const auto before = session.snapshot();
        auto *points = dialog.findChild<QSpinBox *>("pointsSpinBox");
        points->setValue(7);
        points->setValue(3);
        require(session.snapshot() == before, "Smoothing preview published");
        cancel(dialog);
        require(session.snapshot()->pattern.observed == original->pattern.observed,
                "Smoothing cancel changed input");
    }
    {
        SmoothingDialog dialog(&window);
        dialog.setSmoothing();
        const auto before = session.snapshot();
        apply(dialog);
        require(!session.snapshot()->editing && session.snapshot()->revision == before->revision + 1 &&
                    session.snapshot()->preprocessing.smoothing.has_value(),
                "C++ smoothing Apply failed");
    }
    bool rejected = false;
    {
        PeakSearchDialog dialog(&window);
        dialog.setOptions();
        require(dialog.property("preprocessingBackend") == "cpp", "Peak search did not use C++");
        const auto before = session.snapshot();
        if (output.contains("CPD2")) {
            require(dialog.property("preprocessingReason") == "coincident_baseline_endpoints",
                    "CPD2 GUI reason differs");
            auto *button = dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
            require(!button->isEnabled(), "Undefined preview allowed Apply");
            button->click();
            require(session.snapshot() == before && !session.snapshot()->peaks.valid,
                    "Undefined GUI preview published");
            rejected = true;
            cancel(dialog);
        } else {
            dialog.findChild<QDoubleSpinBox *>("intensitySpinBox")->setValue(4);
            dialog.findChild<QDoubleSpinBox *>("intensitySpinBox")->setValue(1.5);
            require(session.snapshot() == before, "Peak preview published");
            apply(dialog);
            require(!session.snapshot()->editing && session.snapshot()->revision == before->revision + 1 &&
                        session.snapshot()->peaks.valid,
                    "C++ peak Apply failed");
        }
    }
    QJsonArray cardsJson;
    if (!rejected) {
        const auto applied = session.snapshot();
        auto builder = buildSearchMatchQuery(AppState::context(), applied->peaks);
        QVector<CardType> cards;
        AppState::searches().makeQueryStrongest(builder, cards);
        require(AppState::searches().searchError().isEmpty(), "C++ GUI workflow search failed");
        compareCpp(applied->pattern, applied->peaks, cards);
        cardsJson = scores(cards);
        ProductWorkflowProbe::save(window, output + "-cpp-gui.qxp");
        QSettings().setValue("Preprocessing/unused", 999);
        ProductWorkflowProbe::load(window, output + "-cpp-gui.qxp");
        require(qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) ==
                        qualpf::infrastructure::encodePreprocessing(applied->preprocessing) &&
                    qualpf::gui::encode_project_peaks(session.snapshot()->peaks) ==
                        qualpf::gui::encode_project_peaks(applied->peaks),
                "C++ GUI project state changed");
        require(session.snapshot()->pattern.observed == applied->pattern.observed &&
                    session.snapshot()->pattern.original_observed == applied->pattern.original_observed &&
                    session.snapshot()->pattern.processing.intensity_scaled ==
                        applied->pattern.processing.intensity_scaled &&
                    session.snapshot()->pattern.background->values == applied->pattern.background->values &&
                    session.snapshot()->pattern.background->window->first ==
                        applied->pattern.background->window->first &&
                    session.snapshot()->automaticDelta == applied->automaticDelta &&
                    session.snapshot()->peakTolerance == applied->peakTolerance,
                "C++ processing state round trip failed");
        QFile validFile(output + "-cpp-gui.qxp");
        require(validFile.open(QIODevice::ReadOnly), "Cannot read saved project");
        auto malformed = QJsonDocument::fromJson(validFile.readAll()).object();
        malformed["preprocessing"] = QJsonObject{{"version", 99}};
        QFile malformedFile(output + "-invalid-config.qxp");
        require(malformedFile.open(QIODevice::WriteOnly), "Cannot save invalid project fixture");
        malformedFile.write(QJsonDocument(malformed).toJson());
        malformedFile.close();
        const auto beforeInvalid = session.snapshot();
        ProductWorkflowProbe::load(window, malformedFile.fileName());
        require(session.snapshot() == beforeInvalid, "Malformed project configuration published");
        SmoothingDialog reopened(&window);
        reopened.setSmoothing();
        require(reopened.findChild<QSpinBox *>("pointsSpinBox")->value() ==
                    applied->preprocessing.smoothing->half_window,
                "Loaded smoothing settings not restored");
        cancel(reopened);
    }
    QFile report(output + "-cpp-gui.json");
    require(report.open(QIODevice::WriteOnly), "Cannot save C++ GUI evidence");
    report.write(
        QJsonDocument(
            QJsonObject{{"backend", "cpp"},
                        {"dialogs_preview_apply_cancel", true},
                        {"expected_atomic_rejection", rejected},
                        {"searchmatch", cardsJson},
                        {"configuration", qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing)},
                        {"peaks", qualpf::gui::encode_project_peaks(session.snapshot()->peaks)}})
            .toJson());
    std::cout << "PASS: real C++ dialogs; atomic rejection=" << rejected << "\n";
}

void preprocessingWorkflow(MainWindow& window,const QString& output,
                           qualpf::gui::SessionViewHandle unprocessed) {
    using namespace qualpf;
    using namespace qualpf::application; using namespace qualpf::gui;
    if (qEnvironmentVariable("QUALPF_CPP_PREPROCESSING_DIALOGS") == "1") {
        cppPreprocessingDialogs(window, output, unprocessed);
        return;
    }
    auto& session=AppState::searchSession();
    const auto original=session.snapshot();
    const auto originalPeaks=qualpf::gui::encode_project_peaks(original->peaks);
    const QString saved=output+"-preprocessing-source.qxp";
    ProductWorkflowProbe::save(window,saved);
    auto cancel=[](QObject& dialog) { require(QMetaObject::invokeMethod(&dialog,"reject",Qt::DirectConnection),"Cancel not connected"); };
    auto apply=[](QDialog& dialog) {
        dialog.show(); QApplication::processEvents();
        auto* buttons=dialog.findChild<QDialogButtonBox*>(); require(buttons!=nullptr,"Missing dialog buttons");
        if (!buttons->button(QDialogButtonBox::Ok)->isEnabled())
            throw std::runtime_error("Apply disabled: " +
                                     dialog.property("preprocessingReason").toString().toStdString());
        buttons->button(QDialogButtonBox::Ok)->setFocus();
        buttons->button(QDialogButtonBox::Ok)->click();
    };
    {
        SmoothingDialog dialog(&window); dialog.setSmoothing();
        const auto before=session.snapshot();
        auto* method=dialog.findChild<QComboBox*>("methodComboBox");
        auto* points=dialog.findChild<QSpinBox*>("pointsSpinBox");
        require(method&&points,"Missing smoothing controls");
        method->setCurrentIndex(1); points->setValue(7); points->setValue(3);
        require(session.snapshot()==before,"Smoothing preview published authoritative state");
        cancel(dialog);
        require(session.snapshot()->pattern.observed==original->pattern.observed&&
                qualpf::gui::encode_project_peaks(session.snapshot()->peaks)==originalPeaks,"Smoothing cancel changed data");
    }
    {
        BackgroundDialog dialog(&window); dialog.setBackground();
        const auto before=session.snapshot();
        auto* type=dialog.findChild<QComboBox*>("typeComboBox"); require(type!=nullptr,"Missing background control");
        type->setCurrentIndex(6); type->setCurrentIndex(3);
        require(session.snapshot()==before,"Background preview published authoritative state");
        cancel(dialog);
        require(session.snapshot()->pattern.observed==original->pattern.observed&&
                qualpf::gui::encode_project_peaks(session.snapshot()->peaks)==originalPeaks,"Background cancel changed data");
        require(session.snapshot()->pattern.background.has_value()==original->pattern.background.has_value()&&
                (!original->pattern.background||session.snapshot()->pattern.background->values==original->pattern.background->values),
                "Background cancel changed authoritative background");
    }
    {
        PeakSearchDialog dialog(&window); dialog.setOptions();
        const auto before=session.snapshot();
        auto* threshold=dialog.findChild<QDoubleSpinBox*>("intensitySpinBox"); require(threshold!=nullptr,"Missing peak threshold control");
        threshold->setValue(20); threshold->setValue(40);
        require(session.snapshot()==before,"Peak preview published authoritative state");
        cancel(dialog);
        require(qualpf::gui::encode_project_peaks(session.snapshot()->peaks)==originalPeaks,"Peak cancel lost snapshot");
    }
    {
        PeakSearchDialog dialog(&window); dialog.setOptions();
        const auto before=session.snapshot();
        dialog.findChild<QDoubleSpinBox*>("intensitySpinBox")->setValue(20);
        require(session.snapshot()==before,"Peak preview changed Session");
        apply(dialog);
        require(!session.snapshot()->editing&&session.snapshot()->revision==before->revision+1,"Apply did not publish once");
        const auto applied=session.snapshot();
        require(applied->peaks.valid&&applied->peaks.tth.size()<original->peaks.tth.size(),"Peak apply did not publish filtered peaks");
        ProductWorkflowProbe::save(window,output+"-preprocessing-applied.qxp");
        ProductWorkflowProbe::load(window,output+"-preprocessing-applied.qxp");
        require(qualpf::gui::encode_project_peaks(session.snapshot()->peaks)==qualpf::gui::encode_project_peaks(applied->peaks)&&
                session.snapshot()->automaticDelta==applied->automaticDelta&&session.snapshot()->peakTolerance==applied->peakTolerance,
                "Applied peak/FWHM/tolerance project round trip failed");
    }
    ProductWorkflowProbe::load(window,saved);
    {
        BackgroundDialog dialog(&window); dialog.setBackground();
        const auto before=session.snapshot();
        dialog.findChild<QComboBox*>("typeComboBox")->setCurrentIndex(3);
        require(session.snapshot()==before,"Background preview changed Session");
        apply(dialog);
        const auto applied=session.snapshot();
        require(!applied->editing&&applied->revision==before->revision+1,"Background apply did not publish once");
        require(applied->pattern.background&&before->pattern.background&&
                applied->pattern.background->values!=before->pattern.background->values,"Background apply missing");
        require(!applied->peaks.valid,"Background retained stale valid peaks");
    }
    ProductWorkflowProbe::load(window,saved);
    {
        SmoothingDialog dialog(&window); dialog.setSmoothing();
        const auto before=session.snapshot();
        dialog.findChild<QComboBox*>("methodComboBox")->setCurrentIndex(1);
        dialog.findChild<QSpinBox*>("pointsSpinBox")->setValue(4);
        require(session.snapshot()==before,"Smoothing preview changed Session");
        apply(dialog);
        require(!session.snapshot()->editing&&session.snapshot()->pattern.observed!=before->pattern.observed,"Smoothing apply missing");
        require(!session.snapshot()->peaks.valid,"Smoothing retained stale valid peaks");
        require(ensurePeaksFound(AppState::context())>0&&session.snapshot()->peaks.valid,"Search did not regenerate invalidated peaks");
    }
    ProductWorkflowProbe::load(window,saved);
    // Isolated C++ prepared result uses the same product owner, without enabling it in GUI.
    qualpf::application::Workspace isolatedOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView isolated(isolatedOwner.session());
    require(bool(isolated.commit(isolated.snapshot(),*original)),"Cannot prepare isolated owner");
    const auto base=isolated.snapshot();
    PreprocessingRequest request;
    request.wavelength=WavelengthAngstrom{original->peaks.wave}; request.tolerance=AngularWidthDegrees{.15};
    request.smoothing=preprocessing::SmoothingOptions{preprocessing::SmoothingMethod::averaging,4,5,0};
    {
        PreprocessingEdit edit(isolated);
        require(bool(edit.preview(request))&&isolated.snapshot()==base,"C++ preview changed owner");
        request.smoothing->half_window=8;
        require(bool(edit.preview(request))&&isolated.snapshot()==base,"Parameter modification changed owner");
        request.smoothing->half_window=-1;
        require(!edit.preview(request)&&!edit.preview()&&!edit.commit(),"Failed parameter request retained old preview");
        edit.cancel(); require(isolated.snapshot()==base,"C++ cancel changed owner");
    }
    request.smoothing->half_window=4;
    PreprocessingEdit stale(isolated),edit(isolated);
    require(bool(stale.preview(request))&&bool(edit.preview(request)),"C++ preview failed");
    require(bool(edit.commit())&&!edit.commit()&&!stale.commit(),"Repeated or stale commit accepted");
    require(!isolated.snapshot()->peaks.valid&&isolated.snapshot()->candidates.empty()&&isolated.snapshot()->phases.empty(),"Processing did not invalidate derived state");
    require(base->pattern.observed==original->pattern.observed,"Old source snapshot mutated");
    {
        request.smoothing.reset(); request.peaks=preprocessing::PeakOptions{};
        const auto before=isolated.snapshot();
        PreprocessingEdit peaksEdit(isolated);
        auto prepared=peaksEdit.preview(request);
        // These exact three subtracted/averaged inputs are archived in phase 9:
        // Fortran emits nonfinite local-baseline candidates. Keep them as an
        // explicit rejection contract; never filter the undefined candidates.
        require(!prepared && prepared.error().code == ErrorCode::undefined_preprocessing &&
                    prepared.error().reason == "coincident_baseline_endpoints",
                "Registered undefined pipeline changed");
        require(!peaksEdit.preview()&&!peaksEdit.commit()&&isolated.snapshot()==before,
                "Undefined peak preview published partial state");
    }
    // A separate defined chain on the original measured counts (background
    // estimation, default SG, automatic range, complete snapshot). This does
    // not replace or count the undefined subtraction sequence as equivalence.
    require(unprocessed && unprocessed->pattern.processing.background_subtracted==false,
            "Normal workflow requires the imported, unsubtracted spectrum");
    qualpf::application::Workspace normalOwner(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy" ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp); SessionView normal(normalOwner.session());
    require(bool(normal.commit(normal.snapshot(),*unprocessed)),"Cannot prepare normal owner");
    request.smoothing=preprocessing::SmoothingOptions{};
    request.background=preprocessing::BackgroundOptions{};
    request.background->method=preprocessing::BackgroundMethod::spline;
    request.peaks->search_range=-1;
    bool rejected = false;
    QJsonArray workflowCards;
    {
        const auto before=normal.snapshot();
        PreprocessingEdit peaksEdit(normal);
        auto prepared=peaksEdit.preview(request);
        if (!prepared) {
            require(output.contains("CPD2") && prepared.error().code == ErrorCode::undefined_preprocessing &&
                        prepared.error().reason == "coincident_baseline_endpoints",
                    "Unexpected workflow rejection");
            require(!peaksEdit.preview() && !peaksEdit.commit() && normal.snapshot() == before,
                    "Rejected workflow published state");
            rejected = true;
        } else {
            require(normal.snapshot() == before, "C++ peak preview changed owner");
            const auto preview = qualpf::gui::encode_project_peaks(peaksEdit.preview()->peaks);
            const double automatic = peaksEdit.preview()->automaticDelta;
            require(bool(peaksEdit.commit()), "C++ peak commit failed");
            require(qualpf::gui::encode_project_peaks(normal.snapshot()->peaks) == preview &&
                        normal.snapshot()->automaticDelta == automatic &&
                        normal.snapshot()->peakTolerance == .15,
                    "C++ committed peak/FWHM/tolerance snapshot differs from "
                    "preview");
            require(bool(searchPeaks(normal.snapshot()->peaks)),
                    "C++ committed Search-Match projection failed");
            require(bool(session.commit(session.snapshot(), *normal.snapshot())),
                    "Cannot publish full workflow for product search");
            window.refreshPatternPresentation();
            auto builder = buildSearchMatchQuery(AppState::context(), AppState::peaks());
            QVector<CardType> cards;
            AppState::searches().makeQueryStrongest(builder, cards);
            require(AppState::searches().searchError().isEmpty(), "Workflow Search-Match failed");
            compareCpp(session.snapshot()->pattern, session.snapshot()->peaks, cards);
            workflowCards = scores(cards);
            const auto applied = session.snapshot();
            const auto configuration = qualpf::infrastructure::encodePreprocessing(applied->preprocessing);
            ProductWorkflowProbe::save(window, output + "-cpp-workflow.qxp");
            ProductWorkflowProbe::load(window, output + "-cpp-workflow.qxp");
            require(qualpf::infrastructure::encodePreprocessing(session.snapshot()->preprocessing) == configuration &&
                        qualpf::gui::encode_project_peaks(session.snapshot()->peaks) ==
                            qualpf::gui::encode_project_peaks(applied->peaks),
                    "C++ configuration/snapshot round trip failed");
            PreprocessingEdit resumed(session);
            PreprocessingRequest again;
            again.wavelength = request.wavelength;
            again.tolerance = request.tolerance;
            again.smoothing = session.snapshot()->preprocessing.smoothing;
            require(bool(resumed.preview(again)), "Loaded configuration could not resume editing");
            resumed.cancel();
        }
    }
    QFile report(output+"-preprocessing.json"); require(report.open(QIODevice::WriteOnly),"Cannot write workflow evidence");
    report.write(
        QJsonDocument(
            QJsonObject{
                {"legacy_gui_preview_cancel_apply", true},
                {"cpp_transaction", true},
                {"project_peak_tolerance_roundtrip", true},
                {"cpp_product_switch", false},
                {"legacy_undefined_rejected_without_publication", true},
                {"expected_atomic_rejection", rejected},
                {"workflow_searchmatch", workflowCards},
                {"preprocessing_configuration", qualpf::infrastructure::encodePreprocessing(normal.snapshot()->preprocessing)},
                {"normal_chain", "unsubtracted measured counts -> spline background estimate "
                                 "-> default SG -> automatic range -> peaks"},
                {"normal_peaks", qualpf::gui::encode_project_peaks(normal.snapshot()->peaks)},
                {"normal_automatic_tolerance", normal.snapshot()->automaticDelta}})
            .toJson());
    std::cout << "PASS: preprocessing real GUI preview/cancel/apply and isolated C++ transaction\n";
}

void currentToleranceChecks(const ExperimentalPeaks &initial) {
    for (double delta : {0.03, 0.08, 0.15, 0.5}) {
        const auto adjusted = qualpf::gui::peaksWithTolerance(initial, delta);
        require(bool(adjusted), "Search tolerance update failed");
        const auto &a = adjusted.value();
        require(a.intensity == initial.intensity && a.intensityOrig == initial.intensityOrig,
                "Tolerance update changed intensities");
        require(a.tth == initial.tth && a.d == initial.d && a.fwhm == initial.fwhm && a.wave == initial.wave &&
                    a.deltaD.size() == initial.deltaD.size() &&
                    std::all_of(a.deltaD.begin(), a.deltaD.end(), [](double d) { return std::isfinite(d) && d >= 0; }),
                "Tolerance update changed represented measurements or produced invalid windows");
    }
}
void currentRangeCancel(MainWindow &window, const CardType &candidate) {
    auto &session = AppState::searchSession();
    const auto before = session.snapshot();
    RangeDialog dialog(&window); dialog.show(); dialog.setRange(window.xpdViewer());
    require(session.snapshot()->editing, "Range edit did not acquire Session boundary");
    const auto opening = session.snapshot();
    const double low = before->pattern.two_theta.front().value + 1;
    const double high = before->pattern.two_theta.back().value - 1;
    const int graphCount = window.xpdViewer()->graphCount();
    window.xpdViewer()->leftCursorPositionChanged(low);
    window.xpdViewer()->rightCursorPositionChanged(high);
    dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Apply)->click();
    QApplication::processEvents();
    require(dialog.property("preprocessingReason").toString().isEmpty() && session.snapshot() == opening &&
                dialog.findChild<QDoubleSpinBox *>("minSpinBox")->property("rangeRequested").toDouble() == low &&
                dialog.findChild<QDoubleSpinBox *>("maxSpinBox")->property("rangeRequested").toDouble() == high,
            "Range did not preview the actual connected cursor request privately");
    std::vector<std::pair<double, double>> expected;
    for (std::size_t i = 0; i < before->pattern.observed.size(); ++i)
        if (before->pattern.two_theta[i].value >= low && before->pattern.two_theta[i].value <= high)
            expected.emplace_back(before->pattern.two_theta[i].value, before->pattern.observed[i]);
    require(!expected.empty() && expected.size() < before->pattern.observed.size() &&
                window.xpdViewer()->graphCount() == graphCount + 1,
            "Range preview did not produce a real cropped private overlay");
    const auto preview = window.xpdViewer()->graph(graphCount)->data();
    require(std::size_t(preview->size()) == expected.size(), "Range preview sample count differs from input slice");
    std::size_t i = 0;
    for (auto row = preview->constBegin(); row != preview->constEnd(); ++row, ++i)
        require(row->key == expected[i].first && row->value == expected[i].second,
                "Range preview differs from the exact input slice");
    auto blockedScore = qualpf::gui::scoreCard(*session.snapshot(), candidate, parameters());
    require(!blockedScore, "Search ran against a preprocessing preview");
    require(QMetaObject::invokeMethod(&dialog, "reject", Qt::DirectConnection), "Range cancel unavailable");
    const auto after = session.snapshot();
    require(!after->editing && after->pattern.observed == before->pattern.observed &&
                qualpf::gui::encode_project_peaks(after->peaks) == qualpf::gui::encode_project_peaks(before->peaks) &&
                scores(after->candidates) == scores(before->candidates), "Range cancel lost application state");
}
void currentRangeNoOp(MainWindow &window) {
    RangeDialog dialog(&window); dialog.show(); dialog.setRange(window.xpdViewer());
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    buttons->button(QDialogButtonBox::Ok)->click(); QApplication::processEvents();
    require(dialog.isClosed() && !AppState::searchSession().snapshot()->editing, "Current Range no-op did not close");
}

int main(int argc, char **argv) {
    const bool defaultCutover96 = argc > 1 && QString(argv[argc - 1]) == "--expect-default";
    if (defaultCutover96) argv[--argc] = nullptr;
    QApplication app(argc, argv);
    QTemporaryDir settings;
    QCoreApplication::setOrganizationName("QualpfValidation");
    QCoreApplication::setApplicationName("ProductProbe");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
    const bool guiDefaults96 = argc == 6 && QString(argv[5]) == "--finite-default96";
    QSettings().setValue("SearchOptions/delta2thetaAuto", guiDefaults96);
    try {
        if (argc == 5 && QString(argv[1]) == "--current-preprocessing-state") {
            using namespace qualpf::application; using namespace qualpf::gui;
            ApplicationInitializationService{}.initialize(AppState::presentation(), PresentationMode::automatic_gui);
            MainWindow::setPathDataFiles(QString::fromLocal8Bit(argv[3]));
            MainWindow window(AppState::context()); window.show();
            ProductWorkflowProbe::load(window, QString::fromUtf8(QUALPF_CURRENT_POOL_FIXTURE));
            const auto fixed = AppState::searchSession().snapshot();
            currentToleranceChecks(fixed->peaks);
            CardType card; card.setId("current-fixed-input"); card.setD(fixed->peaks.d, fixed->peaks.wave);
            card.setIntensity(fixed->peaks.intensity);
            currentRangeCancel(window, card);
            const auto beforeNoOp = AppState::searchSession().snapshot();
            currentRangeNoOp(window);
            require(qualpf::gui::encode_project_peaks(AppState::peaks()) ==
                        qualpf::gui::encode_project_peaks(beforeNoOp->peaks) &&
                        scores(AppState::searchSession().snapshot()->candidates) == scores(beforeNoOp->candidates),
                    "Unchanged current Range changed peak measurements/Search");
            require(window.loadDiffractionPatterns({QString::fromLocal8Bit(argv[2])}, false), "Current raw fixture import failed");
            phase93ProductChecks(window, QString::fromLocal8Bit(argv[4]), AppState::searchSession().snapshot());
            return 0;
        }
        if (argc == 3 && QString(argv[1]) == "--canonical-pattern-projection") {
            using namespace qualpf;
            using namespace qualpf::application; using namespace qualpf::gui;
            ApplicationInitializationService{}.initialize(AppState::presentation(), PresentationMode::headless);
            SessionStateView input;
            input.pattern.id = {1};
            input.pattern.two_theta = {TwoThetaDegrees{10.0000000001}, TwoThetaDegrees{11.0000000001}, TwoThetaDegrees{12.0000000001}};
            input.pattern.observed = {1.0000000001, 2.0000000001, 3.0000000001};
            input.pattern.original_observed = input.pattern.observed;
            input.pattern.radiation.lines = {{WavelengthAngstrom{1.54059307}, 1}};
            input.pattern.source_precision = SourcePrecision::float64;
            input.pattern.source_id = "canonical-pattern-projection";
            input.pattern.calculation_range = AngularRange{input.pattern.two_theta.front(), input.pattern.two_theta.back()};
            require(bool(AppState::searchSession().commit(AppState::searchSession().snapshot(), input)), "Canonical projection fixture rejected");
            const auto observed = pattern();
            QJsonArray x, y;
            QByteArray binary64;
            for (auto v : observed.two_theta) { x.append(v.value); binary64.append(reinterpret_cast<const char *>(&v.value), sizeof(v.value)); }
            for (auto v : observed.observed) { y.append(v); binary64.append(reinterpret_cast<const char *>(&v), sizeof(v)); }
            const double wave = observed.radiation.lines.front().wavelength.value;
            binary64.append(reinterpret_cast<const char *>(&wave), sizeof(wave));
            bool exact = observed.two_theta.size() == 3 && observed.observed.size() == 3 && observed.id == input.pattern.id &&
                observed.source_id == input.pattern.source_id && observed.source_precision == SourcePrecision::float64 &&
                observed.original_observed == input.pattern.original_observed && wave == 1.54059307 &&
                observed.calculation_range && observed.calculation_range->minimum.value == 10.0000000001 &&
                observed.calculation_range->maximum.value == 12.0000000001;
            for (std::size_t i = 0; exact && i < 3; ++i)
                exact = observed.two_theta[i].value == input.pattern.two_theta[i].value && observed.observed[i] == input.pattern.observed[i];
            QFile report(QString::fromLocal8Bit(argv[2]));
            require(report.open(QIODevice::WriteOnly), "Cannot write projection observation");
            report.write(QJsonDocument(QJsonObject{{"x", x}, {"y", y}, {"wave", wave},
                {"binary64_le", QString::fromLatin1(binary64.toHex())}, {"source_precision", int(observed.source_precision)},
                {"original_available", bool(observed.original_observed)}, {"canonical_exact", exact}}).toJson());
            report.close();
            require(exact, "Native projection differs from already-approved canonical Pattern owner");
            return 0;
        }
        if (argc == 3 && QString(argv[1]) == "--pdf2-lifecycle") {
            pdf2Lifecycle34(QString::fromLocal8Bit(argv[2]));
            return 0;
        }
        if (argc == 4 && QString(argv[1]) == "--pattern-lifecycle") {
            patternLifecycle11(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]));
            return 0;
        }
        if (argc == 4 && QString(argv[1]) == "--kalpha2-lifecycle") {
            kalpha2Lifecycle12(QString::fromLocal8Bit(argv[2]), QString::fromLocal8Bit(argv[3]));
            return 0;
        }
        if (defaultCutover96) {
            require(!qEnvironmentVariableIsSet("QUALPF_PREPROCESSING_BACKEND") &&
                        !qEnvironmentVariableIsSet("QUALPF_CPP_PREPROCESSING_DIALOGS") &&
                        !qEnvironmentVariableIsSet("QUALPF_SEARCH_BACKEND") &&
                        qualpf::gui::cppPreprocessingProduct(),
                    "Default acceptance requires absent preprocessing/QA/search variables and C++ dispatch");
            std::cout << "CUTOVER: preprocessing, QA and Search environment variables absent; default C++ dispatch\n";
        }
        // Exercise the actual text round-trip, including both Cu lines and a
        // saved historical/user-defined value that must not become the default.
        WavelengthsWidget radiationWidget;
        require(radiationWidget.wave1().toDouble() == qualpf::radiation::CU_KA1_WAVE,
                "Cu K-alpha1 preset lost precision");
        radiationWidget.setSeconWave(Qt::Checked, qualpf::radiation::CU_KA2_WAVE);
        require(radiationWidget.wave2().toDouble() == qualpf::radiation::CU_KA2_WAVE,
                "Cu K-alpha2 preset lost precision");
        radiationWidget.setComboAsUserDefined(1.54056);
        require(radiationWidget.wave1().toDouble() == 1.54056,
                "Saved historical wavelength was replaced by the default");
        radiationWidget.setUserWave(0.12345678901234567);
        require(radiationWidget.wave1().toDouble() == 0.12345678901234567,
                "User-defined wavelength did not round-trip");
        codecTests();
        if (argc == 2 && QString(argv[1]) == "--codec")
            return 0;
        if (argc == 2 && QString(argv[1]) == "--startup") {
            QSettings startupSettings(settings.path() + "/IC/qualpf.ini", QSettings::IniFormat);
            startupSettings.setValue("databases/defaultDir", settings.path());
            startupSettings.sync();
            QProcess gui;
            QString executable = QString::fromUtf8(QUALPF_PRODUCT_EXECUTABLE);
#ifdef Q_OS_WIN
            executable += ".exe";
#endif
            gui.start(executable, {"--settings-dir", settings.path()});
            require(gui.waitForStarted(10000), "Product GUI failed to start");
            require(!gui.waitForFinished(2000), "Product GUI exited during startup");
            gui.terminate();
            if (!gui.waitForFinished(5000)) {
                gui.kill();
                gui.waitForFinished(5000);
            }
            QProcess automatic;
            automatic.start(executable, {"--auto", "--search", "--settings-dir", settings.path()});
            require(automatic.waitForFinished(10000) && automatic.exitStatus() == QProcess::NormalExit &&
                    automatic.exitCode() == 0, "Automatic GUI search did not drain and exit normally");
            std::cout << "PASS: product GUI executable starts and remains in event loop\n";
            return 0;
        }
        require(argc == 5 || (argc == 6 && (QString(argv[5]) == "--kalpha2-workflow" || QString(argv[5]) == "--preprocessing" || QString(argv[5]) == "--product93" || QString(argv[5]) == "--product94" || QString(argv[5]) == "--formats95" || QString(argv[5]) == "--contract96" || QString(argv[5]) == "--finite96" || QString(argv[5]) == "--reopen96" || QString(argv[5]) == "--manualfix96" || QString(argv[5]) == "--count96" || QString(argv[5]) == "--tolerance96" || guiDefaults96 || QString(argv[5]) == "--radiation-cli")), "usage: probe database-prefix pattern data-tables output-prefix [--preprocessing|--product93|--product94|--formats95|--contract96|--finite96|--reopen96]");
        require(AppState::db().openDatabases(QString::fromLocal8Bit(argv[1])), "Database open failed");
        const QByteArray source = QFile::encodeName(QString::fromLocal8Bit(argv[2]));
        const QByteArray tables =
            QFile::encodeName(QDir(QString::fromLocal8Bit(argv[3])).absolutePath() + "/");
        const QString output = QString::fromLocal8Bit(argv[4]);
        QDir().mkpath(QFileInfo(output).absolutePath());
        MainWindow::setPathDataFiles(QString::fromLocal8Bit(argv[3]));
        MainWindow window(AppState::context());
        window.show();
        int wavelengthDialogs = 0;
        QTimer inputDriver;
        QObject::connect(&inputDriver, &QTimer::timeout, [&] {
            if (auto *dialog = qobject_cast<WaveDialog *>(QApplication::activeModalWidget())) {
                // Exercise the actual GUI default; never mask CLI/GUI drift
                // by supplying an explicit wavelength in the test driver.
                auto *wavelength = dialog->findChild<WavelengthsWidget *>();
                if (!wavelength)
                    std::abort();
                require(dialog->wave1() == qualpf::radiation::default_wavelength_angstrom,
                        "GUI wavelength differs from canonical Pattern IO default");
                std::cout << "GUI wavelength accepted: " << dialog->wave1() << std::endl;
                ++wavelengthDialogs;
                dialog->accept();
            }
        });
        inputDriver.start(10);
        qualpf::application::StartupOptions options{0, 1, 0, -1};
        int error = 0;
        if (argc == 6 && QString(argv[5]) == "--radiation-cli") {
            // Same C++ read/transaction as the CLI, without a radiation dialog.
            options.nogui = 1;
            qualpf::legacy::initialize_product_presentation_for_probe(&options, tables.constData(), tables.size(), &error);
            require(bool(qualpf::gui::openPatternFile(AppState::searchSession(), AppState::context().files().patterns,
                std::filesystem::path(QString::fromLocal8Bit(argv[2]).toStdWString()))), "CLI PatternReader failed");
            window.refreshPatternPresentation();
            const auto state = AppState::searchSession().snapshot();
            require(error == 0 && !state->pattern.radiation.lines.empty(), "CLI radiation import failed");
            const double actual = state->pattern.radiation.lines.front().wavelength.value;
            require(actual == qualpf::radiation::default_wavelength_angstrom,
                    "CLI default differs from the canonical wavelength in the C++ Session");
            require(wavelengthDialogs == 0, "No-GUI import opened a wavelength dialog");
            QFile result(output + "-radiation.json");
            require(result.open(QIODevice::WriteOnly), "Cannot write radiation result");
            result.write(QJsonDocument(QJsonObject{{"canonical", qualpf::radiation::CU_KA1_WAVE},
                {"transported", actual}, {"explicit_wavelength_supplied", false}}).toJson());
            AppState::db().closeDatabeses();
            return 0;
        }
        if (QFileInfo(QString::fromLocal8Bit(argv[2])).suffix().compare("qxp", Qt::CaseInsensitive) == 0) {
            qualpf::legacy::initialize_product_presentation_for_probe(&options, tables.constData(), tables.size(), &error);
            require(error == 0, "Project runtime initialization failed");
            if (argc == 6 && QString(argv[5]) == "--tolerance96") {
                toleranceAcceptance96(window, QString::fromLocal8Bit(argv[2]), output);
                AppState::db().closeDatabeses();
                return 0;
            }
            if (argc == 6 && QString(argv[5]) == "--count96") {
                searchSnapshot96(window, QString::fromLocal8Bit(argv[2]), output);
                AppState::db().closeDatabeses();
                return 0;
            }
            if (argc == 6 && QString(argv[5]) == "--manualfix96") {
                manualAcceptance96(window, QString::fromLocal8Bit(argv[2]), output);
                AppState::db().closeDatabeses();
                return 0;
            }
            if (argc == 6 && QString(argv[5]) == "--reopen96") {
                phase96Reopen(window, QString::fromLocal8Bit(argv[2]), output);
                AppState::db().closeDatabeses();
                return 0;
            }
            archivalProject(window, QString::fromLocal8Bit(argv[2]), output);
            AppState::db().closeDatabeses();
            return 0;
        }
        const bool product96 = argc == 6 && (QString(argv[5]) == "--contract96" || QString(argv[5]) == "--finite96" || guiDefaults96);
        if (product96) {
            qualpf::legacy::initialize_product_presentation_for_probe(&options, tables.constData(), tables.size(), &error);
            require(error == 0, "Product runtime initialization failed");
            // Import through the actual menu, using an identical workspace copy so the
            // retained Pattern IO writer cannot put its .out beside the user's original.
            QFile measured(QString::fromLocal8Bit(argv[2]));
            require(measured.open(QIODevice::ReadOnly), "Cannot read measured input");
            QFile local(output + "-input.dat");
            require(local.open(QIODevice::WriteOnly), "Cannot copy measured input");
            const auto bytes = measured.readAll();
            require(local.write(bytes) == bytes.size(), "Input copy incomplete");
            local.close();
            bool imported = false;
            QTimer importDriver;
            QObject::connect(&importDriver, &QTimer::timeout, [&] {
                if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
                    importDriver.stop();
                    imported = true;
                    dialog->selectFile(local.fileName());
                    QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                }
            });
            importDriver.start(10);
            require(QMetaObject::invokeMethod(&window, "onActionImportDiffractionPatternTriggered", Qt::DirectConnection),
                    "Import menu unavailable");
            require(imported && wavelengthDialogs > 0, "Actual import/wavelength dialogs not exercised");
        } else {
            qualpf::legacy::initialize_product_presentation_for_probe(&options, tables.constData(), tables.size(), &error);
            require(window.loadDiffractionPatterns({QString::fromLocal8Bit(argv[2])}), "C++ GUI PatternReader failed");
        }
        require(error == 0, "Pattern import failed");
        const auto unprocessed = AppState::searchSession().snapshot();
        if (argc == 6 && QString(argv[5]) == "--kalpha2-workflow") {
            kalpha2Workflow12(window,output);
            AppState::db().closeDatabeses();
            return 0;
        }
        require(!unprocessed->pattern.radiation.lines.empty() &&
                    unprocessed->pattern.radiation.lines.front().wavelength.value ==
                        qualpf::radiation::default_wavelength_angstrom,
                "GUI default was not preserved across C++ Pattern IO");
        if (product96) {
            phase96Contract(window, output);
            if (QString(argv[5]) == "--finite96" || guiDefaults96) phase96Finite(window, output);
            AppState::db().closeDatabeses();
            return 0;
        }
        if (argc == 6 && QString(argv[5]) == "--formats95") {
            phase95FormatChecks(window, output, defaultCutover96);
            AppState::db().closeDatabeses();
            return 0;
        }
        if (defaultCutover96 && argc == 6 && QString(argv[5]) == "--product94") {
            // An explicit finite manual input seeds the GUI matrix. Never run the
            // undefined Legacy automatic chain to manufacture a successful fixture.
            qualpf::application::PreprocessingRequest request;
            request.wavelength = unprocessed->pattern.radiation.lines.front().wavelength;
            request.tolerance = qualpf::AngularWidthDegrees{.15};
            request.manual = qualpf::application::ManualPeakChange{};
            request.manual->two_theta = 26.64;
            request.manual->intensity = 100;
            request.manual->fwhm = .12;
            qualpf::gui::PreprocessingEdit edit(AppState::searchSession());
            require(bool(edit.preview(request)) && bool(edit.commit()), "Finite manual matrix fixture failed");
            window.refreshPatternPresentation();
            window.refreshPreprocessingDisplay();
            phase94ProductChecks(window, output, unprocessed, true);
            require(!qEnvironmentVariableIsSet("QUALPF_PREPROCESSING_BACKEND"), "Matrix set a backend variable");
            AppState::db().closeDatabeses();
            return 0;
        }
        if (argc == 6 && QString(argv[5]) == "--product93") {
            phase93ProductChecks(window,output,unprocessed);
            AppState::db().closeDatabeses();
            return 0;
        }
        if (argc == 6) ProductWorkflowProbe::save(window,output+"-unprocessed.qxp");
        require(ensurePeaksFound(AppState::context()) > 0 && loadExperimentalPeaks(AppState::context()) > 0, "Measured peak search failed");
        if (argc == 6 && QString(argv[5]) == "--product94") {
            phase94ProductChecks(window,output,unprocessed);
            AppState::db().closeDatabeses();
            return 0;
        }
        if (argc == 6) {
            preprocessingWorkflow(window, output,unprocessed);
            AppState::db().closeDatabeses();
            return 0;
        }
        const auto initial = AppState::peaks();
        currentToleranceChecks(initial);

        const auto working = pattern();
        int topRows = 0;
        {
            QSqlQuery top(
                QSqlDatabase::database(QFileInfo(QString::fromLocal8Bit(argv[1]) + ".sq.search").fileName()));
            require(top.exec("SELECT n, dval FROM top"), "Cannot read stored strongest index");
            const auto peaks = project(initial);
            while (top.next()) {
                const auto stored =
                    SearchUtil::extractNumbers(top.value(1).toString(), top.value(0).toInt(), 3);
                std::vector<qualpf::DSpacingAngstrom> values;
                for (double value : stored)
                    values.emplace_back(value);
                auto result = qualpf::search::passes_strongest(peaks, values);
                require(bool(result), "Invalid real stored strongest index");
                require(result.value() ==
                            SearchUtil::checkStrongValuesWithTolerance(initial.d, initial.deltaD, stored),
                        "Real strongest prefilter decision differs");
                ++topRows;
            }
        }
        auto builder = buildSearchMatchQuery(AppState::context(), initial);
        builder.enableDeleted(SearchOptionsDialog::savedCheckDeleted(AppState::workspace().session().snapshot(), AppState::context().settings()));
        QVector<CardType> queried;
        AppState::searches().makeQueryStrongest(builder, queried);
        require(!queried.isEmpty(), "Real database returned no candidates");
        const auto countBeforeLimit = queried.size();
        currentRangeCancel(window, queried.first());

        auto constrained = builder;
        constrained.setIdEntry({queried.first().getId()});
        QVector<CardType> constrainedCards;
        AppState::searches().makeQueryWithoutStrongest(constrained, constrainedCards);
        require(AppState::searches().searchError().isEmpty(), "Constrained database scoring failed");
        compareCards(constrainedCards, {queried.first()});
        compareCards(AppState::searches().makeQuery(constrained), constrainedCards);
        const auto failedBrowse = AppState::searches().makeQuery(constrained,
            [](int, int) { AppState::searches().cancelSearch(); });
        require(AppState::searches().isCanceled() && failedBrowse.isEmpty(), "Scored restraint query published partial output");


        QVector<CardType> cancelled{queried.first()};
        const auto sentinel = scores(cancelled);
        AppState::searches().makeQueryStrongest(builder, cancelled,
                                          [](int, int) { AppState::searches().cancelSearch(); });
        require(AppState::searches().isCanceled() && scores(cancelled) == sentinel,
                "Cancelled search published partial cards");
        QVector<CardType> stale{queried.first()};
        AppState::searches().makeQueryStrongest(builder, stale, [&](int, int) { AppState::replacePeaks(initial); });
        require(!AppState::searches().searchError().isEmpty() && scores(stale) == sentinel,
                "Stale search published cards");
        QVector<CardType> databaseChanged{queried.first()};
        AppState::searches().makeQueryStrongest(builder, databaseChanged,
                                          [](int, int) { AppState::db().closeDatabeses(); });
        require(!AppState::searches().searchError().isEmpty() && scores(databaseChanged) == sentinel,
                "Reentrant database replacement published cards");
        AppState::db().closeDatabeses();
        QVector<CardType> failed{queried.first()};
        AppState::searches().makeQueryStrongest(builder, failed);
        require(!AppState::searches().searchError().isEmpty() && scores(failed) == sentinel,
                "Failed SQL published cards");
        require(AppState::db().openDatabases(QString::fromLocal8Bit(argv[1])), "Database reopen failed");
        if (queried.size() > SearchOptionsDialog::savedMaxEntries(AppState::workspace().session().snapshot(), AppState::context().settings())) {
            std::stable_sort(queried.begin(), queried.end(),
                             [](const auto &a, const auto &b) { return a.getFom() > b.getFom(); });
            queried.resize(SearchOptionsDialog::savedMaxEntries(AppState::workspace().session().snapshot(), AppState::context().settings()));
        }
        compareCpp(working, initial, queried);
        // Launch the actual CLI in an isolated settings directory. Do not
        // modify the user's registry-backed application preferences.
        QSettings cliSettings(settings.path() + "/IC/qualpf.ini", QSettings::IniFormat);
        cliSettings.setValue("databases/count", 1);
        cliSettings.setValue("databases/0/inUse", true);
        cliSettings.setValue("databases/0/name", "cod_inorg");
        cliSettings.setValue("databases/0/path", QString::fromLocal8Bit(argv[1]));
        cliSettings.setValue("databases/defaultDir",
                             QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath());
        cliSettings.setValue("SearchOptions/delta2thetaAuto", false);
        cliSettings.sync();
        QProcess cli;
        QString executable = QString::fromUtf8(QUALPF_PRODUCT_EXECUTABLE);
#ifdef Q_OS_WIN
        executable += ".exe";
#endif
        cli.start(executable, {"--nogui", "--auto", "--settings-dir", settings.path(), "--search",
                               QString::fromLocal8Bit(argv[2])});
        require(cli.waitForFinished(60000), "CLI timed out");
        const QByteArray cliOutput = cli.readAllStandardOutput();
        QFile cliLog(output + "-cli.txt");
        require(cliLog.open(QIODevice::WriteOnly), "Cannot write CLI log");
        cliLog.write(cliOutput);
        cliLog.write(cli.readAllStandardError());
        require(cli.exitStatus() == QProcess::NormalExit && cli.exitCode() == 0, "CLI search failed");
        QStringList cliIds, queryIds;
        auto matches = QRegularExpression("\\[([0-9]+)\\]").globalMatch(QString::fromLocal8Bit(cliOutput));
        while (matches.hasNext())
            cliIds.append(matches.next().captured(1));
        for (const auto &card : queried)
            queryIds.append(card.getId());
        require(cliIds == queryIds, "CLI source order/candidate set differs");
        auto legacyEnvironment = QProcessEnvironment::systemEnvironment();
        legacyEnvironment.insert("QUALPF_SEARCH_BACKEND", "legacy");
        cli.setProcessEnvironment(legacyEnvironment);
        cli.start(executable, {"--nogui", "--auto", "--settings-dir", settings.path(), "--search", QString::fromLocal8Bit(argv[2])});
        require(cli.waitForFinished(60000) && cli.exitCode() == 0, "Explicit CLI fallback failed");
        QStringList fallbackIds;
        auto fallbackMatches = QRegularExpression("\\[([0-9]+)\\]").globalMatch(QString::fromLocal8Bit(cli.readAllStandardOutput()));
        while (fallbackMatches.hasNext()) fallbackIds.append(fallbackMatches.next().captured(1));
        require(fallbackIds == queryIds, "Fallback changed CLI candidate set/order");
        cli.setProcessEnvironment(QProcessEnvironment::systemEnvironment());

        cliSettings.setValue("databases/0/path", settings.path() + "/missing-reference-db");
        cliSettings.setValue("databases/defaultDir", settings.path());
        cliSettings.sync();
        cli.start(executable, {"--nogui", "--auto", "--settings-dir", settings.path(), "--search",
                               QString::fromLocal8Bit(argv[2])});
        require(cli.waitForFinished(60000), "CLI failure case timed out");
        require(cli.exitStatus() == QProcess::NormalExit && cli.exitCode() != 0 &&
                    cli.readAllStandardError().contains("Search failed:"),
                "CLI database failure did not return an explicit error");
        window.runSearchMatch(); awaitSearch(window);
        auto *results = window.findChild<DbResultsWidget *>();
        auto *quant = window.findChild<QuantWidget *>();
        require(results && quant, "GUI widgets missing");
        const auto gui = results->allCards();
        compareCards(gui, queried); // GUI table ordering is tested separately below.
        QVector<CardType> unfiltered;
        AppState::searches().makeQueryWithoutStrongest(builder, unfiltered);
        require(AppState::searches().searchError().isEmpty(), "Unfiltered database scoring failed");
        require(unfiltered.size() >= queried.size(), "Unfiltered search lost strongest candidates");
        compareCpp(working, initial, unfiltered);
        const auto inputBeforeManual = AppState::searchSession().snapshot();
        auto mismatched = *inputBeforeManual;
        mismatched.backend = qualpf::application::SearchBackend::legacy_fallback;
        mismatched.pattern.observed[0] += 1;
        auto canonicalInput = mismatched;
        canonicalInput.backend = qualpf::application::SearchBackend::cpp;
        const auto expectedScore = qualpf::gui::scoreCard(canonicalInput, gui.first(), parameters());
        const auto compatibilityScore = qualpf::gui::scoreCard(mismatched, gui.first(), parameters());
        require(expectedScore && compatibilityScore &&
                compatibilityScore.value().total == expectedScore.value().total &&
                compatibilityScore.value().position == expectedScore.value().position &&
                compatibilityScore.value().intensity == expectedScore.value().intensity &&
                compatibilityScore.value().scale == expectedScore.value().scale &&
                AppState::searchSession().snapshot() == inputBeforeManual, "Canonical compatibility score changed state or result");
        auto staleCommit = AppState::searchSession().commit(std::make_shared<const qualpf::gui::SessionStateView>(*inputBeforeManual), *inputBeforeManual);
        require(!staleCommit && AppState::searchSession().snapshot() == inputBeforeManual, "Foreign snapshot committed");
        auto invalid = *inputBeforeManual;
        invalid.pattern.observed.clear();
        auto failedScores = qualpf::gui::rescoreCards(invalid, parameters());
        require(!failedScores && AppState::searchSession().snapshot() == inputBeforeManual, "Failed score changed Session");
        results->setResults({});
        QTimer manualDriver;
        QObject::connect(&manualDriver, &QTimer::timeout, [&] {
            if (auto *dialog = qobject_cast<QInputDialog *>(QApplication::activeModalWidget())) {
                dialog->setTextValue(gui.first().getId()); dialog->accept();
            }
        });
        manualDriver.start(10);
        ProductWorkflowProbe::manual(window);
        manualDriver.stop();
        require(results->allCards().size() == 1, "Manual card insertion failed");
        compareCards(results->allCards(), {gui.first()});
        manualDriver.start(10);
        ProductWorkflowProbe::manual(window);
        manualDriver.stop();
        require(results->allCards().size() == 1, "Duplicate manual card inserted");
        results->setResults(gui);

        const auto beforeAcceptance = AppState::searchSession().snapshot();
        results->selectCard(gui.first().getId());
        results->acceptSelectedCards(); awaitSearch(window);
        require(quant->phases().size() == 1 && quant->phases().first().getId() == gui.first().getId(), "GUI acceptance did not commit phase");
        const auto acceptedResidual = qualpf::gui::encode_project_peaks(AppState::peaks());
        results->addCard(gui.first());
        results->selectCard(gui.first().getId());
        results->acceptSelectedCards(); awaitSearch(window);
        require(quant->phases().size() == 1 && qualpf::gui::encode_project_peaks(AppState::peaks()) == acceptedResidual,
                "Duplicate acceptance subtracted twice");
        auto *phaseTable = quant->findChild<QTableView *>();
        require(phaseTable != nullptr, "Phase table unavailable");
        phaseTable->selectRow(0);
        require(QMetaObject::invokeMethod(quant, "removeSelectedPhase", Qt::DirectConnection), "Cannot remove accepted phase");
        require(quant->phases().isEmpty() && qualpf::gui::encode_project_peaks(AppState::peaks()) == acceptedResidual,
                "Legacy phase removal semantics changed (removal is not residual undo)");
        auto resetStatus = AppState::searchSession().commit(AppState::searchSession().snapshot(), *beforeAcceptance);
        require(bool(resetStatus), "Cannot restore independent workflow input");
        quant->refresh();
        results->setResults(gui);
        // Zero weights are an explicit undefined Legacy input, never an equivalence pass.
        auto invalidAcceptance = *AppState::searchSession().snapshot();
        invalidAcceptance.searchOptions["SearchOptions/weight2thetaD"] = 0.0;
        invalidAcceptance.searchOptions["SearchOptions/weightIntensity"] = 0.0;
        invalidAcceptance.searchOptions["SearchOptions/weightPhases"] = 0.0;
        require(bool(AppState::searchSession().commit(AppState::searchSession().snapshot(), invalidAcceptance)), "Invalid scenario preparation failed");
        const auto beforeFailure = AppState::searchSession().snapshot();
        results->selectCard(gui.first().getId());
        results->acceptSelectedCards(); awaitSearch(window);
        require(AppState::searchSession().snapshot() == beforeFailure, "Failed acceptance changed peaks, results or phases");
        require(bool(AppState::searchSession().commit(beforeFailure, *beforeAcceptance)), "Failed scenario reset");
        results->setResults(gui);
        const auto before = AppState::peaks();
        const auto accepted = gui.first();
        const auto expectedResidual = qualpf::search::subtract_phase(project(before), project(accepted),
                                                                     accepted.getScale(), parameters());
        require(bool(expectedResidual), "C++ residual failed");
        quant->addPhase(accepted);
        ProductWorkflowProbe::residual(window, accepted);
        const auto residual = AppState::peaks();
        require(residual.tth.size() == qsizetype(expectedResidual.value().peaks.peaks.size()),
                "Residual survivor count");
        for (qsizetype i = 0; i < residual.tth.size(); ++i) {
            const auto &peak = expectedResidual.value().peaks.peaks[i];
            close(residual.intensity[i], peak.intensity, 1e-12, 1e-12, "residual current");
            require(residual.intensityOrig[i] == peak.original_intensity.value(), "residual original");
            require(residual.tth[i] == peak.two_theta.value && residual.d[i] == peak.d_spacing->value &&
                        residual.deltaD[i] == peak.delta_d->value && residual.fwhm[i] == peak.fwhm->value,
                    "Residual survivor metadata changed");
        }
        const auto beforeNoOp = AppState::searchSession().snapshot();
        currentRangeNoOp(window);
        require(!AppState::searchSession().snapshot()->editing &&
                qualpf::gui::encode_project_peaks(AppState::peaks()) == qualpf::gui::encode_project_peaks(beforeNoOp->peaks) &&
                scores(AppState::searchSession().snapshot()->candidates) == scores(beforeNoOp->candidates) &&
                AppState::searchSession().snapshot()->phases.first().isFomCalculated(),
                "Unchanged current Range changed residual/original columns or invalidated phases");
        const auto residualCards = results->allCards();
        const auto beforeContinue = qualpf::gui::encode_project_peaks(AppState::peaks());
        require(ensurePeaksFound(AppState::context()) == residual.tth.size() && loadExperimentalPeaks(AppState::context()) == residual.tth.size(), "Continuation peak count changed");
        require(qualpf::gui::encode_project_peaks(AppState::peaks()) == beforeContinue, "Continuation replaced the current residual peaks");

        const auto residualDiagnostics = compareRescore(working, residual, gui, residualCards);
        const auto beforeResidualQuery = AppState::searchSession().snapshot();
        auto residualBuilder = buildSearchMatchQuery(AppState::context(), residual);
        residualBuilder.enableDeleted(SearchOptionsDialog::savedCheckDeleted(AppState::workspace().session().snapshot(), AppState::context().settings()));
        QVector<CardType> freshResidualCards;
        AppState::searches().makeQueryStrongest(residualBuilder, freshResidualCards);
        require(AppState::searches().searchError().isEmpty(), "Residual database query failed");
        compareCpp(working, residual, freshResidualCards);
        window.runSearchMatch(); awaitSearch(window);
        compareCards(results->allCards(), freshResidualCards);
        require(qualpf::gui::encode_project_peaks(AppState::peaks()) == beforeContinue,
                "GUI residual database search replaced current peaks");
        require(bool(AppState::searchSession().commit(AppState::searchSession().snapshot(), *beforeResidualQuery)),
                "Cannot restore independent rescore workflow");

        auto *restraints = window.findChild<RestraintsDialog *>();
        require(restraints != nullptr, "Missing restraints dialog");
        restraints->restore({{"ids", accepted.getId().toStdString()}});
        auto persistenceQuery = buildSearchMatchQuery(AppState::context(), AppState::peaks());
        ProductWorkflowProbe::restraints(window, persistenceQuery);
        const QString projectFile = output + ".qxp";
        ProductWorkflowProbe::save(window, projectFile);
        QFile projectFileBytes(projectFile);
        require(projectFileBytes.open(QIODevice::ReadOnly), "Project not saved");
        const auto saved = QJsonDocument::fromJson(projectFileBytes.readAll()).object();
        require(saved["version"].toInt() == 2, "Wrong written project version");
        projectFileBytes.close();
        const auto beforeInvalidLoad = AppState::searchSession().snapshot();
        const auto currentBeforeInvalid = pattern();
        auto invalidProject = saved;
        invalidProject["automatic_delta"] = -1;
        QFile badProject(output + "-invalid.qxp");
        require(badProject.open(QIODevice::WriteOnly), "Cannot write rejection scenario");
        badProject.write(QJsonDocument(invalidProject).toJson()); badProject.close();
        ProductWorkflowProbe::load(window, badProject.fileName());
        require(AppState::searchSession().snapshot() == beforeInvalidLoad && pattern().observed == currentBeforeInvalid.observed,
                "Rejected project modified canonical Session/Pattern");
        const auto savedState = AppState::searchSession().snapshot();
        // A project carries applied options independently of changed preferences.
        QSettings().setValue("SearchOptions/delta2theta", 0.3);
        ProductWorkflowProbe::load(window, projectFile);
        require(AppState::searchSession().snapshot()->searchOptions == savedState->searchOptions &&
                AppState::searchSession().snapshot()->peakTolerance == savedState->peakTolerance &&
                AppState::searchSession().snapshot()->pattern.calculation_range->minimum.value == savedState->pattern.calculation_range->minimum.value &&
                AppState::searchSession().snapshot()->restraints == savedState->restraints,
                "Project lost applied settings, restraints or range");
        require(restraints->entryIds() == QStringList{accepted.getId()}, "Project did not restore applied GUI restraint");
        require(qualpf::gui::encode_project_peaks(AppState::peaks()) ==
                    qualpf::gui::encode_project_peaks(residual),
                "Live project round trip lost peak state");
        require(scores(results->allCards()) == scores(residualCards),
                "Project lost order/FOM/previous scale");
        require(quant->phases().size() == 1 && quant->phases()[0].getScale() == accepted.getScale(),
                "Accepted phase lost");
        ProductWorkflowProbe::rescore(window);
        if (scores(results->allCards()) != scores(residualCards)) {
            QFile difference(output + "-reload-difference.json");
            require(difference.open(QIODevice::WriteOnly), "Cannot write reload diagnostics");
            const auto reloadedPattern = pattern();
            difference.write(
                QJsonDocument(
                    QJsonObject{
                        {"before", scores(residualCards)},
                        {"after", scores(results->allCards())},
                        {"pattern_y_equal", reloadedPattern.observed == working.observed},
                        {"range_before", QJsonArray{working.calculation_range->minimum.value,
                                                    working.calculation_range->maximum.value}},
                        {"range_after", QJsonArray{reloadedPattern.calculation_range->minimum.value,
                                                   reloadedPattern.calculation_range->maximum.value}}})
                    .toJson());
        }
        require(scores(results->allCards()) == scores(residualCards), "Continued scoring differs after load");
        // Exercise the actual v1 loader, with the layout the old writer used.
        // This is a generated legacy-format project, not an archived user project.
        auto legacyRoot = saved;
        legacyRoot["version"] = 1;
        auto legacyPeaks = legacyRoot["peaks"].toObject();
        legacyPeaks.remove("current_intensity");
        legacyPeaks.remove("valid");
        legacyRoot["peaks"] = legacyPeaks;
        QFile legacyFile(output + "-v1.qxp");
        require(legacyFile.open(QIODevice::WriteOnly), "Cannot write v1 probe");
        legacyFile.write(QJsonDocument(legacyRoot).toJson());
        legacyFile.close();
        ProductWorkflowProbe::load(window, legacyFile.fileName());
        require(AppState::peaks().intensity == residual.intensityOrig,
                "v1 observed restore differs from historical contract");
        ProductWorkflowProbe::load(window, projectFile);
        if (!residualCards.isEmpty()) {
            const auto second = residualCards.first();
            ProductWorkflowProbe::residual(window, second);
            const auto continuedPeaks = qualpf::gui::encode_project_peaks(AppState::peaks());
            const auto continuedCards = scores(results->allCards());
            ProductWorkflowProbe::load(window, projectFile);
            ProductWorkflowProbe::residual(window, second);
            require(qualpf::gui::encode_project_peaks(AppState::peaks()) == continuedPeaks &&
                        scores(results->allCards()) == continuedCards,
                    "Second residual differs across reload");
        }
        QJsonObject report{
            {"source", QString::fromLocal8Bit(argv[2])},
            {"gui_callbacks_enabled", true},
            {"wavelength_dialogs_accepted", wavelengthDialogs},
            {"wavelength", initial.wave},
            {"peaks", initial.tth.size()},
            {"stored_top_rows_compared", topRows},
            {"accepted_before_limit", countBeforeLimit},
            {"without_strongest_retained", unfiltered.size()},
            {"residual_database_candidates", freshResidualCards.size()},
            {"manual_duplicate_accept_remove_failure_checks", true},
            {"initial", scores(gui)},
            {"residual_peak_count", residual.tth.size()},
            {"rescore_undefined_inputs_excluded_from_numeric_passes", residualDiagnostics.size()},
            {"rescore_legacy_undefined_inputs", residualDiagnostics},
            {"residual", scores(residualCards)},
            {"product_backend", AppState::searchSession().snapshot()->backend == qualpf::application::SearchBackend::cpp ? "cpp" : "legacy_fallback"},
            {"status", "passed_scoped"}};
        QFile reportFile(output + ".json");
        require(reportFile.open(QIODevice::WriteOnly), "Cannot write probe report");
        reportFile.write(QJsonDocument(report).toJson());
        std::cout << "PASS: real import/peak search/SQL/GUI/retained FOM/residual/qxp/continued scoring\n";
        AppState::db().closeDatabeses();
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        AppState::db().closeDatabeses();
        return 1;
    }
}
