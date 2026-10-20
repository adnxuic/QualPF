#pragma once
#include "qualpf/application/configured_fwhm.h"
#include "gui/search/search_task_controller.h"
#include <QDialog>
#include <QPointer>
class MainWindow;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QSlider;
class QSpinBox;
class QLabel;
class QDialogButtonBox;

class FwhmOptionsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit FwhmOptionsDialog(MainWindow *);
    bool setOptions();
  public slots:
    void accept() override;
    void reject() override;
  private:
    void setWidgets();
    void updateMode();
    void setRunning(bool);
    void completed(qualpf::gui::SearchTaskController::Outcome);
    void restoreDefaults();
    QPointer<MainWindow> window_;
    qualpf::application::ProductHandle source_;
    qualpf::application::FwhmConfiguration draft_;
    std::shared_ptr<const qualpf::application::ConfiguredFwhmEvaluation> result_;
    bool running_ = false, cancelRequested_ = false, widthEdited_ = false;
    QComboBox *algorithm_;
    QWidget *samplingPanel_;
    QSlider *samplingSlider_;
    QSpinBox *samplingSpin_;
    QCheckBox *fixed_;
    QLineEdit *width_;
    QLabel *status_;
    QDialogButtonBox *buttons_;
};
