#include "gui/state/workspace_context.h"
#include "gui/widgets/dbresultswidget.h"
#include "gui/models/paginationmodel.h"
#include "ui_dbresultswidget.h"
#include "gui/models/textfilterproxymodel.h"
#include "gui/support/scopedtimer.h"

#include "gui/widgets/floatdelegate.h"
#include "gui/adapters/preferences.h"
#include "qualpf/application/pfsm.h"

#include <QColor>
#include "gui/presentation/project_view_values.h"
#include <QColorDialog>
#include <QLabel>
#include <QSpinBox>
#include <QToolBar>
#include <QToolButton>
#include <QHash>
#include <QMenu>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSignalBlocker>
#include <QItemSelectionModel>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <stdexcept>

QColor cardColor(const QString &id, const QObject *owner)
{
    if (owner) {
        const auto view=qualpf::gui::workspaceContext(owner).workspace().projectView();
        if (const auto found=view->card_colors.find(id.toStdString());found!=view->card_colors.end()) return qualpf::gui::projectColor(found->second);
    }
    bool ok;
    quint64 n = id.toULongLong(&ok);
    if (!ok)
        n = static_cast<quint64>(qHash(id));
    const double hue = std::fmod(n * 0.618033988749895, 1.0);
    return QColor::fromHsvF(hue, 0.65, 0.88);
}

namespace {
class PfsmNumberDelegate final : public FloatDelegate {
    bool signed_;
  public:
    PfsmNumberDelegate(QObject *parent, bool sign = false) : FloatDelegate(parent, 5), signed_(sign) {}
    QString displayText(const QVariant &value, const QLocale &locale) const override {
        bool ok = false; const double number = value.toDouble(&ok);
        if (!ok) return QStringLiteral("-");
        return (signed_ && number >= 0 ? QStringLiteral("+") : QString()) + locale.toString(number, 'f', 5);
    }
};
void populateResultRow(QStandardItemModel &model, int row, const CardType &card,
                       const std::optional<qualpf::application::PfsmCandidateMetrics> &pfsm, const QObject *owner) {
    QString chemName = card.getChemicalName();
    const QString mineral = card.getMineralName();
    if (!mineral.isEmpty()) {
        if (chemName.isEmpty())
            chemName = mineral;
        else
            chemName += QStringLiteral(" [") + mineral + QLatin1Char(']');
    }

    auto *colorItem = new QStandardItem();
    colorItem->setBackground(QBrush(cardColor(card.getId(), owner)));
    colorItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    colorItem->setData(QVariant::fromValue(card), Qt::UserRole);
    model.setItem(row, 0, colorItem);
    model.setItem(row, 1, new QStandardItem(card.getQuality()));
    model.setItem(row, 2, new QStandardItem(card.getId()));
    model.setItem(row, 3, new QStandardItem(chemName));
    model.setItem(row, 4, new QStandardItem(card.getChemicalFormula()));

    const bool hasFom = card.isFomCalculated();
    auto numItem = [](double v) {
        auto *it = new QStandardItem();
        it->setData(QVariant(v), Qt::DisplayRole);
        return it;
    };
    auto naItem = []() { return new QStandardItem(QStringLiteral("-")); };

    model.setItem(row, 5, hasFom ? numItem(card.getFomPeakPos())   : naItem());
    model.setItem(row, 6, hasFom ? numItem(card.getFomIntensity()) : naItem());
    model.setItem(row, 7, hasFom ? numItem(card.getScale())        : naItem());
    model.setItem(row, 8, hasFom ? numItem(card.getFom())          : naItem());

    const QString rir = card.getRIR().trimmed();
    bool rirOk = false;
    const double rirVal = rir.toDouble(&rirOk);
    model.setItem(row, 9, (rirOk && rirVal != 0.0) ? numItem(rirVal) : naItem());
    if (pfsm) {
        model.setItem(row, 5, numItem(pfsm->rwp_percent));
        model.setItem(row, 6, numItem(pfsm->shift_degrees));
        model.setItem(row, 7, numItem(pfsm->scale));
        model.setItem(row, 8, numItem(pfsm->relative_fom));
        model.setItem(row, 9, numItem(pfsm->delta_rwp_pp));
    }
}
}

