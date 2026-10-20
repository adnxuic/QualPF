#include "gui/adapters/quantification.h"
#include "gui/state/workspace_context.h"
#include "gui/widgets/quantwidget.h"
#include "ui_quantwidget.h"
#include "gui/widgets/piechartwidget.h"
#include "gui/widgets/floatdelegate.h"
#include "qualpf/application/pfsm.h"
#include "qualpf/application/export/quantification_export.h"
#include <QFileDialog>

#include <QStandardItemModel>
#include <QHeaderView>
#include <QMenu>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QHBoxLayout>

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
    m_status = new QLabel(this);
    m_status->setObjectName("quantificationStatus");
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::PlainText);
    m_status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ui->verticalLayout->insertWidget(0, m_status);
    auto *actions = new QHBoxLayout;
    m_editRir = new QPushButton(tr("Edit RIR…"), this);
    m_editRir->setObjectName("editRir");
    m_restoreRir = new QPushButton(tr("Restore reference RIR"), this);
    m_restoreRir->setObjectName("restoreRir");
    m_exportCsv = new QPushButton(tr("Export CSV…"), this);
    m_exportCsv->setObjectName("exportQuantificationCsv");
    actions->addWidget(m_editRir); actions->addWidget(m_restoreRir); actions->addWidget(m_exportCsv); actions->addStretch();
    ui->verticalLayout->insertLayout(1, actions);
    connect(m_editRir, &QPushButton::clicked, this, &QuantWidget::editRir);
    connect(m_restoreRir, &QPushButton::clicked, this, &QuantWidget::restoreRir);
    connect(m_exportCsv, &QPushButton::clicked, this, &QuantWidget::exportCsv);

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
        updateRirActions();
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
    if (qualpf::gui::workspaceContext(this).workspace().session().snapshot()->pfsm) return;
    auto cards = phases();
    if (std::any_of(cards.begin(), cards.end(), [&](const auto &c) { return c.getId() == card.getId(); })) return;
    cards.append(card);
    qualpf::gui::workspaceContext(this).searchSession().replacePhases(std::move(cards));
    updateQuant();
}

void QuantWidget::clearPhases()
{
    if (qualpf::gui::workspaceContext(this).workspace().session().snapshot()->pfsm) { updateQuant(); return; }
    qualpf::gui::workspaceContext(this).searchSession().replacePhases({});
    updateQuant();
}

void QuantWidget::removeSelectedPhase()
{
    const QModelIndex current = ui->tableView->currentIndex();
    if (!current.isValid()) return;
    const int row = current.row();
    if (row < 0 || row >= phases().size()) return;

    const CardType card = phases().at(row);
    if (qualpf::gui::workspaceContext(this).workspace().session().snapshot()->pfsm) {
        emit phaseRemovalRequested(card.getId()); return;
    }
    auto cards = phases();
    cards.removeAt(row);
    qualpf::gui::workspaceContext(this).searchSession().replacePhases(std::move(cards));
    updateQuant();
    emit phaseRemoved(card);
}

void QuantWidget::updateQuant()
{
    const auto source = qualpf::gui::workspaceContext(this).workspace().session().snapshot();
    m_result = qualpf::gui::prepareQuantification(source);
    const bool pfsm = m_result.method == qualpf::application::QuantificationMethod::pfsm_rir;
    const auto values = m_result.percentages();
    m_quant = QVector<double>(values.begin(), values.end());
    m_model->setColumnCount(pfsm ? 7 : 4);
    m_model->setHeaderData(3, Qt::Horizontal, "wt.%");
    const QStringList extra = {"Scale", "Shift °2θ", "RIR"};
    for (int col = 3; col < m_model->columnCount(); ++col) {
        if (col > 3) m_model->setHeaderData(col, Qt::Horizontal, extra[col-4]);
        auto *delegate = new FloatDelegate(this, col == 4 || col == 5 ? 5 : 2);
        delegate->setDoublePrecision(pfsm); delegate->setSigned(col == 5);
        auto *old = ui->tableView->itemDelegateForColumn(col);
        ui->tableView->setItemDelegateForColumn(col, delegate);
        if (old && old->parent() == this) old->deleteLater();
        ui->tableView->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    }
    m_model->setRowCount(static_cast<int>(m_result.phases.size()));
    QVector<QPair<QColor, double>> slices;
    for (int i = 0; i < m_model->rowCount(); ++i) {
        const auto &phase = m_result.phases[i];
        const auto id = QString::fromStdString(phase.id);
        const auto color = phaseColor(id);
        auto *colorItem = new QStandardItem;
        colorItem->setBackground(QBrush(color));
        m_model->setItem(i, 0, colorItem);
        m_model->setItem(i, 1, new QStandardItem(id));
        m_model->setItem(i, 2, new QStandardItem(QString::fromStdString(phase.name)));
        const auto number = [&](int col, std::optional<double> value) {
            auto *item = new QStandardItem;
            if (value) item->setData(*value, Qt::DisplayRole); else item->setText("-");
            m_model->setItem(i, col, item);
        };
        number(3, phase.weight_percent);
        if (pfsm) {
            number(4, phase.scale); number(5, phase.shift_degrees); number(6, phase.rir);
            m_model->item(i, 6)->setToolTip(QString::fromStdString(phase.rir_source) +
                tr("\nReference RIR: %1").arg(QString::fromStdString(phase.original_rir)));
        }
        if (m_result.available()) slices.append({color, *phase.weight_percent});
    }
    ui->pieChart->setSlices(slices);
    ui->pieChart->setVisible(m_result.available());
    QStringList status;
    status << (m_result.available() ? tr("Available · selected crystalline phases") :
               m_result.phases.empty() ? tr("Select phases to quantify") : tr("Quantification unavailable"));
    if (pfsm && m_result.rwp_net)
        status << tr("Joint Rwp (net): %1").arg(*m_result.rwp_net, 0, 'g', 7);
    if (pfsm && std::any_of(m_result.phases.begin(), m_result.phases.end(), [](const auto &p) { return !p.rir_revised; }))
        status << tr("Reference-library RIR provenance is unverified (Cu Kα assumed).");
    for (const auto &issue : m_result.issues)
        status << QString::fromStdString(issue.phase_id.empty() ? issue.message : issue.phase_id + ": " + issue.message);
    m_status->setText(status.join("\n"));
    QStringList notes;
    for (const auto &note : m_result.qualifications) notes << QString::fromStdString(note);
    m_status->setToolTip(notes.join("\n"));
    m_editRir->setVisible(pfsm); m_restoreRir->setVisible(pfsm);
    m_exportCsv->setEnabled(!m_result.phases.empty());
    updateRirActions();
    emit quantificationChanged();
}

