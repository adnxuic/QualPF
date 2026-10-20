#include "gui/widgets/initial_peak_candidates_widget.h"
#include <QAbstractTableModel>
#include <QHeaderView>
#include <QLabel>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>

using namespace qualpf::application;
class InitialPeakTableModel final : public QAbstractTableModel {
  public:
    using QAbstractTableModel::QAbstractTableModel;
    void setUsage(std::shared_ptr<const InitialPeakUsage> usage) {
        if (usage == usage_) return;
        beginResetModel(); usage_ = std::move(usage); endResetModel();
    }
    int rowCount(const QModelIndex &parent = {}) const override {
        return parent.isValid() || !usage_ ? 0 : int(usage_->detection.candidates.size());
    }
    int columnCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 3; }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (role != Qt::DisplayRole) return {};
        if (orientation == Qt::Vertical) return section + 1;
        switch (section) {
        case 0: return tr("Sample"); case 1: return tr("2\u03b8 (\u00b0)"); case 2: return tr("Observed intensity");
        default: return {};
        }
    }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!usage_ || !index.isValid() || index.row() < 0 || index.row() >= rowCount() || role != Qt::DisplayRole) return {};
        const auto at = usage_->detection.candidates[std::size_t(index.row())].sample_index;
        switch (index.column()) {
        case 0: return QVariant::fromValue(qulonglong(at + 1));
        case 1: return QString::number(usage_->spectrum->pattern().two_theta[at].value, 'g', 12);
        case 2: return QString::number(usage_->spectrum->pattern().observed[at], 'g', 12);
        default: return {};
        }
    }
  private:
    std::shared_ptr<const InitialPeakUsage> usage_;
};
InitialPeakCandidatesWidget::InitialPeakCandidatesWidget(QWidget *parent) : QWidget(parent) {
    setObjectName("initialPeakCandidatesPage");
    auto *layout = new QVBoxLayout(this);
    status_ = new QLabel(this); status_->setObjectName("initialPeakCandidateSummary"); status_->setWordWrap(true); layout->addWidget(status_);
    auto *help = new QLabel(tr("Positions are derivative trigger samples, before peak-top refinement. Sample numbers start at 1."), this);
    help->setWordWrap(true); layout->addWidget(help);
    table_ = new QTableView(this); table_->setObjectName("initialPeakCandidateTable");
    model_ = new InitialPeakTableModel(table_); table_->setModel(model_);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers); table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->horizontalHeader()->setMinimumSectionSize(80);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table_);
    setUsage({});
}
void InitialPeakCandidatesWidget::setUsage(std::shared_ptr<const InitialPeakUsage> usage) {
    model_->setUsage(usage);
    if (!usage) status_->setText(tr("No current candidates. Run Initial Peak Search."));
    else status_->setText(tr("%1 candidate(s) \u00b7 half-window: %2 samples \u00b7 effective SG degree: %3")
        .arg(qulonglong(usage->detection.candidates.size())).arg(usage->detection.applied.half_window)
        .arg(std::min(usage->detection.applied.polynomial_degree, 2 * usage->detection.applied.half_window)));
    table_->resizeColumnsToContents();
}
