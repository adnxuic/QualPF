#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "gui/dialogs/plotstyledialog.h"
#include "gui/dialogs/peaksearchdialog.h"
#include "gui/dialogs/backgrounddialog.h"
#include "gui/dialogs/restraintsdialog.h"
#include "gui/search/search_request.h"
#include "gui/model/cardtype.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/dialogs/managedatabasesdialog.h"
#include "gui/search/search_options.h"
#include "qualpf/application/startup/startup_options.h"
#include "gui/dialogs/aboutdialog.h"
#include "gui/dialogs/smoothingdialog.h"
#include "gui/dialogs/rangedialog.h"

#include <QMainWindow>
#include "gui/state/workspace_context.h"
#include "gui/search/search_task_controller.h"

typedef struct {
    QString fileName;
    QString fileType;
} RecentFileInfo;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    qualpf::gui::WorkspaceContext &context_;
    Q_OBJECT
    friend class ProductWorkflowProbe;

public:
    enum MouseAction {
        NoZoom,
        HorizontalZoom,
        RectangleZoom,
        Pan,
        AddBackgroundPoint,
        DeleteBackgroundPoint,
        AddPeak,
        DeletePeak,
        NoAction,
        AddInferredPeak
    };
    Q_ENUM(MouseAction)

    enum RecentFileType {Input, Data, Structure, Project, LegacyPowderCif};
    Q_ENUM(RecentFileType)

    enum EnabledActions {
        InitAction,
        PatternAction,
        StructureAction,
        PatternAndStructureAction,
        DialogOpenAction,
        RunAction,
        RunSkipAction,
        NextAction,
        PhaseAction,
        SaveAction,
        RestoreAction,
        IntervalsAction,
        ExtraAction,
        ExtraBackgroundAction,
        ProfileAction,
        CycleAction,
        PeaksAction
    };
    Q_ENUM(EnabledActions)

    MainWindow(qualpf::gui::WorkspaceContext &context, QWidget *parent = nullptr,
               std::shared_ptr<qualpf::application::CandidateReaderFactory> factory = {},
               qualpf::gui::SearchTaskController::Trace trace = {});
    ~MainWindow();
    qualpf::gui::WorkspaceContext &context() const { return context_; }
    XpdViewWidget *xpdViewer() const;
    void setAction(const MouseAction &action, bool writeConfig = true);
    void setCurrentFile(const QString &fullFileName, const QString &fileType);
    void setStatusMessage(const QString &message);
    void clearStatusMessage();
    QProgressBar *getStatusProgressBar() const;
    void runSearch(const SearchOptions &opts);
    void runSearchMatch();
    qualpf::gui::SearchTaskController *searchTasks() const { return searchTasks_; }
    bool isClosing() const { return closingRequested_; }
    bool loadDiffractionPatterns(QStringList files, bool chooseRadiation = true, bool legacyCif = false);
    bool loadPowderCif(const QString &file);
    void updatePeakListTable();
    void refreshPreprocessingDisplay();
    bool refreshPatternPresentation();
    void checkAction(MouseAction action);
    void saveAction();
    void restoreAction();
    void setZoomAction();
    void enableActions(MainWindow::EnabledActions action, bool state=false);
    void saveEnabledActions();
    void restoreEnabledActions();
    void enableMenu(QMenu *menu, bool enab);

    static QString getPathDataFiles();
    static void setPathDataFiles(const QString &newPathDataFiles);

