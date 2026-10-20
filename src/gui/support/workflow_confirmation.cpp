#include "gui/support/workflow_confirmation.h"
#include <QMessageBox>
#include <QPushButton>
#include <QStringList>
#include <algorithm>

namespace qualpf::gui {
using namespace application;
bool confirmWorkflowImpact(QWidget *parent,ProductHandle source,const std::vector<std::uint64_t> &affected,const QString &action) {
    if(affected.empty()) return true;
    QStringList names;bool analysis=false;
    for(const auto &r:source->workflow.records) if(std::find(affected.begin(),affected.end(),r.id)!=affected.end()) {
        names<<QString::fromUtf8(workflowStepName(r.step))+" · "+QString::fromUtf8(workflowName(r.workflow));
        analysis|=r.step==WorkflowStep::search||r.step==WorkflowStep::phases||r.step==WorkflowStep::rir;
    }
    QMessageBox box(QMessageBox::Warning,QObject::tr("Dependent results"),
        action+QObject::tr("\n\nThe following dependent results will also be removed:\n")+names.join("\n")+
        (analysis?QObject::tr("\n\nAccepted phases and quantitative results are also affected."):QString()),QMessageBox::Cancel,parent);
    box.setObjectName("workflowImpactConfirmation");
    auto *confirm=box.addButton(QObject::tr("Continue with All Listed"),QMessageBox::AcceptRole);
    box.setDefaultButton(QMessageBox::Cancel);box.exec();return box.clickedButton()==confirm;
}
}
