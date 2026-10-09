#include "infrastructure/database/database_build_provider.h"
#include "infrastructure/network/update_service.h"
#include "infrastructure/platform/platform_services.h"
#include "infrastructure/settings/preference_store.h"
#include "infrastructure/settings/search_settings.h"
#include "infrastructure/database/qt_sql/sql_reference_repository.h"
#include "infrastructure/settings/database_catalog_store.h"
#include "infrastructure/io/product_files.h"
#include "infrastructure/database/qt_sql/candidate_reader_factory.h"
#include "gui/search/search_request.h"
#include "infrastructure/database/cif_database_product.h"
#include "infrastructure/database/cif_database_build.h"
using namespace qualpf::infrastructure;
#include "gui/state/pattern_input.h"
#include "qualpf/application/startup/application_initialization_service.h"
#include "gui/mainwindow.h"
#include "apps/qualpf/commandline.h"
#include "gui/support/qt_utils.h"
#include "infrastructure/resources/fileutils.h"
#include "infrastructure/settings/settings_migration.h"
#include "gui/database/databasebuilder.h"
#include "gui/reference/libcomune.h"
#include "gui/state/workspace_context.h"
#include "gui/search/searchmatch.h"
#include "gui/dialogs/searchoptionsdialog.h"
#if USE_CONFIG_H
#include "config.h"
#endif

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QScopeGuard>
#include <algorithm>
#include <csetjmp>
#include <csignal>



namespace {

jmp_buf qtInitEnv;

void onSigabrt(int)
{
    longjmp(qtInitEnv, 1);
}

// Tries to create a QApplication (needed for the GUI). If no display is
// available, Qt aborts while initializing the "xcb" platform plugin; that
// abort is trapped here and we fall back to a plain QCoreApplication so
// command-line usage (e.g. over SSH without X11 forwarding) keeps working.
QCoreApplication *loadQt(int &argc, char **argv)
{
    QCoreApplication *application = nullptr;

    bool gui = true;
    for (int i = 1; i < argc; ++i) {
        if (!qstrcmp(argv[i], "--nogui"))
            gui = false;
    }

    if (gui) {
        if (setjmp(qtInitEnv) == 0) {
            signal(SIGABRT, &onSigabrt);
            application = new QApplication(argc, argv);
        }
        signal(SIGABRT, SIG_DFL);
        if (!application)
            qWarning() << qPrintable(formattedMessage("The Display cannot be accessed !!!\n"
                                      "Program will go on without graphics", 50));
    }
    if (!application)
        application = new QCoreApplication(argc, argv);
    return application;
}

// Headless equivalent of Search > Match (MainWindow::onActionSearchMatchTriggered),
// used when --search is given together with a data file in --nogui mode, where no
// MainWindow (and thus no result widgets) exists. Prints the matching cards to stdout.
bool runSearchMatchCli(qualpf::gui::WorkspaceContext &context, const SearchOptions &searchopt)
{
    int npeaks = ensurePeaksFound(context);
    if (npeaks == 0) {
        fputs("Error: no peaks found. Check the data/peak-search parameters and try again.\n", stderr);
        return false;
    }

    loadExperimentalPeaks(context);
    const ExperimentalPeaks &ep = context.peaks();

    qualpf::gui::SearchRequest builder = buildSearchMatchQuery(context, ep);
    builder.enableDeleted(SearchOptionsDialog::savedCheckDeleted(context.workspace().session().snapshot(), context.settings()));
    if (!searchopt.composition.isEmpty()) {
        builder.setElements(searchopt.composition);
        if (searchopt.exactComposition)
            builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
        else if (searchopt.containsAny)
            builder.setBOperator(qualpf::gui::SearchRequest::JUST_OP);
    }
    printf("Searching database...\n");
    int lastProg = -1;
    auto progress = [&lastProg](int current, int total) {
        int prog = total > 0 ? (100 * current) / total : 0;
        if (prog != lastProg) {
            lastProg = prog;
            printf("\r  %3d%%", prog);
            fflush(stdout);
        }
    };

    QVector<CardType> acceptedCards;
    if (SearchOptionsDialog::savedCheckStrongest(context.workspace().session().snapshot(), context.settings()))
        context.searches().makeQueryStrongest(builder, acceptedCards, progress);
    else
        context.searches().makeQueryWithoutStrongest(builder, acceptedCards, progress);

    if (context.searches().isCanceled() || !context.searches().searchError().isEmpty()) {
        fprintf(stderr, "Search failed: %s\n", qPrintable(context.searches().searchError()));
        return false;
    }

    const int maxEntries = SearchOptionsDialog::savedMaxEntries(context.workspace().session().snapshot(), context.settings());
    if (acceptedCards.size() > maxEntries) {
        std::stable_sort(acceptedCards.begin(), acceptedCards.end(),
                  [](const CardType &a, const CardType &b) {
                      return a.getFom() > b.getFom();
                  });
        acceptedCards.resize(maxEntries);
    }

    context.searchSession().replaceCandidates(acceptedCards);
    printf("\rFound %d card(s):\n", (int)acceptedCards.size());
    for (const CardType &card : acceptedCards) {
        printf("  [%s] %-40s | %-50s | %s\n",
               qPrintable(card.getId()),
               qPrintable(card.getChemicalName()),
               qPrintable(card.getChemicalFormula()),
               qPrintable(card.getSpaceGroup()));
    }
    return true;
}

}

