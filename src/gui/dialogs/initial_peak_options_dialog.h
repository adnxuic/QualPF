#pragma once
#include "qualpf/application/initial_peak_search.h"
#include "gui/search/search_task_controller.h"
#include <QDialog>
#include <QPointer>
class MainWindow;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QDialogButtonBox;
class QTabWidget;
class InitialPeakCandidatesWidget;

class InitialPeakOptionsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit InitialPeakOptionsDialog(MainWindow *);
    bool setOptions();
    void setUsage(std::shared_ptr<const qualpf::application::InitialPeakUsage>);
    void showCandidates();
  public slots:
    void accept() override;
    void reject() override;
  private:
    void setWidgets();
    void updateMode();
    void completed(qualpf::gui::SearchTaskController::Outcome);
    void restoreDefaults();
    QPointer<MainWindow> window_;
    qualpf::application::ProductHandle source_;
    qualpf::application::InitialPeakConfiguration draft_;
    bool running_ = false, cancelRequested_ = false, slopeEdited_ = false;
    QLineEdit *slope_;
    QCheckBox *automatic_;
    QSpinBox *halfWindow_, *degree_;
    QLabel *status_;
    QDialogButtonBox *buttons_;
    QTabWidget *tabs_;
    InitialPeakCandidatesWidget *candidates_;
};
