#pragma once
#include "qualpf/application/initial_peak_configuration.h"
#include <QWidget>
class QLabel;
class QTableView;
class InitialPeakTableModel;

class InitialPeakCandidatesWidget final : public QWidget {
    Q_OBJECT
  public:
    explicit InitialPeakCandidatesWidget(QWidget *);
    void setUsage(std::shared_ptr<const qualpf::application::InitialPeakUsage>);
  private:
    QLabel *status_;
    QTableView *table_;
    InitialPeakTableModel *model_;
};