DbResultsWidget::DbResultsWidget(QWidget* parent)
    : QWidget(parent), ui(new Ui::DbResultsWidget)
{
    ui->setupUi(this);

    sourceModel = new QStandardItemModel(0, 10, this);
    sourceModel->setHeaderData(0, Qt::Horizontal, "");
    sourceModel->setHeaderData(1, Qt::Horizontal, "QM");
    sourceModel->setHeaderData(2, Qt::Horizontal, "ID");
    sourceModel->setHeaderData(3, Qt::Horizontal, "Chemical Name");
    sourceModel->setHeaderData(4, Qt::Horizontal, "Chemical Formula");
    sourceModel->setHeaderData(5, Qt::Horizontal, "Peakpos.");
    sourceModel->setHeaderData(6, Qt::Horizontal, "Intensity");
    sourceModel->setHeaderData(7, Qt::Horizontal, "Scale");
    sourceModel->setHeaderData(8, Qt::Horizontal, "FOM");
    sourceModel->setHeaderData(9, Qt::Horizontal, "S-Quant.");

    ui->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    ui->table->horizontalHeader()->show();
    ui->table->verticalHeader()->setDefaultSectionSize(22);
    ui->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->table->setSelectionMode(QAbstractItemView::ExtendedSelection);

    filterModel = new TextFilterProxyModel(this);
    filterModel->setSourceModel(sourceModel);
    filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    pageModel = new PaginationModel(500, this);
    pageModel->setSourceModel(filterModel);

    ui->table->setModel(pageModel);
    ui->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // NIENTE sortingEnabled qui!

    // FloatDelegate: right-align numerics, show '-' when value is missing
    for (int col : {5, 6, 7, 8})
        ui->table->setItemDelegateForColumn(col, new FloatDelegate(this, 5));
    ui->table->setItemDelegateForColumn(9, new FloatDelegate(this, 3));

    ui->table->horizontalHeader()->setSortIndicatorShown(true);
    ui->table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);

    // ── Navigation toolbar ──────────────────────────────────────────
    // Use addAction() for buttons so they get native toolbar-button rendering
    // (same as toolBarEntry), and addWidget() only for spinboxes/labels.
    navToolBar = new QToolBar(this);
    navToolBar->setMovable(false);
    navToolBar->setFloatable(false);

    auto addNavAction = [&](const QString &tip, const QString &iconPath) -> QToolButton * {
        QAction *act = navToolBar->addAction(QIcon(iconPath), QString());
        act->setToolTip(tip);
        return qobject_cast<QToolButton *>(navToolBar->widgetForAction(act));
    };
    firstBtn = addNavAction(tr("Go to first page"),    ":/icons/first-page.png");
    prevBtn  = addNavAction(tr("Go to previous page"), ":/icons/previous-page.png");

    pageSpin = new QSpinBox(this);
    pageSpin->setMinimum(1);
    pageSpin->setMaximum(pageModel->pageCount());
    pageSpin->setValue(1);
    pageSpin->setSuffix(QString("/%1").arg(pageModel->pageCount()));
    navToolBar->addWidget(pageSpin);

    nextBtn  = addNavAction(tr("Go to next page"),  ":/icons/next-page.png");
    lastBtn  = addNavAction(tr("Go to last page"),  ":/icons/last-page.png");
    navToolBar->addSeparator();
    navToolBar->addWidget(new QLabel(tr("Max rows:"), this));

    maxRowSpin = new QSpinBox(this);
    maxRowSpin->setMinimum(1);
    maxRowSpin->setMaximum(10000);
    maxRowSpin->setSingleStep(5);
    maxRowSpin->setValue(pageModel->maxRows());
    navToolBar->addWidget(maxRowSpin);

    ui->topLayout->addWidget(navToolBar);

    connect(ui->textFilterEdit, &QLineEdit::textEdited, filterModel, &TextFilterProxyModel::setFilterFixedString);
    connect(maxRowSpin, QOverload<int>::of(&QSpinBox::valueChanged), pageModel, &PaginationModel::setMaxRows);

    connect(firstBtn, &QToolButton::clicked, pageModel, &PaginationModel::firstPage);
    connect(prevBtn,  &QToolButton::clicked, pageModel, &PaginationModel::previousPage);
    connect(nextBtn,  &QToolButton::clicked, pageModel, &PaginationModel::nextPage);
    connect(lastBtn,  &QToolButton::clicked, pageModel, &PaginationModel::lastPage);
    connect(pageSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this](int p){ pageModel->setCurrentPage(p - 1); });

    connect(pageModel, &PaginationModel::currentPageChanged, this, &DbResultsWidget::currentPageChanged);
    connect(pageModel, &PaginationModel::pageCountChanged, this, &DbResultsWidget::pageCountChanged);
    connect(pageModel, &PaginationModel::canGoBackChanged, this, &DbResultsWidget::updateButtons);
    connect(pageModel, &PaginationModel::canGoForwardChanged, this, &DbResultsWidget::updateButtons);

    // Sort per click sull'intestazione della colonna
    connect(ui->table->horizontalHeader(), &QHeaderView::sectionClicked,
            this, &DbResultsWidget::onHeaderSectionClicked);
    connect(ui->table->horizontalHeader(), &QHeaderView::sectionResized, this, [this] { if (!restoringLayout_) saveLayout(); });


    // Card selection: emit id (for info panel) and full CardType (for peak compare)
    connect(ui->table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid()) return;
        const QString id = ui->table->model()->index(current.row(), 2).data().toString();
        if (id.isEmpty()) return;
        emit cardSelected(id);
        const QVariant v = ui->table->model()->index(current.row(), 0).data(Qt::UserRole);
        if (v.isValid())
            emit cardDataSelected(v.value<CardType>());
    });

    connect(ui->table->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        emit entrySelectionChanged(hasSelection());
        emit selectedCardsChanged(selectedCards());
    });

    clearSelAction = new QAction(tr("Clear Selection"), this);
    clearSelAction->setShortcut(QKeySequence(Qt::Key_Escape));
    clearSelAction->setShortcutContext(Qt::WidgetShortcut);
    connect(clearSelAction, &QAction::triggered, ui->table, &QAbstractItemView::clearSelection);
    ui->table->addAction(clearSelAction);

    updateButtons();
}

