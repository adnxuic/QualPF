#ifndef REPORTWIDGET_H
#define REPORTWIDGET_H

#include <QWidget>
#include <QVector>
#include <QPair>
#include <QColor>
#include <QStringList>
#include "gui/model/experimentalpeaks.h"
#include "gui/model/cardtype.h"
#include "qualpf/application/presentation/quantification.h"
#include "gui/dialogs/peaksearchdialog.h"

class QPrinter;

QT_BEGIN_NAMESPACE
namespace Ui { class ReportWidget; }
QT_END_NAMESPACE

class ReportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ReportWidget(QWidget *parent = nullptr);
    ~ReportWidget();

    void updateReport(const ExperimentalPeaks &ep,
                      const QVector<CardType> &resultCards,
                      int maxCards = 100);
    void updateQuantitative(const qualpf::application::QuantificationResult &);
    void clearQuantitative();
    void clear();
    void print(QPrinter *printer);

    void setPeakSearchSettings(const qualpf::gui::PeakSearchPresentationOptions &s);
    void setRestraintsInfo(bool hasRestraints, const QStringList &active);

private:
    void generateHtml();
    static QPixmap renderPieChart(const QVector<QPair<QColor, double>> &slices, int size);

    Ui::ReportWidget  *ui;

    ExperimentalPeaks   m_ep;
    QVector<CardType>   m_cards;
    int                 m_maxCards = 100;
    qualpf::application::QuantificationResult m_quantification;
    qualpf::gui::PeakSearchPresentationOptions  m_pkSettings  = {};
    bool                m_hasRestraints = false;
    QStringList         m_activeRestraints;
};

#endif // REPORTWIDGET_H
