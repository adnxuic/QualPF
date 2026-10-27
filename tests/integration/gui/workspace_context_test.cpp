#include "infrastructure/database/database_access.h"
#include <QFile>
#include "infrastructure/database/database_build_provider.h"
#include "infrastructure/network/update_service.h"
#include "infrastructure/platform/platform_services.h"
#include "infrastructure/settings/preference_store.h"
#include "infrastructure/settings/search_settings.h"
#include "infrastructure/io/product_files.h"
#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "infrastructure/settings/database_catalog_store.h"
#include "gui/mainwindow.h"
#include "gui/widgets/quantwidget.h"
#include "gui/widgets/dbresultswidget.h"
#include "gui/presentation/product_plot_refresh.h"
#include "gui/search/search_request.h"
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include "infrastructure/settings/database_settings.h"
#include "infrastructure/settings/progkeysettings.h"
#include <QApplication>
#include <QTableView>
#include <QTextBrowser>
#include <QTextBlock>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include "classic_fixture.h"
#include "qualpf/application/result_management.h"
#include "infrastructure/serialization/quantification.h"
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
using namespace qualpf::gui;
namespace {
void check(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void isolation() {
    infrastructure::QtPreferenceStore preferencesStore;
    infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    infrastructure::QtUpdateServiceProvider updates;
    infrastructure::QtPlatformServices platform;
    infrastructure::ProductFileProviders files;
    Workspace first, second;
    auto factory = std::make_shared<infrastructure::SqlCandidateReaderFactory>();
    infrastructure::DatabaseCatalogSettings settings;
    infrastructure::SqlReferenceRepository firstRepository, secondRepository;
    DatabaseCatalog firstCatalog(first.searches(), settings, firstRepository), secondCatalog(second.searches(), settings, secondRepository);
    WorkspaceContext left(first, firstCatalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, factory), right(second, secondCatalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, factory);
    left.presentation().configure(PresentationMode::automatic_gui);
    right.presentation().configure(PresentationMode::automatic_gui);
    const auto beforeLeft = first.session().snapshot(), beforeRight = second.session().snapshot();
    {
        MainWindow one(left), two(right);
        check(&workspaceContext(one.xpdViewer()) == &left && &workspaceContext(two.xpdViewer()) == &right,
              "Designer children must resolve their own root context");
        auto next = *beforeLeft;
        next.projectMetadata["context_marker"] = std::int64_t{17};
        check(bool(first.session().commit(beforeLeft, std::move(next))), "isolated state commit");
        check(first.session().snapshot() != beforeLeft && second.session().snapshot() == beforeRight,
              "one workspace commit must not change another canonical owner");
        check(left.searchSession().snapshot()->canonical == first.session().snapshot() &&
                  right.searchSession().snapshot()->canonical == beforeRight,
              "Qt views must derive from their own canonical state");
        auto *results = one.findChild<DbResultsWidget *>();
        check(results, "real candidate widget");
        check(bool(qualpf::gui::presentation::request_product_plot_refresh(left)) &&
                  bool(qualpf::gui::presentation::request_product_plot_refresh(right)),
              "instance presentation binding");
        testing::ClassicFixture fixture;
        auto scientific = *first.session().snapshot();
        scientific.pattern = fixture.pattern;
        scientific.pattern.radiation.lines = {{WavelengthAngstrom{1.54}, 1}};
        auto peaks = fixture.peaks;
        peaks.effective_wavelength = WavelengthAngstrom{1.54};
        scientific.peaks = productMeasuredPeaks(peaks).value();
        check(bool(first.session().commit(first.session().snapshot(), scientific)), "real scoring inputs");
        ProductCard low, high;
        low.id = "low"; low.fom = .25; low.fomCalculated = true; low.scale = 5;
        for (const auto &reflection : fixture.card.reflections) {
            low.tth.push_back(reflection.two_theta->value);
            low.intensity.push_back(reflection.intensity);
        }
        low.d = scientific.peaks.d;
        high = low; high.id = "high"; high.fom = .75; high.scale = 3;
        first.session().replaceCandidates({low, high});
        results->refresh();
        const auto rankingSource = first.session().snapshot();
        check(QMetaObject::invokeMethod(results, "onHeaderSectionClicked", Q_ARG(int, 8)) &&
                  QMetaObject::invokeMethod(results, "onHeaderSectionClicked", Q_ARG(int, 8)),
              "exercise FOM header sorting");
        check(first.session().snapshot() == rankingSource &&
                  first.session().snapshot()->candidates.front().id == "low" &&
                  results->displayCards().front().getId() == "high",
              "header sorting changes only display, preserving snapshot, revision and formal order");
        auto rescored = SearchEngine{}.rescore(*rankingSource, fixture.parameters, fixture.context);
        check(rescored && rescored.value().size() == 2 && rescored.value()[0].id == "low" &&
              rescored.value()[1].id == "high" && rescored.value()[0].scale == 5,
              "display sorting cannot change stable ties or previous scales in a later rescore");
        QTemporaryDir projectDirectory;check(projectDirectory.isValid(),"project directory");
        const auto projectPath=(projectDirectory.path()+"/ranking.qpf").toStdString();
        auto project=saveProjectInput(first,files.projectWriter,projectPath);
        check(bool(project),"QPF formal candidate order save");
        auto restored=files.projects.read(projectPath,SearchBackend::cpp);
        check(restored && restored.value().state.candidates.size()==2 && restored.value().state.candidates[0].id=="low" && restored.value().state.candidates[1].id=="high","QPF saves formal candidate order");
        QObject::disconnect(results, nullptr, &one, nullptr);
        QVector<CardType> accepted;
        QObject::connect(results, &DbResultsWidget::phasesAccepted, results, [&](const auto &cards) { accepted = cards; });
        auto *table = results->findChild<QTableView *>();
        check(table, "result table"); table->selectAll(); results->acceptSelectedCards();
        check(accepted.size() == 2 && accepted[0].getId() == "high" && accepted[1].getId() == "low" &&
              first.session().snapshot() == rankingSource, "batch acceptance preserves the explicit selected display order");
        auto intent = phaseAcceptanceInput(rankingSource, accepted[0].productValue().id, fixture.parameters, true);
        check(intent && intent.value() && intent.value()->phase->id == "high" && intent.value()->phase->scale == 3,
              "phase selection resolves canonical identity and scale");
        auto duplicateSource = *rankingSource; duplicateSource.phases = {high};
        auto duplicate = phaseAcceptanceInput(std::make_shared<const ProductState>(duplicateSource), "high", fixture.parameters, true);
        check(duplicate && !duplicate.value(), "duplicate identity creates no acceptance intent");
        auto middle = low; middle.id = "middle"; middle.fom = .5;
        results->mergeResults({CardType(high), CardType(middle), CardType(middle)});
        auto tail = low; tail.id = "tail"; tail.fom = .1; results->addCard(CardType(tail));
        auto canonical = first.session().snapshot();
        check(canonical->candidates.size() == 4 && canonical->candidates[0].id == "low" &&
              canonical->candidates[1].id == "high" && canonical->candidates[2].id == "middle" &&
              canonical->candidates[3].id == "tail", "merge and add retain canonical order and reject duplicate identities");
        results->selectCard("high"); results->deleteSelectedCards();
        canonical = first.session().snapshot();
        check(canonical->candidates.size() == 3 && canonical->candidates[0].id == "low" &&
              canonical->candidates[1].id == "middle" && canonical->candidates[2].id == "tail",
              "sorted selection deletes the right identity without reordering survivors");
        ProductCard phase;
        phase.id = "A";
        phase.intensity = {100};
        phase.d = {2};
        phase.scale = 2;
        phase.rir = "2";
        phase.fomCalculated = true;
        auto other = phase;
        other.id = "B";
        other.scale = 3; other.rir = "1";
        first.session().replacePhases({phase, other});
        auto *quantification = one.findChild<QuantWidget *>();
        check(quantification, "real quantification widget");
        quantification->refresh();
        check(quantification->quantPercentages() == QVector<double>({25, 75}) &&
                  second.session().snapshot() == beforeRight,
              "widget consumes fixed RIR from its canonical snapshot");
        auto saved=saveProjectInput(first,files.projectWriter,projectPath);
        auto quantitativeSource = first.session().snapshot();
        auto prepared = prepareProjectSerialization(quantitativeSource, infrastructure::quantificationInputs(quantitativeSource->phases));
        check(saved && prepared && prepared.value().source() == quantitativeSource && prepared.value().percentages() == std::vector<double>({25,75}),"Widget and QPF share quantitative results");
        restored=files.projects.read(projectPath,SearchBackend::cpp);
        check(restored && prepareQuantification(std::make_shared<const ProductState>(restored.value().state),infrastructure::quantificationInputs(restored.value().state.phases)).percentages()==std::vector<double>({25,75}),"QPF preserves fixed quantitative output");

        auto graphView=one.xpdViewer()->captureProjectView(*first.projectView());
        graphView.axis="inverse_d_squared";graphView.bounds=std::array{.01,1.,.5,200.};
        graphView.curves["observed/0"].pen.rgba=0x123456ff;graphView.curves["observed/0"].pen.width=2.5;
        graphView.card_colors["A"]=0xabcdef80;graphView.result_order={"tail","middle","low"};
        check(bool(first.updateProjectView(graphView)),"project graph intent");
        check(bool(saveProjectInput(first,files.projectWriter,projectPath)),"save graph project");
        check(one.loadProject(QString::fromStdString(projectPath)),"load graph project through real window");
        check(one.xpdViewer()->pSettings().getAbscissa()==xpdutils::ONE_OVER_DVALUE2 && one.xpdViewer()->xAxis->range().lower==.01 && one.xpdViewer()->xAxis->range().upper==1. && one.xpdViewer()->yAxis->range().lower==.5,"restore axis and viewport");
        check(!one.xpdViewer()->obs.empty() && one.xpdViewer()->obs.front().getPen().color()==QColor(0x12,0x34,0x56) && one.xpdViewer()->obs.front().getPen().widthF()==2.5,"restore graph styles");
        check(cardColor("A",&one)==QColor(0xab,0xcd,0xef,0x80) && cardColor("A",&two)!=cardColor("A",&one),"card colors belong to one project");
        check(results->displayCards().front().getId()=="tail" && first.session().snapshot()->candidates.front().id=="low","restore display order without changing scientific order");
        const auto graphSource=first.session().snapshot();const auto viewSource=first.projectView();const auto projectTitle=one.windowTitle();
        const auto legacyPath=projectDirectory.path()+"/legacy.qxp";QFile old(legacyPath);check(old.open(QIODevice::WriteOnly),"write old format input");old.write("{\"version\":3}");old.close();
        check(!one.loadProject(legacyPath) && first.session().snapshot()==graphSource && first.projectView()==viewSource && one.windowTitle()==projectTitle,"old QXP rejection is atomic in the real window");

    }
    check(!qualpf::gui::presentation::request_product_plot_refresh(left) &&
              !qualpf::gui::presentation::request_product_plot_refresh(right),
          "window destruction unbinds presentation");
    auto persisted=*first.session().snapshot();persisted.analysis_database="stable-database";persisted.restored_project=true;
    persisted.projectStateExtensions=MetadataObject{{"unknown",std::int64_t{9007199254740993LL}}};
    const auto projection=searchView(std::make_shared<const ProductState>(persisted));
    const auto converted=productState(projection);
    check(converted.analysis_database==persisted.analysis_database && converted.restored_project &&
          converted.projectStateExtensions==persisted.projectStateExtensions,"GUI projections preserve persistence admission and unknown fields");
    int observed = 0;
    {
        SessionView extra(first.session());
        QObject::connect(&extra, &SessionView::changed, &extra, [&] { ++observed; });
        auto source = first.session().snapshot();
        auto next = *source;
        next.projectMetadata["context_marker"] = std::int64_t{18};
        check(bool(first.session().commit(source, std::move(next))) && observed == 1, "view subscription");
    }
    auto source = first.session().snapshot();
    auto next = *source;
    next.projectMetadata["context_marker"] = std::int64_t{19};
    check(bool(first.session().commit(source, std::move(next))) && observed == 1,
          "destroyed view subscription cannot be called");
    QObject orphan;
    bool rejected = false;
    try {
        workspaceContext(&orphan);
    } catch (const std::logic_error &) {
        rejected = true;
    }
    check(rejected, "an unbound object cannot silently acquire a global workspace");
}
void presenterLifetime() {
    infrastructure::QtPreferenceStore preferencesStore;
    infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    infrastructure::QtUpdateServiceProvider updates;
    infrastructure::QtPlatformServices platform;
    infrastructure::ProductFileProviders files;
    Workspace product;
    infrastructure::DatabaseCatalogSettings settings;
    infrastructure::SqlReferenceRepository repository;
    DatabaseCatalog catalog(product.searches(), settings, repository);
    WorkspaceContext context(product, catalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, std::make_shared<infrastructure::SqlCandidateReaderFactory>());
    context.presentation().configure(PresentationMode::automatic_gui);
    int calls = 0;
    {
        QObject owner, foreign;
        context.bindPresenter(&owner, [&] {
            ++calls;
            return Status{std::monostate{}};
        });
        context.unbindPresenter(&foreign);
        check(bool(context.refreshPresentation()) && calls == 1, "foreign owner cannot unbind a presenter");
    }
    check(!context.refreshPresentation() && calls == 1, "expired QObject presenter is never invoked");
    QObject owner;
    context.bindPresenter(&owner, [&] {
        ++calls;
        return Status{std::monostate{}};
    });
    product.searches().close();
    const auto closing = context.refreshPresentation();
    check(!closing && closing.error().reason == "closing" && calls == 1, "Closing rejects presentation");
}
void cardDetailsPresentation() {
    struct Repository final : ReferenceRepository {
        application::CardDetails card;
        bool open(const std::string &) override { return true; }
        void close() override {}
        std::pair<int, std::string> information() override { return {1, "COD"}; }
        application::CardDetails details(const std::string &) const override { return card; }
        NamedCounts spaceGroups() const override { return {}; }
        NamedCounts colors() const override { return {}; }
        Result<std::vector<ProductCard>> read(const CandidateQuery &, const ExecutionContext &,
                                              SearchProgress) override {
            return Result<std::vector<ProductCard>>{std::vector<ProductCard>{}};
        }
    } repository;
    repository.card.valid = true;
    repository.card.id = "007'\"><&/A";
    repository.card.name = "<b>oxide</b> & hydrate";
    repository.card.mineralName = "<i>mineral</i>";
    repository.card.chemicalFormula = "A<B & C";
    repository.card.spaceGroup = "P < 1";
    repository.card.quality = "A & B";
    repository.card.rir = "1<2";
    repository.card.color = "<font color='red'>blue</font>";
    repository.card.type = "type &alpha;";
    repository.card.authors = "A <a href='https://invalid.test/'>link</a>";
    repository.card.journal = "<b>Journal</b>";
    repository.card.journalVolume = "<i>3</i>";
    repository.card.journalYear = 2026;
    repository.card.pageStart = "<10";
    repository.card.pageEnd = "20>";
    infrastructure::QtPreferenceStore preferences;
    infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    DatabaseBuildUseCases databaseBuild(databaseBuildProvider);
    infrastructure::QtUpdateServiceProvider updates;
    infrastructure::QtPlatformServices platform;
    infrastructure::ProductFileProviders files;
    infrastructure::DatabaseCatalogSettings settings;
    Workspace workspace;
    DatabaseCatalog catalog(workspace.searches(), settings, repository);
    WorkspaceContext context(workspace, catalog, files.ports, preferences, searchSettings, platform, updates,
                             databaseBuild, std::make_shared<infrastructure::SqlCandidateReaderFactory>());
    context.presentation().configure(PresentationMode::automatic_gui);
    MainWindow window(context);
    auto *results = window.findChild<DbResultsWidget *>();
    auto *browser = window.findChild<QTextBrowser *>("cardBrowser");
    check(results && browser, "real card details presentation");
    const auto source = workspace.session().snapshot();
    const auto &card = repository.card;
    results->cardSelected(QString::fromStdString(card.id));
    const auto text = browser->toPlainText();
    for (const auto *value : {&card.id, &card.name, &card.mineralName, &card.chemicalFormula, &card.spaceGroup,
                              &card.quality, &card.rir, &card.color, &card.type, &card.authors, &card.journal,
                              &card.journalVolume, &card.pageStart, &card.pageEnd})
        check(text.contains(QString::fromStdString(*value)), "card metadata must display as literal text");
    const QString expectedUrl = "https://www.crystallography.net/cod/007%27%22%3E%3C%26%2FA.html";
    bool foundLink = false;
    for (auto block = browser->document()->begin(); block.isValid(); block = block.next())
        for (auto it = block.begin(); !it.atEnd(); ++it) {
            const auto format = it.fragment().charFormat();
            if (format.isAnchor()) {
                check(format.anchorHref() == expectedUrl, "only the encoded COD entry link may be active");
                foundLink = true;
            }
        }
    check(foundLink && QUrl(expectedUrl).host() == "www.crystallography.net", "COD link retains its destination");
    check(workspace.session().snapshot() == source, "card details do not mutate canonical state");
}
void selectionAndRequest(const QString &directory) {
    QSettings preferences;
    preferences.setValue(DB_COUNT_KEY, 17);
    infrastructure::QtPreferenceStore preferencesStore;
    infrastructure::settings::SearchSettings searchSettings;
    infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    infrastructure::QtUpdateServiceProvider updates;
    infrastructure::QtPlatformServices platform;
    infrastructure::ProductFileProviders files;
    Workspace product;
    infrastructure::DatabaseCatalogSettings settings;
    infrastructure::SqlReferenceRepository repository;
    DatabaseCatalog catalog(product.searches(), settings, repository, std::numeric_limits<std::uint64_t>::max());
    WorkspaceContext exhausted(product, catalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, std::make_shared<infrastructure::SqlCandidateReaderFactory>());
    gui::DatabaseEntry entry;
    entry.inUse = true;
    entry.name = "independent database";
    entry.path = directory + "/must-not-open";
    const auto source = product.session().snapshot();
    auto failed = exhausted.setDatabases({entry});
    check(!failed && failed.error().reason == "revision_exhausted", "bounded database selection epoch");
    check(exhausted.databases().isEmpty() && exhausted.referenceDatabase().basePath.empty() &&
              exhausted.referenceDatabase().selectionVersion == std::numeric_limits<std::uint64_t>::max() &&
              preferences.value(DB_COUNT_KEY).toInt() == 17 && !QFileInfo::exists(entry.path + ".sq") &&
              product.session().snapshot() == source,
          "exhaustion cannot partly change settings, SQL or Session");
    Workspace active;
    infrastructure::SqlReferenceRepository activeRepository;
    DatabaseCatalog activeCatalog(active.searches(), settings, activeRepository);
    WorkspaceContext context(active, activeCatalog, files.ports, preferencesStore, searchSettings, platform, updates, databaseBuild, std::make_shared<infrastructure::SqlCandidateReaderFactory>());
    auto task = active.searches().begin(context.referenceDatabase());
    check(bool(task), "active selection fixture admission");
    failed = context.setDatabases({entry});
    check(!failed && active.searches().cancelled() && context.databases().isEmpty() &&
              preferences.value(DB_COUNT_KEY).toInt() == 17,
          "Busy selection cancels without partial persistence");
    auto retired = active.searches().complete(
        task.value().id, Result<PreparedSearchResult>{Error{ErrorCode::cancelled, "test", "cancelled"}});
    check(!retired && !active.searches().running(), "cancelled fixture retires");
    SearchRequest request;
    check(request.getMinFom() == .35 && request.getWeight2thetaD() == .50 &&
              request.getWeightIntensity() == .50 && request.getWeightPhases() == .50 &&
              request.getDelta2theta() == .08 && !request.getCalcFom(),
          "existing boundary defaults");
    request.setCalcFom(true);
    request.setMinFom(.7);
    request.setElements("Si O");
    auto query = request.candidateQuery(CandidateQuery::Route::matching);
    check(query.decodeReflections && query.elements == "Si O", "SQL gets data projection and filters");
    request.initialize();
    check(request.getMinFom() == .35 && !request.getCalcFom() && !request.decodeReflections(),
          "request reset preserves existing scoring and projection defaults");
}
void removalAdmission(const QString &directory) {
    Workspace workspace;
    infrastructure::DatabaseCatalogSettings store;
    infrastructure::SqlReferenceRepository repository;
    DatabaseCatalog catalog(workspace.searches(), store, repository);
    const auto base = directory + "/remove-fixture";
    for (const auto *suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"}) {
        QFile file(base + suffix);
        check(file.open(QIODevice::WriteOnly), "owned database removal fixture");
        file.write("fixture");
    }
    std::vector<ReferenceDatabaseEntry> entries{{true, "remove", 1, "fixture", base.toStdString()},
                                               {false, "remaining", 2, "fixture", (directory + "/remaining").toStdString()}};
    store.save(entries);
    const auto source = workspace.session().snapshot();
    auto read = infrastructure::DatabaseLease::read(base);
    check(bool(read), "removal read admission fixture");
    auto busy = catalog.removeDraft(entries, 0, 0, true);
    check(!busy && busy.error().reason == "database_busy" && QFileInfo::exists(base + ".sq") &&
          QSettings{}.value(DB_COUNT_KEY).toInt() == 2 && workspace.session().snapshot() == source,
          "busy removal has no file, settings or product mutation");
    read.reset();
    bool observed = false;
    auto removed = catalog.removeDraft(entries, 0, 0, true, [&](const auto &result) {
        observed = true;
        check(!infrastructure::DatabaseLease::write(base) && QSettings{}.value(DB_COUNT_KEY).toInt() == 2 && result.entries.size() == 1,
              "write lease spans pre-persistence failure presentation with original settings timing");
    });
    check(observed, "single removal presentation");
    check(bool(removed) && removed.value().activeIndex == 0 && removed.value().entries.size() == 1 &&
          removed.value().entries[0].inUse && removed.value().failedFiles.empty() && QSettings{}.value(DB_COUNT_KEY).toInt() == 1,
          "draft removal reassigns active entry and persists under lease");
    for (const auto *suffix : {".sq", ".sq.info", ".sq.infostat", ".sq.search"})
        check(!QFileInfo::exists(base + suffix), "all original database files removed");
    check(catalog.entries().empty() && workspace.session().snapshot() == source,
          "dialog draft removal does not publish a canonical selection");
}
} // namespace
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid())
        return 2;
    QCoreApplication::setOrganizationName("QualPF architecture tests");
    QCoreApplication::setApplicationName("workspace-context");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory.path());
    try {
        isolation();
        presenterLifetime();
        cardDetailsPresentation();
        selectionAndRequest(directory.path());
        removalAdmission(directory.path());
        std::cout << "PASS: explicit workspace ownership, isolation, subscription and bounded selection\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
