#pragma once
#include "qualpf/application/pfsm.h"
#include <QDialog>
#include <array>
class MainWindow;
class QLineEdit;
class QLabel;
class QComboBox;
class QDoubleSpinBox;
class QSlider;
class QSpinBox;
class QCheckBox;
class QTabWidget;
class QScrollArea;
class QToolButton;
namespace qualpf::gui {
class WorkspaceContext;
qualpf::application::PfsmSettings savedPfsmSettings(WorkspaceContext &, std::optional<qualpf::Error> *recallWarning = nullptr);
}
class PfsmOptionsDialog final : public QDialog {
    Q_OBJECT
  public:
    explicit PfsmOptionsDialog(MainWindow *);
    bool setOptions();
  public slots:
    void accept() override;
  signals:
    void settingsApplied();
  private:
    void setWidgets(const qualpf::application::PfsmSettings &);
    void updateResolved();
    void updateRecallEnabled();
    void clearErrors();
    void showRecallError(std::size_t, const QString &);
    MainWindow *window_;
    qualpf::application::ProductHandle source_;
    QLineEdit *lower_, *upper_;
    QLabel *width_, *resolved_, *error_;
    QComboBox *weighting_, *resourceFallback_;
    QDoubleSpinBox *minimum_;
    QSlider *slider_;
    QSpinBox *workers_;
    QCheckBox *cache_;
    QTabWidget *tabs_;
    QScrollArea *generalScroll_, *recallScroll_;
    QToolButton *advanced_;
    QCheckBox *recallEnabled_;
    std::array<QSpinBox *, 5> recallCounts_{};
    std::array<QLabel *, 5> recallErrors_{};
    QLabel *shiftError_, *recallWarning_, *currentRecall_;
};
