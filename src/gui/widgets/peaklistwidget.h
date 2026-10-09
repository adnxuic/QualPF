#ifndef PEAKLISTWIDGET_H
#define PEAKLISTWIDGET_H

#include <QWidget>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include "qualpf/application/startup/startup_options.h"
#include "gui/model/preprocessing_options.h"
#include "gui/state/search_session.h"

namespace Ui {
class PeakListWidget;
}

class PeakListWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PeakListWidget(QWidget *parent = nullptr);
    ~PeakListWidget();
    void updatePeakListTable();

    QStandardItemModel *peakListModel;

private slots:
    void createContextMenu(QPoint pos);
    void deletePeaks();
    void addPeak();
    void clearSelection();
    void peakListTableChanged(const QModelIndex &topLeft, const QModelIndex &);
    void markSelectedPeaks();

private:
    Ui::PeakListWidget *ui;
    QSortFilterProxyModel *peakListProxyModel;
    QAction *deletePeakAction;
    QAction *addPeakAction;
    QAction *clearSelectionAction;
    bool tableChangedEnabled;
    qualpf::gui::SessionViewHandle tableSource;

    void createPeakListModel();
    void createContextMenuActions();
    void updatePeakListModel();
    void setPeakTableView();
    void drawPeaks();
    void addRow();
};

#endif // PEAKLISTWIDGET_H