private slots:
    void closeEvent(QCloseEvent *event);
    void zoomGroupTriggered(QAction *action);
    void plotStyleClosed(QDialogButtonBox::StandardButton button);

    //File
    void onActionNewTriggered();
    void openRecentFile();
    void onActionImportDiffractionPatternTriggered();
    void onActionLoadProjectTriggered();
    void onActionSaveProjectTriggered();
    void onActionSaveProjectAsTriggered();
    void onActionExportDiffractionPattern();
    void onActionImagePowderPatternTriggered();
    void onActionPrintTriggered();
    void onActionFileDropped(const QStringList &fileList);

    //Pattern
    void onActionRangeTriggered();
    void onActionBackgroundTriggered();
    void onActionBackgroundExportTriggered();
    void onActionSubtractBackgroundTriggered();
    void onActionSmoothingTriggered();
    void onActionKAlpha2StrippingTriggered();
    void onActionPeakSearchTriggered();
    void onActionPeakSearchConditionsTriggered();
    void onActionLoadPeaksTriggered();
    void onActionSavePeaksTriggered();

    //View
    void onActionPlotStyleTriggered();
    void onActionResetZoomTriggered();
    void onActionAutoscaleTriggered();

    //Search
    void onActionSearchMatchTriggered();
    void onActionSearchMatchOptionsTriggered();
    void actionRestraintsTriggered();
    void actionManageDatabasesTriggered();
    void onActionCancelSearchTriggered();
    void onActionTestDatabaseTriggered();

    //Entry
    void onActionLoadAddTriggered();
    void onActionRecalculateFomTriggered();

    //Window
    void onActionDefaultLayoutTriggered();

    //Help
    void onActionDocumentationHtmlTriggered();
    void onActionDocumentationPdfTriggered();
    void onActionCheckForUpdatesTriggered();
    void onActionAboutTriggered();

private:
    qualpf::gui::SearchTaskController *searchTasks_ = nullptr;
    bool searchUiActive_ = false, closingRequested_ = false;
    quint64 searchUiTask_ = 0;
    bool searchAdmission();
    void beginMatch(bool restraintsOnly = false);
    void publishSearchView(bool browse);
    void acceptPhases(QVector<CardType>, int index = 0);
    void submitSearch(qualpf::application::SearchInput, std::function<void(const qualpf::application::PreparedSearchResult &)>);
    void createDialogs();
    void createActionGroup();
    void actionsSetup();
    void updateReport();
    void writeSettings();
    void readSettings();
    void readAction();
    void plotStyleConfig();
    void deleteSelectedPeaks(const QVector<int> &selected);
    void addDeleteSelectedPoint(int action, double xp, double yp, int &ier);

    Ui::MainWindow *ui;
    QLabel *statusLabel1;
    QProgressBar *statusProgressBar;
    MouseAction mAction;
    MouseAction savedAction;
    MouseAction savedZoomAction;
    QMap<QAction *, bool> stateActions;
    void enumerateEnabledActionsMenu(QMenu *menu);

    //Dialog Windows
    PeakSearchDialog  *peakSearchDialog  = nullptr;
    PlotStyleDialog *plotStyleDialog = nullptr;
    BackgroundDialog  *backgroundDialog  = nullptr;
    RestraintsDialog  *m_restraintsDialog = nullptr;
    SmoothingDialog *smoothingDialog = nullptr;
    AboutDialog *aboutDialog = nullptr;
    QPointer<RangeDialog> rangeDialog;

    //Files
    QString currentFile;
    QString currentProjectFile;
    bool projectFileSaved;
    void clearProjectFile();
    void markProjectModified();
    static QString pathDataFiles;
    enum { maxFileNr = 10 };
    QList<QAction*> recentFileActionList;
    void saveProject(const QString &fileName);
    void createRecentActions();
    void updateRecentFileActions();
    void setRecentFiles(const QString &fullFileName, const QString &fileType);
    QByteArray defaultState;

    void testSelection(qualpf::gui::SearchRequest &builder, int testCase);
    void executeSearch(qualpf::gui::SearchRequest &builder, bool merge = false);
    void onRestraintsSearch(bool merge);
    void onRestraintsSearchMatch();
    void onCardSelected(const QString &id);
    void applyDialogRestraints(qualpf::gui::SearchRequest &builder);
    void performResidualSearch(const CardType &acceptedCard);
    void recalculateFOMs(const QString &statusSuffix = QString());

    void loadProject(QString fileName);
};
#endif // MAINWINDOW_H
