#pragma once
#include "qualpf/application/search_coordinator.h"
#include <QDockWidget>
class QTreeWidget;
class QPushButton;
class QLabel;
namespace qualpf::gui {
class WorkflowDock final : public QDockWidget {
    Q_OBJECT
  public:
    WorkflowDock(application::Workspace &,QWidget *parent);
    ~WorkflowDock() override;
    void refresh();
  signals:
    void resultsChanged();
  private:
    void undoSelected();
    void showDetails();
    application::Workspace &workspace_;
    application::ProductSearchSession::Subscription subscription_;
    QTreeWidget *steps_;
    QPushButton *undo_;
    QLabel *status_;
};
}
