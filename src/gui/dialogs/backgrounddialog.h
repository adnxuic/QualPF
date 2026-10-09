#ifndef BACKGROUNDDIALOG_H
#define BACKGROUNDDIALOG_H

#include <memory>
namespace qualpf::gui {
class PreprocessingDialog;
}
namespace qualpf::gui {
struct SessionStateView;
}
#include "gui/model/preprocessing_options.h"

#include <QDialog>
#include <QAbstractButton>

namespace Ui {
class BackgroundDialog;
}

class BackgroundDialog : public QDialog
{
    Q_OBJECT
    friend class ProductWorkflowProbe;

public:
    explicit BackgroundDialog(QWidget *parent = nullptr);
    ~BackgroundDialog();

    enum BackgroundType {Chebyshev, Polynomial, Cosine_Fourier, Cubic_Spline, Bezier_Spline, Filter, None};
    Q_ENUM(BackgroundType)
    void setBackground();

private slots:
    void onTypeComboBoxCurrentIndexChanged(int index);
    void onButtonBoxClicked(QAbstractButton *button);
    void applyAutoChanged();
    void applyNcoefChanged();
    void onApplyFilterButtonClicked();
    void onResetFilterButtonClicked();
    void accept() override;
    void reject() override;

private:
  std::unique_ptr<qualpf::gui::PreprocessingDialog> cpp_;
  std::shared_ptr<const qualpf::gui::SessionStateView> legacySource_;
  bool explicitLegacy_ = false;
  bool initializing_ = false;
  void preprocessingAction(int action);
  Ui::BackgroundDialog *ui;
  qualpf::gui::BackgroundPresentationOptions bkSettings[7];
  qualpf::gui::BackgroundPresentationOptions bkCurrentSettings, bkSavedSettings;
  int oldIndex = Chebyshev;
  void setWidgets(const qualpf::gui::BackgroundPresentationOptions &bSettings);
  void setPage(const qualpf::gui::BackgroundPresentationOptions &bSettings);
  void enableBkWidget(bool enabled, int index);
  void restoreSavedSettings();
};

#endif // BACKGROUNDDIALOG_H
