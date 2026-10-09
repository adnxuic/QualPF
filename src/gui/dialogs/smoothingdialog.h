#ifndef SMOOTHINGDIALOG_H
#define SMOOTHINGDIALOG_H

class MainWindow;

#include <memory>
namespace qualpf::gui {
class PreprocessingDialog;
}
#include <QDialog>
#include <QPushButton>

#include "qualpf/application/startup/startup_options.h"
#include "gui/model/preprocessing_options.h"

namespace Ui {
class SmoothingDialog;
}

class SmoothingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SmoothingDialog(MainWindow *mw);
    ~SmoothingDialog();
    enum SmoothingMethod {SavGol, Averaging};
    void setSmoothing();

private slots:
    void on_methodComboBox_currentIndexChanged(int index);
    void on_pointsSpinBox_valueChanged(int arg1);
    void on_polynomialSpinBox_valueChanged(int arg1);
    void accept() override;
    void reject() override;

    void on_buttonBox_clicked(QAbstractButton *button);

private:
  std::unique_ptr<qualpf::gui::PreprocessingDialog> cpp_;
  bool initializing_ = false;
  void preprocessingAction(int action);
  Ui::SmoothingDialog *ui;
  MainWindow *mWindow;
  qualpf::gui::SmoothingPresentationOptions sOptions, sOptionsSaved;
  int getPoints() const;
  void setPoints(int npoints);
  void restoreOptions();
};

#endif // SMOOTHINGDIALOG_H