DbResultsWidget::~DbResultsWidget()
{
    delete ui;
}

void DbResultsWidget::setResults(const QVector<CardType>& results)
{
    qualpf::gui::workspaceContext(this).searchSession().replaceCandidates(results);
    refresh();
}

void DbResultsWidget::refresh()
{
    const auto input = qualpf::gui::workspaceContext(this).searchSession().snapshot();
    const auto &results = input->candidates;
    updateMethod(input->canonical->algorithms->spec.method());
    QSignalBlocker selectionBlocker(ui->table->selectionModel());
    qualpf::gui::ScopedTimer timer("DbResultsWidget::setResults");

    sourceModel->removeRows(0, sourceModel->rowCount());
    sourceModel->setRowCount(results.size());

    sourceModel->blockSignals(true);
    for (int i = 0; i < results.size(); ++i)
        populateRow(i, results[i]);
    sourceModel->blockSignals(false);
    filterModel->setMinimumScore(input->pfsm ? qualpf::application::pfsmSettings(input->canonical->searchOptions).minimum_relative_fom : -1);

    if (m_sortColumn >= 0) {
        sourceModel->sort(m_sortColumn, m_sortOrder);
        ui->table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
    }

    pageModel->setCurrentPage(0);
    // The model rebuild invalidates selection while its signals are blocked.
    // Publish the resulting empty selection so card plots cannot outlive it.
    emit entrySelectionChanged(false);
    emit selectedCardsChanged({});
    emit hasResultsChanged(hasResults());
}

