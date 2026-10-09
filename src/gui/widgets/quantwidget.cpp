#include "gui/adapters/quantification.h"
#include "gui/state/workspace_context.h"
#include "gui/widgets/quantwidget.h"
#include "ui_quantwidget.h"
#include "gui/widgets/piechartwidget.h"
#include "gui/widgets/floatdelegate.h"

#include <QStandardItemModel>
#include <QHeaderView>
#include <QMenu>

#include <algorithm>
#include <cmath>

// Golden-ratio hue mapping (same algorithm as DbResultsWidget)
static QColor phaseColor(const QString &id)
{
    bool ok;
    quint64 n = id.toULongLong(&ok);
    if (!ok) n = static_cast<quint64>(qHash(id));
    const double hue = std::fmod(n * 0.618033988749895, 1.0);
    return QColor::fromHsvF(hue, 0.65, 0.88);
}

QuantWidget::QuantWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::QuantWidget)
{
    ui->setupUi(this);

    m_model = new QStandardItemModel(0, 4, this);
    m_model->setHeaderData(0, Qt::Horizontal, "");
    m_model->setHeaderData(1, Qt::Horizontal, "ID");
    m_model->setHeaderData(2, Qt::Horizontal, "Name");
    m_model->setHeaderData(3, Qt::Horizontal, "Quant.");

    ui->tableView->setModel(m_model);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->verticalHeader()->setDefaultSectionSize(22);
    ui->tableView->verticalHeader()->hide();
    ui->tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tableView->horizontalHeader()->resizeSection(0, 18);
    ui->tableView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->tableView->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);

    ui->tableView->setItemDelegateForColumn(3, new FloatDelegate(this, 2));

    connect(ui->tableView->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid()) return;
        const QString id = m_model->item(current.row(), 1)->text();
        if (!id.isEmpty())
            emit cardSelected(id);
    });

    connect(ui->tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
        const auto selected = ui->tableView->selectionModel()->selectedRows();
        QVector<CardType> cards;
        for (const QModelIndex &idx : selected) {
            if (idx.row() < phases().size())
                cards.append(phases().at(idx.row()));
        }
        emit selectedPhasesChanged(cards);
    });

    m_clearSelAction = new QAction(tr("Clear Selection"), this);
    m_clearSelAction->setShortcut(QKeySequence(Qt::Key_Escape));
    m_clearSelAction->setShortcutContext(Qt::WidgetShortcut);
    connect(m_clearSelAction, &QAction::triggered, ui->tableView, &QAbstractItemView::clearSelection);
    ui->tableView->addAction(m_clearSelAction);

    m_removePhaseAction = new QAction(tr("Remove Phase"), this);
    connect(m_removePhaseAction, &QAction::triggered, this, &QuantWidget::removeSelectedPhase);

    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->tableView, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        const bool hasSel = ui->tableView->selectionModel()->hasSelection();
        m_clearSelAction->setEnabled(hasSel);
        m_removePhaseAction->setEnabled(hasSel);
        QMenu menu(this);
        menu.addAction(m_removePhaseAction);
        menu.addSeparator();
        menu.addAction(m_clearSelAction);
        menu.exec(ui->tableView->viewport()->mapToGlobal(pos));
    });
}

QuantWidget::~QuantWidget()
{
    delete ui;
}

void QuantWidget::addPhase(const CardType &card)
{
    auto cards = phases();
    if (std::any_of(cards.begin(), cards.end(), [&](const auto &c) { return c.getId() == card.getId(); })) return;
    cards.append(card);
    qualpf::gui::workspaceContext(this).searchSession().replacePhases(std::move(cards));
    updateQuant();
}

void QuantWidget::clearPhases()
{
    qualpf::gui::workspaceContext(this).searchSession().replacePhases({});
    m_quant.clear();
    m_model->setRowCount(0);
}

void QuantWidget::removeSelectedPhase()
{
    const QModelIndex current = ui->tableView->currentIndex();
    if (!current.isValid()) return;
    const int row = current.row();
    if (row < 0 || row >= phases().size()) return;

    const CardType card = phases().at(row);
    auto cards = phases();
    cards.removeAt(row);
    qualpf::gui::workspaceContext(this).searchSession().replacePhases(std::move(cards));
    updateQuant();
    emit phaseRemoved(card);
}

void QuantWidget::updateQuant()
{
    const int n = phases().size();

    const auto values = qualpf::gui::quantitativePercentages(qualpf::gui::workspaceContext(this).workspace().session().snapshot()->phases);
    const bool valid = values.size() == static_cast<std::size_t>(n) && !values.empty();
    const QVector<double> quant(values.begin(), values.end());
    m_quant = quant;

    // Rebuild table and pie slices
    m_model->setRowCount(n);
    QVector<QPair<QColor, double>> slices;
    slices.reserve(n);

    for (int i = 0; i < n; ++i) {
        const CardType &card = phases()[i];
        const QColor color = phaseColor(card.getId());

        QString name = card.getChemicalName();
        if (!card.getMineralName().isEmpty())
            name += " [" + card.getMineralName() + "]";

        auto *colorItem = new QStandardItem();
        colorItem->setBackground(QBrush(color));
        colorItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

        auto *quantItem = new QStandardItem();
        if (valid)
            quantItem->setData(QVariant(quant[i]), Qt::DisplayRole);
        else
            quantItem->setText("-");

        m_model->setItem(i, 0, colorItem);
        m_model->setItem(i, 1, new QStandardItem(card.getId()));
        m_model->setItem(i, 2, new QStandardItem(name));
        m_model->setItem(i, 3, quantItem);

        slices.append({color, (valid) ? quant[i] : (100.0 / n)});
    }

    ui->pieChart->setSlices(slices);
}

const QVector<CardType> &QuantWidget::phases() const
{
    return qualpf::gui::workspaceContext(this).searchSession().snapshot()->phases;
}
