#ifndef QUANTWIDGET_H
#define QUANTWIDGET_H

#include <QWidget>
#include <QVector>
#include "gui/model/cardtype.h"
#include "qualpf/application/presentation/quantification.h"

class QAction;
class QStandardItemModel;
class QLabel;
class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui { class QuantWidget; }
QT_END_NAMESPACE

class QuantWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QuantWidget(QWidget *parent = nullptr);
    ~QuantWidget();

signals:
    void cardSelected(const QString &id);
    void selectedPhasesChanged(const QVector<CardType> &cards);
    void phaseRemoved(const CardType &card);
    void phaseRemovalRequested(const QString &id);
    void quantificationChanged();

public slots:
    void addPhase(const CardType &card);

public:
    void clearPhases();
    void refresh() { updateQuant(); }
    const QVector<CardType>  &phases()            const;
    const QVector<double>    &quantPercentages()  const { return m_quant;  }
    const qualpf::application::QuantificationResult &quantificationResult() const { return m_result; }

private slots:
    void removeSelectedPhase();
    void editRir();
    void restoreRir();
    void exportCsv();

private:
    void updateQuant();
    void updateRirActions();

    Ui::QuantWidget      *ui;
    QStandardItemModel   *m_model;

    QVector<double>       m_quant;   // computed percentages (empty if RIR unavailable)
    QAction              *m_clearSelAction    = nullptr;
    QAction              *m_removePhaseAction = nullptr;
    QLabel *m_status = nullptr;
    QPushButton *m_editRir = nullptr, *m_restoreRir = nullptr;
    QPushButton *m_exportCsv = nullptr;
    qualpf::application::QuantificationResult m_result;
};

#endif // QUANTWIDGET_H