void DbResultsWidget::populateRow(int row, const CardType &card)
{
    const auto source = qualpf::gui::workspaceContext(this).workspace().session().snapshot();
    populateResultRow(*sourceModel, row, card, qualpf::application::pfsmCandidateMetrics(*source, card.productValue()), this);
}
void DbResultsWidget::saveLayout() {
    if (!method_ || restoringLayout_) return;
    qualpf::gui::Preferences preferences(qualpf::gui::workspaceContext(this).preferences());
    const QString prefix = *method_ == qualpf::application::SearchMethod::pfsm ? "Results/PFSM/" : "Results/CSM/";
    preferences.setValue(prefix + "header", ui->table->horizontalHeader()->saveState());
    preferences.setValue(prefix + "sortColumn", m_sortColumn);
    preferences.setValue(prefix + "sortOrder", int(m_sortOrder));
}
void DbResultsWidget::updateMethod(qualpf::application::SearchMethod method) {
    if (method_ == method) return;
    saveLayout(); restoringLayout_ = true; method_ = method;
    const bool pfsm = method == qualpf::application::SearchMethod::pfsm;
    sourceModel->setHeaderData(5, Qt::Horizontal, pfsm ? "Rwp(net) %" : "Peakpos.");
    sourceModel->setHeaderData(6, Qt::Horizontal, pfsm ? "Shift °2θ" : "Intensity");
    sourceModel->setHeaderData(9, Qt::Horizontal, pfsm ? "ΔRwp pp" : "S-Quant.");
    sourceModel->setHeaderData(8, Qt::Horizontal, pfsm ? tr("Relative improvement within this round; not a probability") : tr("Classical figure of merit"), Qt::ToolTipRole);
    for (int col : {5,6,7,8,9}) {
        auto *old = ui->table->itemDelegateForColumn(col);
        ui->table->setItemDelegateForColumn(col, pfsm ? static_cast<QAbstractItemDelegate *>(new PfsmNumberDelegate(this, col == 6)) : new FloatDelegate(this, col == 9 ? 3 : 5));
        if (old) old->deleteLater();
    }
    auto *header = ui->table->horizontalHeader();
    qualpf::gui::Preferences preferences(qualpf::gui::workspaceContext(this).preferences());
    const QString prefix = pfsm ? "Results/PFSM/" : "Results/CSM/";
    const QByteArray saved = preferences.value(prefix + "header").toByteArray();
    if (saved.isEmpty() || !header->restoreState(saved)) {
        const int sizes[] = {25,40,80,250,140,110,110,100,100,100};
        for (int i = 0; i < 10; ++i) header->resizeSection(i, sizes[i]);
    }
    header->setSectionResizeMode(3, QHeaderView::Stretch);
    m_sortColumn = std::clamp(preferences.value(prefix + "sortColumn", 8).toInt(), 0, 9);
    m_sortOrder = preferences.value(prefix + "sortOrder", int(Qt::DescendingOrder)).toInt() == int(Qt::AscendingOrder) ? Qt::AscendingOrder : Qt::DescendingOrder;
    restoringLayout_ = false;
}

void DbResultsWidget::mergeResults(const QVector<CardType> &newCards)
{
    auto &session = qualpf::gui::workspaceContext(this).workspace().session();
    std::vector<qualpf::application::ProductCard> values;
    values.reserve(newCards.size());
    for (const auto &card : newCards) values.push_back(card.productValue());
    const auto source = session.snapshot();
    auto status = session.mergeCandidates(source, std::move(values));
    if (!status) throw std::runtime_error(status.error().message);
    if (session.snapshot() != source) refresh();
}

void DbResultsWidget::addCard(const CardType &card)
{
    mergeResults({card});
    selectCard(card.getId());
}

QVector<CardType> DbResultsWidget::selectedCards() const
{
    const auto selected = ui->table->selectionModel()->selectedRows();
    QVector<CardType> result;
    result.reserve(selected.size());
    for (const QModelIndex &idx : selected) {
        const QModelIndex srcIdx = filterModel->mapToSource(pageModel->mapToSource(idx));
        if (auto *item = sourceModel->item(srcIdx.row(), 0))
            result.append(item->data(Qt::UserRole).value<CardType>());
    }
    return result;
}

