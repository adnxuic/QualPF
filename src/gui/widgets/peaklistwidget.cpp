#include "gui/widgets/peaklistwidget.h"
#include "ui_peaklistwidget.h"
#include "gui/widgets/floatdelegate.h"
#include "gui/mainwindow.h"
#include "gui/state/workspace_context.h"
#include "gui/state/preprocessing_product.h"
#include "gui/dialogs/manual_peak_dialog.h"
#include <QIcon>







PeakListWidget::PeakListWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PeakListWidget),
    tableChangedEnabled(true)
{
    ui->setupUi(this);

    createPeakListModel();
    setPeakTableView();
    createContextMenuActions();
}

PeakListWidget::~PeakListWidget()
{
    delete ui;
}

void PeakListWidget::updatePeakListTable()
{
    updatePeakListModel();

    ui->peakTableView->resizeColumnsToContents();
}

void PeakListWidget::createContextMenu(QPoint pos)
{
    QModelIndex indexMenu=ui->peakTableView->indexAt(pos);
    QMenu *menu=new QMenu(this);
    menu->addAction(addPeakAction);
    if (indexMenu.isValid()) {
        menu->addAction(QIcon(":/icons/edit-peak.png"), tr("Modify measured peak..."), this, [this,indexMenu] {
            if (tableSource != qualpf::gui::workspaceContext(this).searchSession().snapshot()) { updatePeakListTable(); return; }
            if (auto *mw = qobject_cast<MainWindow *>(window()))
                qualpf::gui::editManualPeak(*mw, std::size_t(peakListProxyModel->mapToSource(indexMenu).row()), {}, true);
        });
    }
    menu->addAction(deletePeakAction);
    menu->addSeparator();
    menu->addAction(clearSelectionAction);

    deletePeakAction->setEnabled(indexMenu.isValid());
    clearSelectionAction->setEnabled(!ui->peakTableView->selectionModel()->selectedRows().isEmpty());

    menu->popup(ui->peakTableView->viewport()->mapToGlobal(pos));
}

void PeakListWidget::deletePeaks()
{
    if (auto *mw = qobject_cast<MainWindow *>(window())) mw->xpdViewer()->deleteSelectedPeaks();
}

void PeakListWidget::addRow()
{
    QList<QStandardItem *> items;
    items.append(new QStandardItem());
    items.append(new QStandardItem());
    QStandardItem *item = new QStandardItem();
    item->setFlags(item->flags() &  ~Qt::ItemIsEditable);  //Not editable
    items.append(item);
    item = new QStandardItem();
    item->setFlags(item->flags() &  ~Qt::ItemIsEditable);  //Not editable
    items.append(item);
    peakListModel->insertRow(peakListModel->rowCount(),items);
}

void PeakListWidget::addPeak()
{
    if (auto *mw = qobject_cast<MainWindow *>(window())) qualpf::gui::editManualPeak(*mw, {}, {}, true);
}

void PeakListWidget::drawPeaks()
{
    if (auto *mw = qobject_cast<MainWindow *>(window())) mw->refreshPreprocessingDisplay();
}

void PeakListWidget::peakListTableChanged(const QModelIndex &topLeft, const QModelIndex &)
{
    if (tableChangedEnabled) {
        auto *mw = qobject_cast<MainWindow *>(window());
        auto &session = qualpf::gui::workspaceContext(this).searchSession();
        if (!mw) return;
        if (!tableSource || tableSource != session.snapshot() || !topLeft.isValid() ||
            topLeft.row() < 0 || topLeft.row() >= tableSource->peaks.tth.size() ||
            topLeft.column() < 0 || topLeft.column() > 3) {
            mw->setStatusMessage(tr("The peak snapshot changed. Reload the table before editing."));
            updatePeakListTable(); return;
        }
        const auto &p = tableSource->peaks;
        auto allowed = qualpf::gui::peakEditEntryStatus(qualpf::gui::workspaceContext(this));
        if (!allowed) { mw->setStatusMessage(QString::fromStdString(allowed.error().reason)); updatePeakListTable(); return; }
        const auto row = topLeft.row();
        qualpf::application::ManualPeakChange change;
        change.operation = qualpf::application::ManualPeakChange::Operation::modify;
        change.rows = {std::size_t(row)};
        change.two_theta = p.tth[row]; change.intensity = p.intensity[row]; change.fwhm = p.fwhm[row];
        if (p.integratedIntensity) change.integrated_intensity = (*p.integratedIntensity)[row];
        bool numeric = false;
        const double value = topLeft.data().toDouble(&numeric);
        if (!numeric) { updatePeakListTable(); return; }
        if (topLeft.column() == 0) change.two_theta = value;
        if (topLeft.column() == 2) change.intensity = value;
        if (topLeft.column() == 3) change.fwhm = value;
        qualpf::application::PreprocessingRequest request;
        qualpf::application::TablePeakChange input;
        input.row = change;
        input.coordinate = topLeft.column() == 1 ? value : change.two_theta;
        input.representation = topLeft.column() == 1 ? qualpf::application::PeakCoordinateRepresentation::d_spacing :
            qualpf::application::PeakCoordinateRepresentation::two_theta;
        request.table = input; request.wavelength = qualpf::WavelengthAngstrom{p.wave};
        request.tolerance = qualpf::AngularWidthDegrees{tableSource->peakTolerance.value_or(.15)};
        qualpf::gui::PreprocessingEdit edit(session);
        auto status = edit.preview(request);
        if (status) status = edit.commit();
        if (!status) mw->setStatusMessage(QString::fromStdString(status.error().message));
        else mw->refreshPatternPresentation();
        mw->refreshPreprocessingDisplay();
        return;
    }
}

