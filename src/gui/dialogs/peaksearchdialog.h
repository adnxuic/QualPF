#ifndef PEAKSEARCHDIALOG_H
#define PEAKSEARCHDIALOG_H

#include <memory>
namespace qualpf::gui {
class PreprocessingDialog;
}
#include <QDialog>
#include <QAbstractButton>

class MainWindow;

#include "qualpf/application/startup/startup_options.h"
#include "gui/model/preprocessing_options.h"

namespace Ui {
class PeakSearchDialog;
}

class PeakSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PeakSearchDialog(MainWindow *mw);
    void setOptions();
    ~PeakSearchDialog();

    const qualpf::gui::PeakSearchPresentationOptions &settings() const { return pkSettings; }

private slots:
    void onNumPeaksSpinBoxValueChanged();
    void onButtonBoxClicked(QAbstractButton *button);
    void onIntensitySpinBoxValueChanged(double arg1);
    void onMinSpinBoxValueChanged(double arg1);
    void onMaxSpinBoxValueChanged(double arg1);
    void onSensitivitySpinBoxValueChanged(int arg1);
    void accept() override;
    void reject() override;

private:
  std::unique_ptr<qualpf::gui::PreprocessingDialog> cpp_;
  bool initializing_ = false;
  void preprocessingAction(int action);
  Ui::PeakSearchDialog *ui;
  MainWindow *mWindow;
  qualpf::gui::PeakSearchPresentationOptions pkSettings, pkSettingsSaved;
  void setWidgets(const qualpf::gui::PeakSearchPresentationOptions &psettings);
  void restoreSavedSettings();
  void updatePeakListTable();
};

#endif // PEAKSEARCHDIALOG_H