void DbResultsWidget::selectCard(const QString &id)
{
    // Scan the source model to find the row with the given ID
    int sourceRow = -1;
    for (int r = 0; r < sourceModel->rowCount(); ++r) {
        if (auto *item = sourceModel->item(r, 2); item && item->text() == id) {
            sourceRow = r;
            break;
        }
    }
    if (sourceRow < 0) return;

    // Map through filter and page models
    const QModelIndex srcIdx    = sourceModel->index(sourceRow, 0);
    const QModelIndex filterIdx = filterModel->mapFromSource(srcIdx);
    if (!filterIdx.isValid()) return;

    // Ensure the page containing this row is visible
    const QModelIndex pageIdx = pageModel->mapFromSource(filterIdx);
    if (pageIdx.isValid()) {
        ui->table->selectionModel()->setCurrentIndex(
            pageIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        ui->table->scrollTo(pageIdx);
    } else {
        // Row may be on a different page — navigate there first
        const int filterRow = filterIdx.row();
        const int targetPage = filterRow / pageModel->maxRows();
        pageModel->setCurrentPage(targetPage);
        const QModelIndex newPageIdx = pageModel->mapFromSource(filterIdx);
        if (newPageIdx.isValid()) {
            ui->table->selectionModel()->setCurrentIndex(
                newPageIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            ui->table->scrollTo(newPageIdx);
        }
    }
}

bool DbResultsWidget::hasResults() const
{
    return sourceModel->rowCount() > 0;
}

bool DbResultsWidget::hasSelection() const
{
    return ui->table->selectionModel()->hasSelection();
}

void DbResultsWidget::selectFirstCard()
{
    if (pageModel->rowCount() == 0) return;
    const QModelIndex first = pageModel->index(0, 0);
    ui->table->selectionModel()->setCurrentIndex(
        first, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

QVector<CardType> DbResultsWidget::displayCards() const
{
    QVector<CardType> result;
    result.reserve(sourceModel->rowCount());
    for (int r = 0; r < sourceModel->rowCount(); ++r) {
        auto *item = sourceModel->item(r, 0);
        if (item)
            result.append(item->data(Qt::UserRole).value<CardType>());
    }
    return result;
}

void DbResultsWidget::currentPageChanged(int page)
{
    QSignalBlocker blocker(pageSpin);
    pageSpin->setValue(page + 1);
}

void DbResultsWidget::pageCountChanged(int count)
{
    QSignalBlocker blocker(pageSpin);
    pageSpin->setMaximum(qMax(1, count));
    pageSpin->setSuffix(QString("/%1").arg(count));
}

void DbResultsWidget::updateButtons()
{
    firstBtn->setEnabled(pageModel->canGoBack());
    prevBtn->setEnabled(pageModel->canGoBack());
    nextBtn->setEnabled(pageModel->canGoForward());
    lastBtn->setEnabled(pageModel->canGoForward());
}

void DbResultsWidget::setContextMenuActions(const QList<QAction *> &actions)
{
    ui->table->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->table, &QWidget::customContextMenuRequested,
            this, [this, actions](const QPoint &pos) {
        QMenu menu(this);
        for (QAction *a : actions) {
            if (a)
                menu.addAction(a);
            else
                menu.addSeparator();
        }
        menu.addSeparator();
        clearSelAction->setEnabled(hasSelection());
        menu.addAction(clearSelAction);
        menu.exec(ui->table->viewport()->mapToGlobal(pos));
    });
}

void DbResultsWidget::setEntryToolBar(QToolBar *tb)
{
    tb->setMovable(false);
    tb->setFloatable(false);
    // Keep navToolBar icon size in sync with the entry toolbar
    navToolBar->setIconSize(tb->iconSize());
    connect(tb, &QToolBar::iconSizeChanged, navToolBar, &QToolBar::setIconSize);
    ui->topLayout->insertWidget(0, tb);
}

void DbResultsWidget::changeSelectedCardColor()
{
    const auto selected = ui->table->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    // Work on the first selected card only
    const QModelIndex srcIdx = filterModel->mapToSource(pageModel->mapToSource(selected.first()));
    auto *colorItem = sourceModel->item(srcIdx.row(), 0);
    if (!colorItem) return;

    const QString id = colorItem->data(Qt::UserRole).value<CardType>().getId();
    const QColor current = colorItem->background().color();

    const QColor chosen = QColorDialog::getColor(current, this, tr("Choose card color"));
    if (!chosen.isValid()) return;

    auto &workspace=qualpf::gui::workspaceContext(this).workspace();
    auto view=*workspace.projectView();view.card_colors[id.toStdString()]=qualpf::gui::projectRgba(chosen);
    if(!workspace.updateProjectView(std::move(view))) return;
    colorItem->setBackground(QBrush(chosen));

    emit cardColorChanged(id);
}

void DbResultsWidget::deleteSelectedCards()
{
    const auto selected = ui->table->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    // Map selected rows to source model row indices
    QSet<int> srcRowSet;
    for (const QModelIndex &idx : selected) {
        const QModelIndex srcIdx = filterModel->mapToSource(pageModel->mapToSource(idx));
        srcRowSet.insert(srcIdx.row());
    }

    const int minSrc = *std::min_element(srcRowSet.begin(), srcRowSet.end());
    const int maxSrc = *std::max_element(srcRowSet.begin(), srcRowSet.end());

    // Determine which card to select after deletion:
    // prefer the nearest row above the first deleted row; fall back to below the last.
    int targetSrcRow = -1;
    for (int r = minSrc - 1; r >= 0; --r) {
        if (!srcRowSet.contains(r)) { targetSrcRow = r; break; }
    }
    if (targetSrcRow < 0) {
        for (int r = maxSrc + 1; r < sourceModel->rowCount(); ++r) {
            if (!srcRowSet.contains(r)) { targetSrcRow = r; break; }
        }
    }

    // Save target ID so we can re-find it after row indices shift
    QString targetId;
    if (targetSrcRow >= 0) {
        if (auto *item = sourceModel->item(targetSrcRow, 2))
            targetId = item->text();
    }

    std::vector<std::string> ids;
    for (int row : srcRowSet)
        ids.push_back(sourceModel->item(row, 2)->text().toStdString());
    auto &session = qualpf::gui::workspaceContext(this).workspace().session();
    auto status = session.removeCandidates(session.snapshot(), ids);
    if (!status) throw std::runtime_error(status.error().message);
    refresh();
    if (!targetId.isEmpty()) selectCard(targetId);
}

void DbResultsWidget::acceptSelectedCards()
{
    const auto selected = ui->table->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;

    // Map selected page-model indices → source model rows + card data
    QVector<QPair<int, CardType>> rowCards;
    rowCards.reserve(selected.size());
    for (const QModelIndex &idx : selected) {
        const QModelIndex srcIdx = filterModel->mapToSource(pageModel->mapToSource(idx));
        auto *item = sourceModel->item(srcIdx.row(), 0);
        if (item)
            rowCards.append({srcIdx.row(), item->data(Qt::UserRole).value<CardType>()});
    }
    if (rowCards.isEmpty()) return;

    // Emit phaseAccepted in ascending row order
    std::sort(rowCards.begin(), rowCards.end(),
              [](const auto &a, const auto &b){ return a.first < b.first; });
    QVector<CardType> cards;
    for (const auto &rc : rowCards) cards.append(rc.second);
    emit phasesAccepted(cards);

    emit hasResultsChanged(hasResults());
}

void DbResultsWidget::onAcceptClicked()
{
    acceptSelectedCards();
}

void DbResultsWidget::onHeaderSectionClicked(int column)
{
    if (column == m_sortColumn) {
        m_sortOrder = (m_sortOrder == Qt::AscendingOrder)
                      ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        m_sortColumn = column;
        m_sortOrder  = Qt::AscendingOrder;
    }
    sourceModel->sort(m_sortColumn, m_sortOrder);
    ui->table->horizontalHeader()->setSortIndicator(m_sortColumn, m_sortOrder);
    pageModel->setCurrentPage(0);
    saveLayout();
}

QVector<CardType> DbResultsWidget::allCards() const
{
    return qualpf::gui::workspaceContext(this).searchSession().snapshot()->candidates;
}

qualpf::application::ProjectViewState DbResultsWidget::captureProjectView(qualpf::application::ProjectViewState view) const {
    view.result_order.clear();for(const auto &card:displayCards()) view.result_order.push_back(card.getId().toStdString());
    view.selected_cards.clear();for(const auto &card:selectedCards()) view.selected_cards.push_back(card.getId().toStdString());
    for(const auto &card:allCards()) view.card_colors[card.getId().toStdString()]=qualpf::gui::projectRgba(cardColor(card.getId(),this));
    for(const auto &card:qualpf::gui::workspaceContext(this).searchSession().snapshot()->phases) view.card_colors[card.getId().toStdString()]=qualpf::gui::projectRgba(cardColor(card.getId(),this));
    return view;
}
void DbResultsWidget::applyProjectView(const qualpf::application::ProjectViewState &view) {
    QSignalBlocker blocker(ui->table->selectionModel());
    QList<QList<QStandardItem *>> rows;
    for(const auto &id:view.result_order) for(int row=0;row<sourceModel->rowCount();++row)
        if(sourceModel->item(row,2)->text().toStdString()==id) {rows.push_back(sourceModel->takeRow(row));break;}
    while(sourceModel->rowCount()) rows.push_back(sourceModel->takeRow(0));
    for(auto &row:rows) sourceModel->appendRow(row);
    pageModel->setCurrentPage(0);ui->table->clearSelection();
    if(!view.selected_cards.empty()) for(int row=0;row<filterModel->rowCount();++row)
        if(filterModel->index(row,2).data().toString().toStdString()==view.selected_cards.front()) {pageModel->setCurrentPage(row/pageModel->maxRows());break;}
    for(const auto &id:view.selected_cards) for(int row=0;row<filterModel->rowCount();++row) {
        const auto index=filterModel->index(row,2);if(index.data().toString().toStdString()!=id) continue;
        // The current UI selection is page-local. Move to the first selected page.
        const auto source=pageModel->mapFromSource(index);
        if(source.isValid()) ui->table->selectionModel()->select(source,QItemSelectionModel::Select|QItemSelectionModel::Rows);
    }
    blocker.unblock();emit selectedCardsChanged(selectedCards());emit entrySelectionChanged(hasSelection());
    if(!view.selected_cards.empty()) emit cardSelected(QString::fromStdString(view.selected_cards.front()));
}