void PeakListWidget::createPeakListModel()
{
    peakListModel = new QStandardItemModel(this);
    peakListProxyModel = new QSortFilterProxyModel(this);
    peakListProxyModel->setSourceModel(peakListModel);
    peakListModel->setColumnCount(4);
    //const QStringList titles = { "2"+QString(0x03B8), "d", "Intensity", "FWHM"}; Not compatible with Qt6
    const QStringList titles = { "2"+QString(QChar(0x03B8)), "d", "Intensity", "FWHM"};
    peakListModel->setHorizontalHeaderLabels(titles);
    connect(peakListModel, &QStandardItemModel::dataChanged, this, &PeakListWidget::peakListTableChanged);
}

void PeakListWidget::createContextMenuActions()
{
    deletePeakAction = new QAction(QIcon(":/icons/delete-peak.png"), "Delete Peaks", this);
    connect(deletePeakAction, &QAction::triggered, this, &PeakListWidget::deletePeaks);
    addPeakAction = new QAction(QIcon(":/icons/add-peak.png"), "Add Peak", this);
    connect(addPeakAction, &QAction::triggered, this, &PeakListWidget::addPeak);
    clearSelectionAction = new QAction(tr("Clear Selection"), this);
    clearSelectionAction->setShortcut(QKeySequence(Qt::Key_Escape));
    clearSelectionAction->setShortcutContext(Qt::WidgetShortcut);
    connect(clearSelectionAction, &QAction::triggered, this, &PeakListWidget::clearSelection);
    ui->peakTableView->addAction(clearSelectionAction);
}

void PeakListWidget::clearSelection()
{
    ui->peakTableView->clearSelection();
}

void PeakListWidget::updatePeakListModel()
{
    tableChangedEnabled = false;
    {
        tableSource = qualpf::gui::workspaceContext(this).searchSession().snapshot();
        const auto &p = tableSource->peaks;
        peakListModel->setRowCount(p.tth.size());
        for (int row = 0; row < p.tth.size(); ++row) {
            const double values[] = {p.tth[row], p.d[row], p.intensity[row], p.fwhm[row]};
            for (int col = 0; col < 4; ++col) {
                auto *item = new QStandardItem(); item->setData(values[col], Qt::DisplayRole);
                peakListModel->setItem(row, col, item);
            }
        }
        tableChangedEnabled = true;
        return;
    }
}

void PeakListWidget::markSelectedPeaks()
{
    if (!tableChangedEnabled) return;
    QModelIndexList selection = ui->peakTableView->selectionModel()->selectedRows();
    QVector<int> selPeak(selection.count());
    for(int i=0; i < selection.count(); i++)
    {
        selPeak[i] = peakListProxyModel->mapToSource(selection.at(i)).row();
    }
    auto *mw = qobject_cast<MainWindow *>(window());
    if (mw) mw->xpdViewer()->drawSelectedPeaks(selPeak);
}

void PeakListWidget::setPeakTableView()
{
    ui->peakTableView->setModel(peakListProxyModel);
    ui->peakTableView->verticalHeader()->setDefaultSectionSize(22);
    ui->peakTableView->setSortingEnabled(true);
    ui->peakTableView->sortByColumn(0,Qt::AscendingOrder);
    ui->peakTableView->verticalHeader()->hide();
    ui->peakTableView->setShowGrid(false);
#ifndef _WIN32
    ui->peakTableView->setAlternatingRowColors(true);
#endif
    ui->peakTableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->peakTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->peakTableView->horizontalHeader()->stretchLastSection();

    FloatDelegate *floatDelegate = new FloatDelegate(ui->peakTableView);
    floatDelegate->setDecimals(5);
    ui->peakTableView->setItemDelegateForColumn(0, floatDelegate);
    ui->peakTableView->setItemDelegateForColumn(1, floatDelegate);
    FloatDelegate *floatDelegate1 = new FloatDelegate(ui->peakTableView);
    ui->peakTableView->setItemDelegateForColumn(2, floatDelegate1);
    ui->peakTableView->setItemDelegateForColumn(3, floatDelegate1);

    ui->peakTableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->peakTableView, &QTableView::customContextMenuRequested,
            this, &PeakListWidget::createContextMenu);
    connect(ui->peakTableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &PeakListWidget::markSelectedPeaks);

}