int runApplication(int argc, char *argv[])
{
    QScopedPointer<QCoreApplication> a(loadQt(argc, argv));
    bool isGuiApp = (qobject_cast<QApplication*>(QCoreApplication::instance()) != 0);

    if (isGuiApp) {
    QApplication::setWindowIcon(QIcon(":/images/images/qualx.png"));
    QGuiApplication::setApplicationDisplayName(APP_DISPLAY_NAME);
    }
    qApp->setApplicationVersion(APP_VERSION);
    qApp->setApplicationName(APP_NAME);
    qApp->setOrganizationName("IC");
    qApp->setOrganizationDomain("www.ba.ic.cnr.it/softwareic");

    QLocale locale = QLocale::system();
    if (locale.decimalPoint() == ',') {
        QLocale::setDefault(QLocale::c());
        setlocale(LC_ALL,"C");
    }

    qualpf::infrastructure::QtPlatformServices platform;
    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main","Software for qualitative phase analysis from powder diffraction data:"));
    QString errorMessage;
    qualpf::application::StartupOptions opt;
    DbBuildOptions dbopt;
    QString filein, fileout, testFolder;
    bool testApp;
    SearchOptions searchopt;
    switch (parseCommandLine(parser, filein, fileout, opt, dbopt, searchopt, errorMessage, testApp, testFolder)) {
    case CommandLineOk:
        break;
    case CommandLineError:
        fputs(qPrintable(errorMessage), stderr);
        fputs("\n\n", stderr);
        fputs(qPrintable(parser.helpText()), stderr);
        return 1;
    case CommandLineVersionRequested:
    {
        AppVersionInfo appVersion = getVersionInfo(platform);
        printf("%s Version %s\n%s\n%s\n%s\n",
               qPrintable(QCoreApplication::applicationName()),
               qPrintable(QCoreApplication::applicationVersion()),
               qPrintable(QString("Created on %1").arg(appVersion.data)),
               qPrintable(appVersion.compilersString),
               qPrintable(appVersion.qtVersionInfo));
    }
        return 0;
    case CommandLineHelpRequested:
        parser.showHelp();
        Q_UNREACHABLE();
    }

    // If no display was available, loadQt() already fell back to a
    // QCoreApplication: force nogui mode so we don't try to create a MainWindow.
    if (!isGuiApp)
        opt.nogui = 1;

    if (parser.isSet("settings-dir")) {
        const QString directory = QDir(parser.value("settings-dir")).absolutePath();
        if (parser.value("settings-dir").isEmpty() || !QDir().mkpath(directory)) {
            fputs("Error: invalid settings directory.\n", stderr);
            return EXIT_FAILURE;
        }
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory);
        QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, directory);
    }

    const auto migration = qualpf::infrastructure::migrateLegacyUserSettings();
    if (!migration.error.isEmpty())
        fprintf(stderr, "%s\n", qPrintable(migration.error));

    qualpf::infrastructure::QtPreferenceStore preferences;
    qualpf::infrastructure::settings::SearchSettings searchSettings;
    qualpf::infrastructure::QtDatabaseBuildProvider databaseBuildProvider;
    qualpf::application::DatabaseBuildUseCases databaseBuild{databaseBuildProvider};
    qualpf::infrastructure::QtUpdateServiceProvider updates;
    qualpf::infrastructure::ProductFileProviders files;
    qualpf::infrastructure::DatabaseCatalogSettings catalogSettings;
    qualpf::infrastructure::SqlReferenceRepository referenceRepository;
    qualpf::application::Workspace workspace(qEnvironmentVariable("QUALPF_SEARCH_BACKEND") == "legacy"
        ? qualpf::application::SearchBackend::legacy_fallback : qualpf::application::SearchBackend::cpp);
    qualpf::application::DatabaseCatalog catalog(workspace.searches(), catalogSettings, referenceRepository);
    qualpf::gui::WorkspaceContext context(workspace, catalog, files.ports, preferences, searchSettings, platform, updates, databaseBuild, std::make_shared<qualpf::infrastructure::SqlCandidateReaderFactory>());
    qualpf::application::ApplicationInitializationService initialization;
    using qualpf::application::PresentationMode;
    initialization.initialize(context.presentation(), opt.nogui ? PresentationMode::headless :
        opt.autom ? PresentationMode::automatic_gui : PresentationMode::interactive_gui);


    // Load databases only after early-exit cases (--version, --help, bad args).
    // Declared after 'a' so the scope guard is destroyed before QApplication,
    // ensuring closeDatabeses() is called while QCoreApplication still exists.
    // A new database does not depend on an existing active search database.
    // In particular, a fresh --nogui builder must not open the first-run GUI
    // database-location prompt through context.load().
    if (!dbopt.enabled)
        context.load();
    auto dbCleanup = qScopeGuard([&context] { context.closeDatabases(); });


    int ier = 0;
    if (dbopt.enabled) {
        if (dbopt.source == DbBuildOptions::Source::Pdf2) {
            printf("Reading PDF-2 file: %s\n", qPrintable(dbopt.pdf2File));
            if (!DatabaseBuilder::buildPdfDatabase(databaseBuild, dbopt.outputDb, dbopt.pdf2File,
                    [](int cards, qint64, qint64) {
                        printf("\r  %d cards processed...", cards);
                        fflush(stdout);
                    })) {
                fputs("Error: could not build PDF-2 database.\n", stderr);
                return 1;
            }
            printf("\nDone.\n");
            return 0;
        }

        {
            // CIF source
            if (dbopt.inorganic)
                printf("Scanning CIF folder (inorganic only): %s\n", qPrintable(dbopt.cifDir));
            else
                printf("Scanning CIF folder: %s\n", qPrintable(dbopt.cifDir));

            DatabaseBuilder::CifBuildStats stats;
            const bool built = DatabaseBuilder::buildCifDatabase(databaseBuild,
                    dbopt.outputDb, dbopt.cifDir, dbopt.recursive,
                    dbopt.inorganic,
                    [](int ok, int skip, int err, const QString &path) {
                        printf("\r  [%d ok / %d skip / %d err] %s",
                               ok, skip, err, qPrintable(path));
                        fflush(stdout);
                    },
                    &stats);
            // Qt logging in a Windows GUI-subsystem executable may go only to
            // the debugger. The CLI contract must always print full failures,
            // including every ambiguity candidate, to its stderr stream.
            for (const auto &failure : stats.report.failures)
                fprintf(stderr, "\n%s\n", qPrintable(qualpf::infrastructure::describe_cif_failure(failure)));
            if (stats.report.fatal)
                fprintf(stderr, "\n%s\n", qPrintable(qualpf::infrastructure::describe_cif_failure(*stats.report.fatal)));
            if (!built) {
                fputs("Error: could not build CIF database.\n", stderr);
                return 1;
            }
            if (dbopt.inorganic)
                printf("\nDone: %d ok, %d skipped (not inorganic), %d errors.\n",
                       stats.ok, stats.skipped, stats.errors);
            else
                printf("\nDone: %d ok, %d errors.\n", stats.ok, stats.errors);
        }
        return 0;
    } else if (opt.nogui) {
        if (!filein.isEmpty()) {
            auto loaded = qualpf::gui::openPatternFile(context.searchSession(), files.patterns,
                std::filesystem::path(filein.toStdWString()));
            if (!loaded) {
                fprintf(stderr, "Cannot open pattern: %s\n", loaded.error().message.c_str());
                return EXIT_FAILURE;
            }
        }

        if (searchopt.enabled) {
            if (!filein.isEmpty() && !ier) {
                // A data file was loaded: search & match against its experimental
                // peaks, as if the user had triggered Search > Match from the menu.
                if (!runSearchMatchCli(context, searchopt)) return EXIT_FAILURE;
            } else {
                qualpf::gui::SearchRequest builder;
                if (!searchopt.composition.isEmpty()) {
                    builder.setElements(searchopt.composition);
                    if (searchopt.exactComposition)
                        builder.setBOperator(qualpf::gui::SearchRequest::ONLY_OP);
                    else if (searchopt.containsAny)
                        builder.setBOperator(qualpf::gui::SearchRequest::JUST_OP);
                }
                printf("Searching database...\n");
                int lastProg = -1;
                auto progress = [&lastProg](int current, int total) {
                    int prog = total > 0 ? (100 * current) / total : 0;
                    if (prog != lastProg) {
                        lastProg = prog;
                        printf("\r  %3d%%", prog);
                        fflush(stdout);
                    }
                };

                QVector<CardType> cards = context.searches().makeQuery(builder, progress);
                if (context.searches().isCanceled() || !context.searches().searchError().isEmpty()) {
                    fprintf(stderr, "Search failed: %s\n", qPrintable(context.searches().searchError()));
                    return EXIT_FAILURE;
                }
                printf("\rFound %d card(s):\n", (int)cards.size());
                for (const CardType &card : cards) {
                    printf("  [%s] %-40s | %-50s | %s\n",
                           qPrintable(card.getId()),
                           qPrintable(card.getChemicalName()),
                           qPrintable(card.getChemicalFormula()),
                           qPrintable(card.getSpaceGroup()));
                }
            }
        }
    } else {
        MainWindow w(context);
        w.show();
        if (!ier && !filein.isEmpty() && !w.loadDiffractionPatterns({filein}, !opt.autom))
            ier = 1;
        if (!filein.isEmpty() && !ier)
            w.enableActions(MainWindow::PatternAction);
        if (searchopt.enabled) {
            if (!filein.isEmpty() && !ier)
                // A data file was loaded: search & match against its experimental
                // peaks, as if the user had triggered Search > Match from the menu.
                w.runSearchMatch();
            else
                w.runSearch(searchopt);
        }
        // Automatic GUI execution still dispatches queued worker completion.
        // Each accepted internal continuation keeps Busy; only final completion
        // ends the normal event loop before window dependencies leave scope.
        if (opt.autom) {
            if (w.searchTasks()->busy()) {
                QObject::connect(w.searchTasks(), &qualpf::gui::SearchTaskController::finished,
                    a.data(), [&w] { if (!w.searchTasks()->busy()) QCoreApplication::exit(0); });
                a->exec();
            }
            return ier ? EXIT_FAILURE : 0;
        }
        return a->exec();
    }
    return ier ? EXIT_FAILURE : EXIT_SUCCESS;
}

int main(int argc, char *argv[]) { return runApplication(argc, argv); }
