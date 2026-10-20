#include "gui/mainwindow.h"
#include "gui/dialogs/workflow_settings_dialog.h"
#include "gui/support/workflow_confirmation.h"
#include "gui/widgets/workflow_dock.h"
#include "qualpf/application/preprocessing_evaluation.h"
#include "qualpf/application/workflow.h"
#include "ui_mainwindow.h"
#include <QMenuBar>
using namespace qualpf;
using namespace qualpf::application;
void MainWindow::createWorkflowUi() {
    auto *menu=new QMenu(tr("Setting"),this);menu->setObjectName("menuSetting");
    menuBar()->insertMenu(ui->menuHelp->menuAction(),menu);
    for(auto flow:workflows) {
        auto *action=menu->addAction(QString::fromUtf8(workflowName(flow))+"...");action->setObjectName("settingWorkflow"+QString::number(int(flow)));
        connect(action,&QAction::triggered,this,[this,flow]{openWorkflowDefaults(flow);});
    }
    workflowDock_=new qualpf::gui::WorkflowDock(context().workspace(),this);addDockWidget(Qt::RightDockWidgetArea,workflowDock_);
    workflowDock_->hide();ui->menuView->addAction(workflowDock_->toggleViewAction());
    connect(workflowDock_,&qualpf::gui::WorkflowDock::resultsChanged,this,[this]{
        markProjectModified();refreshPatternPresentation();refreshPreprocessingDisplay();publishSearchView(false);
    });
    connect(searchTasks_,&qualpf::gui::SearchTaskController::started,workflowDock_,[this]{workflowDock_->refresh();});
    connect(searchTasks_,&qualpf::gui::SearchTaskController::workflowProgress,workflowDock_,[this]{workflowDock_->refresh();});
    connect(searchTasks_,&qualpf::gui::SearchTaskController::finished,workflowDock_,[this]{workflowDock_->refresh();});
}
void MainWindow::openWorkflowDefaults(WorkflowId flow) {
    if(closingRequested_||searchTasks_->busy()||context().workspace().session().snapshot()->editing)return;
    auto *dialog=new qualpf::gui::WorkflowSettingsDialog(context().workspace(),this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if(!dialog->load(flow)){delete dialog;return;}
    dialog->show();
}
Status MainWindow::applyProjectOptions(ProductHandle source,const ProjectSettings &settings) {
    auto change=prepareSettingsChange(source,settings);
    if(!change)return Status{change.error()};
    if(!qualpf::gui::confirmWorkflowImpact(this,source,change.value().affected,tr("Save changed project parameters?")))
        return Status{Error{ErrorCode::cancelled,"settings","Project options were not changed"}};
    auto status=context().workspace().applySettings(change.value());
    if(status) {
        markProjectModified();refreshPatternPresentation();refreshPreprocessingDisplay();publishSearchView(false);updateFwhmActions();
    }
    return status;
}
bool MainWindow::confirmWorkflowReplacement(WorkflowStep step) {
    const auto source=context().workspace().session().snapshot();
    const auto *record=currentWorkflowRecord(source->workflow,step);if(!record)return true;
    const auto affected=workflowDependents(source->workflow,record->id);
    if(affected.size()<2)return true;
    return qualpf::gui::confirmWorkflowImpact(this,source,affected,
        tr("Replace %1?").arg(QString::fromUtf8(workflowStepName(step))));
}
void MainWindow::runWorkflowPreparation(bool background) {
    const auto source=context().workspace().session().snapshot();
    auto request=automaticPreprocessingRequest(source,background?PreprocessingAction::generate_background:PreprocessingAction::automatic_peaks);
    if(!request){setStatusMessage(QString::fromStdString(request.error().message));return;}
    auto status=searchTasks_->submit(context().referenceDatabase(),[source,request=request.value()](const SearchTask &task,SearchProgress){
        using Outcome=qualpf::gui::SearchTaskController::Outcome;
        PreprocessingEvaluation evaluation(source);auto status=evaluation.preview(request,task.context.cancellation);
        if(!status)return Outcome{status.error()};
        return Outcome{PreparedSearchResult{source,task.id,task.database.selectionVersion,*evaluation.preview()}};
    },[this,background](auto outcome){
        if(closingRequested_)return;
        if(!outcome){setStatusMessage(QString::fromStdString(outcome.error().message));return;}
        if(background)setBackgroundSubtractionDisplay(false);
        markProjectModified();refreshPatternPresentation();refreshPreprocessingDisplay();updateBackgroundActions();updateReport();
    },{},SearchPublication::commit,{},WorkflowTarget{background?WorkflowId::preprocessing:WorkflowId::peak_search,
        background?WorkflowStep::background:WorkflowStep::peaks});
    if(!status)setStatusMessage(QString::fromStdString(status.error().message));
}