void QuantWidget::updateRirActions()
{
    auto &workspace = qualpf::gui::workspaceContext(this).workspace();
    const auto source = workspace.session().snapshot();
    const auto row = ui->tableView->currentIndex().row();
    const bool selected = row >= 0 && row < static_cast<int>(m_result.phases.size());
    const bool allowed = source->pfsm && !source->editing && !workspace.searches().running() &&
                         !workspace.searches().closing() && selected && m_result.source == source;
    m_editRir->setEnabled(allowed);
    m_restoreRir->setEnabled(allowed && m_result.phases[row].rir_revised);
}

void QuantWidget::editRir()
{
    updateRirActions();
    if (!m_editRir->isEnabled()) return;
    const auto source = m_result.source;
    const auto phase = m_result.phases[ui->tableView->currentIndex().row()];
    QDialog dialog(window()); dialog.setObjectName("rirDialog");
    dialog.setWindowTitle(tr("Edit RIR · %1").arg(QString::fromStdString(phase.id)));
    auto *layout = new QFormLayout(&dialog);
    auto *value = new QLineEdit(&dialog); value->setObjectName("rirValue");
    if (phase.rir) value->setText(QString::number(*phase.rir, 'g', 17));
    auto *origin = new QLineEdit(&dialog); origin->setObjectName("rirSource");
    if (phase.rir_revised) origin->setText(QString::fromStdString(phase.rir_source));
    auto *radiation = new QComboBox(&dialog); radiation->setObjectName("rirRadiation");
    radiation->addItem(tr("Cu Kα"));
    auto *error = new QLabel(&dialog); error->setObjectName("rirError"); error->setWordWrap(true);
    error->setTextFormat(Qt::PlainText);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addRow(tr("Reference RIR"), new QLabel(QString::fromStdString(phase.original_rir), &dialog));
    layout->addRow(tr("Effective RIR"), value); layout->addRow(tr("Source / calibration"), origin);
    layout->addRow(tr("Applicable radiation"), radiation); layout->addRow(error); layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        bool ok = false; const double rir = value->text().trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(rir) || rir <= 0 || origin->text().trimmed().isEmpty()) {
            error->setText(tr("Enter a finite positive RIR and its source.")); return;
        }
        const auto result = qualpf::gui::workspaceContext(this).workspace().revisePfsmRir(source, phase.id,
            qualpf::application::PfsmRirRevision{rir, origin->text().trimmed().toStdString()});
        if (!result) { error->setText(QString::fromStdString(result.error().message)); return; }
        dialog.accept();
    });
    if (dialog.exec() == QDialog::Accepted) updateQuant();
}

void QuantWidget::restoreRir()
{
    updateRirActions();
    if (!m_restoreRir->isEnabled()) return;
    const auto result = qualpf::gui::workspaceContext(this).workspace().revisePfsmRir(
        m_result.source, m_result.phases[ui->tableView->currentIndex().row()].id, std::nullopt);
    if (result) updateQuant(); else m_status->setText(QString::fromStdString(result.error().message));
}

void QuantWidget::exportCsv()
{
    // Freeze before opening the chooser; later publications cannot mix rows.
    const auto result = m_result;
    if (result.phases.empty()) return;
    const auto path = QFileDialog::getSaveFileName(window(), tr("Export quantitative analysis"),
        QStringLiteral("quantification.csv"), tr("CSV files (*.csv)"));
    if (path.isEmpty()) return;
    const auto status = qualpf::application::exporting::writeQuantificationCsv(
        qualpf::gui::workspaceContext(this).files().exports, path.toStdString(), result);
    if (!status) m_status->setText(QString::fromStdString(status.error().message));
}

const QVector<CardType> &QuantWidget::phases() const
{
    return qualpf::gui::workspaceContext(this).searchSession().snapshot()->phases;
}
