#include "gui/state/workspace_context.h"
#include "qualpf/application/domain_inputs.h"
#include "gui/plotting/xpdviewwidget.h"
#include "gui/state/preprocessing_product.h"
#include "gui/model/cardtype.h"
#include "gui/plotting/qt_plot_sink.h"
#include "gui/reference/libcomune.h"
#include "gui/plotting/xpdutils.h"
#include "gui/reference/nr.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <QLabel>
#include <QHash>
#include <QSignalBlocker>

XpdViewWidget::XpdViewWidget(QWidget *parent)
    : CustomPlotZoom(parent)
    , nObserved(0)
    , nReflections(0)
    , nProfCurves(0)
    , yLowerRange(DBL_MAX)
    , yUpperRange(-DBL_MAX)
    , yLowerRangeNoRef(0.0)
    , mLastYOffset(0)
    , rescalePlot(true)
    , mAction(NoAction)
    , savedZoomAction(mAction)
{
    hideGraphicArea();

    // TODO
    // remove parameter idcolor from setColorLine
    // fix default idcolor in setColor (ex. 3 for back and bpoints)
    // setDefaultLine and scatter with no par, default idcolor shoud be written in graphItem.cpp
    calc.gtype = graphItem::Calculated;
    calc.defaultIDColor = 2;
    calc.setColorLine(qualpf::gui::workspaceContext(this).preferences());
    calc.setName();
    back.gtype = graphItem::Background;
    back.defaultIDColor = 3;
    back.setColorLine(qualpf::gui::workspaceContext(this).preferences());
    back.setName();
    bpoints.gtype = graphItem::Background_Points;
    bpoints.defaultIDColor = 3;
    bpoints.setColorLine(qualpf::gui::workspaceContext(this).preferences());
    bpoints.setName();
    bpoints.setLineStyle(Qt::NoPen);
    bpoints.setLineConnectionType(QCPGraph::lsNone);
    diff.gtype = graphItem::Difference;
    diff.defaultIDColor = 4;
    diff.setColorLine(qualpf::gui::workspaceContext(this).preferences());
    diff.setName();
    cdiff.gtype = graphItem::Cumulative;
    cdiff.defaultIDColor = 5;
    cdiff.setColorLine(qualpf::gui::workspaceContext(this).preferences());
    cdiff.setName();
    peaks.gtype = graphItem::Peaks;
    peaks.defaultIDColor = 6;
    peaks.setColorLine(qualpf::gui::workspaceContext(this).preferences());
    peaks.setName();
    peaks.setLineConnectionType(QCPGraph::lsImpulse);
    smooth.gtype = graphItem::Smoothing;
    smooth.setName();
    smooth.setPen(QPen(Qt::green,1,Qt::DashLine));
    undpeaks.gtype = graphItem::Unindexed_Peaks;
    undpeaks.setName();
    QColor colorUndPeaks = peaks.getPen().color();
    QCPScatterStyle scatter(QCPScatterStyle::ssTriangleInverted,colorUndPeaks,colorUndPeaks,10);
    undpeaks.setScatter(scatter);
    undpeaks.setLineStyle(Qt::NoPen);
    undpeaks.setLineConnectionType(QCPGraph::lsNone);
    sysAbs.gtype = graphItem::Systematic_Absences;
    sysAbs.setName();
    sysAbs.setLineStyle(Qt::NoPen);
    sysAbs.setLineConnectionType(QCPGraph::lsNone);
    selectedRef.gtype = graphItem::Selected_Reflections;
    selectedRef.setName();
    selectedRef.setLineStyle(Qt::NoPen);
    selectedRef.setLineConnectionType(QCPGraph::lsNone);
    selectedPeak.gtype = graphItem::Selected_Peaks;
    selectedPeak.setName();
    selectedPeak.setLineStyle(Qt::NoPen);
    selectedPeak.setLineConnectionType(QCPGraph::lsNone);
    intervalLimit.gtype = graphItem::Intervals;
    intervalLimit.setName();

    setAcceptDrops(true);

    plotSettings.read(qualpf::gui::workspaceContext(this).preferences());
}

void XpdViewWidget::hideGraphicArea()
{
    axisRect(0)->setVisible(false);
}

void XpdViewWidget::clearProjectPlot()
{
    deleteDoubleCursor();
    setGraphicArea();
    // Ordinary canvas replacement keeps accepted cards and observed styles.
    // An empty project must also release every retained data/selection buffer.
    obs.clear();
    profCurves.clear();
    refSet.clear();
    m_cardPeaks.clear();
    for (auto *item : {&calc, &back, &bpoints, &diff, &cdiff, &peaks, &smooth,
                       &undpeaks, &sysAbs, &selectedRef, &selectedPeak, &intervalLimit}) {
        item->setData({}, {});
        item->setX(QVector<int>{});
        item->wave = -1;
        item->visible = false;
        item->min = DBL_MAX;
        item->max = -DBL_MAX;
        item->yPos = item->lengthRef = 0;
        item->itemIndexStart = item->itemIndexEnd = -1;
    }
    yLowerRange = DBL_MAX;
    yUpperRange = -DBL_MAX;
    yLowerRangeNoRef = mLastYOffset = 0;
    rescalePlot = true;
    setDataBounds(0, 5);
    {
        const QSignalBlocker xBlocker(xAxis), yBlocker(yAxis);
        xAxis->setRange(0, 5);
        yAxis->setRange(0, 5);
    }
    if (horizontalScrollBar) {
        const QSignalBlocker blocker(horizontalScrollBar);
        horizontalScrollBar->setRange(0, 0);
        horizontalScrollBar->setValue(0);
        horizontalScrollBar->setPageStep(scrollBarLen);
        horizontalScrollBar->hide();
    }
    for (const auto &label : {labelWidget1, labelWidget2, labelWidget3})
        if (label) label->clear();
    hideGraphicArea();
    replot();
}

void XpdViewWidget::setGraphicArea()
{
    disableTracer();
    clearGraphs();
    m_cardPeakGraphs.clear(); // pointers invalidated by clearGraphs()
    m_selectedCompareGraphs.clear(); // pointers invalidated by clearGraphs()
    clearPlottables();
    clearItems();

    plotSettings.applyTo(this);

    axisRect(0)->setVisible(true);

    xAxis2->setVisible(true);
    xAxis2->setTickLabels(false);
    xAxis2->setTicks(false);

    yAxis2->setVisible(true);
    yAxis2->setTickLabels(false);
    yAxis2->setTicks(false);    
    yAxis->grid()->setZeroLinePen(Qt::NoPen); // hide zero-line

    nObserved = 0;
    // Save accepted-card entries (id != "") before clearing; clearItems() above
    // already destroyed their QCPItemLine objects, so reset their item indices.
    QVector<ReflectionBar> savedRefSet;
    for (int i = 0; i < refSet.size(); i++) {
        if (!refSet[i].id.isEmpty()) {
            ReflectionBar rb = refSet[i];
            rb.itemIndexStart = -1;
            rb.itemIndexEnd   = -1;
            rb.graphIndex = -1; // clearGraphs() destroyed the QCPGraph objects
            savedRefSet.push_back(rb);
        }
    }

    nReflections = 0;
    refSet  = savedRefSet;
    nProfCurves = 0;
    yLowerRange = DBL_MAX;
    yUpperRange = -DBL_MIN;
    for (int i = 0; i < obs.count(); i++) obs[i].graphIndex = -1;
    for (int i = 0; i < profCurves.count(); i++) profCurves[i].graphIndex = -1;
    back.graphIndex = -1;
    bpoints.graphIndex = -1;
    calc.graphIndex = -1;
    diff.graphIndex = -1;
    cdiff.graphIndex = -1;
    //for (int i = 0; i < refl.count(); i++) refl[i].graphIndex = -1;
    peaks.graphIndex = -1;
    smooth.graphIndex = -1;
    undpeaks.graphIndex = -1;
    sysAbs.graphIndex = -1;
    sysAbs.visible = false;
    selectedRef.graphIndex = -1;
    selectedPeak.graphIndex = -1;
    intervalLimit.graphIndex = -1;
    intervalLimit.visible = false;
    plotWave.clear();
}

void XpdViewWidget::makePlot(const QVector<double> &xvet, const QVector<double> &yvet, graphItem &item)
{
    double ymin = Minimo(yvet);
    double ymax = Massimo(yvet);
    item.min = ymin;
    item.max = ymax;
    item.graphIndex = graphCount();
    plotWave.append(item.wave);
    addGraph();
    graph()->setName(item.getName());
    graph()->setData(xvet,yvet,false);
    graph()->setPen(item.getPen());
    graph()->setLineStyle(item.getLineConnectionType());
    graph()->setScatterStyle(item.getScatter());
}

void XpdViewWidget::addPlot(const QVector<double> &xvet0, const QVector<double> &yvet0, graphItem::ItemType type, int visible, float wave, const QString &name)
{
    QVector<double> xvet = xvet0;
    QVector<double> yvet = yvet0;
    // Native compatibility redraws also receive 2theta arrays. Project before
    // drawPlot computes viewport bounds, just as Session-owned updates do.
    projectSessionCoordinates(xvet, yvet, wave);
    addProjectedPlot(xvet, yvet, type, visible, wave, name);
}

void XpdViewWidget::addProjectedPlot(const QVector<double> &xvet, const QVector<double> &yvet,
                                   graphItem::ItemType type, int visible, double wave, const QString &name)
{
    xAxis->setRangeReversed(plotSettings.getAbscissa() == xpdutils::DVALUE);

    switch (type) {
    case graphItem::Observed:
        addObserved(xvet,yvet,visible,wave,name);
        break;
    case graphItem::Calculated:
        calc.visible = visible;
        calc.wave = wave;
        makePlot(xvet,yvet,calc);
        break;

    case graphItem::Background:
        back.visible = visible;
        back.wave = wave;
        makePlot(xvet,yvet,back);
        break;

    case graphItem::Background_Points:
        bpoints.visible = visible;
        bpoints.wave = wave;
        makePlot(xvet,yvet,bpoints);
        break;

    case graphItem::Difference:
        diff.visible = visible;
        diff.wave = wave;
        makePlot(xvet,yvet,diff);
        break;

    case graphItem::Cumulative:
        cdiff.visible = visible;
        cdiff.wave = wave;
        makePlot(xvet,yvet,cdiff);
        break;

    case graphItem::Peaks:
        peaks.visible = visible;
        peaks.wave = wave;
        makePlot(xvet,yvet,peaks);
        break;

    case graphItem::Smoothing:
        smooth.visible = visible;
        smooth.wave = wave;
        makePlot(xvet,yvet,smooth);
        break;

    case graphItem::Unindexed_Peaks:
        undpeaks.visible = visible;
        undpeaks.wave = wave;
        makePlot(xvet,yvet,undpeaks);
        graph()->setScatterStyle(undpeaks.getScatter());
        enableRescalePlot(false);
        break;

    case graphItem::Profile_Curves:
        if (profCurves.size() <= nProfCurves) { //new graphic element required
            graphItem item;
            item.setColorLine(qualpf::gui::workspaceContext(this).preferences(), nProfCurves);
            item.setName(name);
            item.wave = wave;
            profCurves.push_back(item);
        }
        profCurves[nProfCurves].setData(xvet,yvet);
        profCurves[nProfCurves].visible = visible;
        makePlot(xvet,yvet,profCurves[nProfCurves]);
        graph()->setScatterStyle(profCurves[nProfCurves].getScatter());
        graph()->removeFromLegend();
        nProfCurves++;
        break;

    default:
        break;
    }
}

void XpdViewWidget::addPlot(const QVector<double> &xvet0, graphItem::ItemType type, int visible, float wave)
{
    // QVector<double> xvet = xvet0;
    // if (type == graphItem::Reflections) {
    //     if (refl.size() <= nReflections) { //new graphic element required
    //         graphItem item;
    //         item.gtype = graphItem::Reflections;
    //         item.setColorLine(qualpf::gui::workspaceContext(this).preferences(), nReflections);
    //         item.setVisible(visible);
    //         item.wave = wave;
    //         refl.push_back(item);
    //     }
    //     refl[nReflections].setX(xvet);

    //     //Default Name: for more phases use Phase 1, Phase 2, ...
    //     if (nReflections == 0) {
    //         refl[0].setName();
    //     } else if (nReflections == 1) {
    //         refl[0].setName("Phase "+QString::number(1));
    //         refl[1].setName("Phase "+QString::number(2));
    //     } else {
    //         refl[nReflections].setName("Phase "+QString::number(nReflections+1));
    //     }
    //     //makeReflections(xvet);
    //     nReflections++;
    // } else if (type == graphItem::Intervals) {
    if (type == graphItem::Intervals) {
        QVector<double> xvet = xvet0;
        if (plotSettings.getAbscissa() == xpdutils::DVALUE) {
            xvet = xpdutils::dvalue(xvet,wave);
        }
        intervalLimit.visible = visible;
        intervalLimit.wave = wave;
        intervalLimit.setX(xvet);
    }
}

void XpdViewWidget::addPlot(float xvet[], float yvet[], int num, graphItem::ItemType type, int visible, float wave, const QString &name)
{
    QVector<double> xv(num);
    QVector<double> yv(num);
    for (int i = 0; i < num; i++) {
        xv[i] = xvet[i];
        yv[i] = yvet[i];
    }

    addPlot(xv,yv,type,visible,wave,name);
}

void XpdViewWidget::addPlot(float xvet[], int num, graphItem::ItemType type, int visible, float wave)
{
    QVector<double> xv(num);
    for (int i = 0; i < num; i++) {
        xv[i] = xvet[i];
    }

    addPlot(xv,type,visible,wave);
}

void XpdViewWidget::addReflections(float xvet[], int h[], int k[], int l[], int num, int visible, float wave)
{
//Print Reflections
    // for (int i = 0; i < num; i++) {
    //     qInfo() << "Reflection: " << xvet[i] << xpdutils::dvalue(xvet[i], wave) << h[i] << k[i] << l[i];
    // }

    //Fill refSet
    ReflectionBar reflS;
    reflS.wave = wave;
    for (int i = 0; i < num; i++) {
        refInfo r;
        r.x = xvet[i];
        r.hkl[0] = h[i];
        r.hkl[1] = k[i];
        r.hkl[2] = l[i];
        reflS.ref.push_back(r);
    }
    if (plotSettings.getAbscissa() == xpdutils::DVALUE) {
        for (int i = 0; i < reflS.ref.size(); i++) {
            reflS.ref[i].x = xpdutils::dvalue(reflS.ref[i].x, wave);
        }
    }

    if (refSet.size() <= nReflections) { //new graphic element required
        reflS.setColorLine(qualpf::gui::workspaceContext(this).preferences(), nReflections);
        reflS.visible = visible;
        refSet.push_back(reflS);
    } else {
        refSet[nReflections].ref = reflS.ref;
    }

    //Default Name: for more phases use Phase 1, Phase 2, ...
    if (nReflections == 0) {
        refSet[0].setName();
    } else if (nReflections == 1) {
        refSet[0].setName("Phase "+QString::number(1));
        refSet[1].setName("Phase "+QString::number(2));
    } else {
        refSet[nReflections].setName("Phase "+QString::number(nReflections+1));
    }
    nReflections++;
}

void XpdViewWidget::drawPlot()
{
    getYLimits(yLowerRangeNoRef, yUpperRange);

    //Compute space for reflections
    double spaceRef = (yUpperRange - yLowerRangeNoRef) * 0.04; //space for single set of reflections
    double lengthRef = spaceRef * 0.75; // length of the bar
    int nVisibleRef = 0;
    for (int i = 0; i < refSet.size(); ++i) {
        if (refSet.at(i).visible) nVisibleRef++;
    }
    yLowerRange = yLowerRangeNoRef - nVisibleRef*spaceRef; // make space for bars

    //Draw Reflections
    for (int i = 0; i < refSet.size(); i++) {
        QPen mPen(refSet[i].pen);
        refSet[i].graphIndex = graphCount();
        plotWave.append(refSet[i].wave);
        addGraph();
        graph()->setPen(mPen);
        graph()->setName(refSet[i].name);
        double ypos = yLowerRange + (refSet.size()-i-1)*spaceRef;
        drawReflections(refSet[i].ref,ypos,lengthRef,mPen, refSet[i].itemIndexStart, refSet[i].itemIndexEnd);
        refSet[i].yPos = ypos;
        refSet[i].lengthRef = lengthRef;

        //Create custom scatter for legend
        QPainterPath customScatterPath(QPointF(0,-10));
        customScatterPath.lineTo(0,10);
        graph()->setScatterStyle(QCPScatterStyle(customScatterPath, mPen));
        graph()->setLineStyle(QCPGraph::lsNone);
    }

    //Draw Intervals
    drawIntervals();

    if (rescalePlotEnabled()) {
        yAxis->setRange(yLowerRange,yUpperRange);
        updateMinMax();
        xAxis->rescale();
    } else {
        if (refSet.size() > 0) yAxis->setRange(yLowerRange,yUpperRange);
    }

    drawCardPeaks();

    setDraggingLegend(true);
    setLegendPosition(1.0,0.0);

    replot();
}

void XpdViewWidget::redrawPlot(bool computeLimits)
{
    if (computeLimits) getYLimits(yLowerRangeNoRef, yUpperRange);

    //Compute space for reflections
    double spaceRef = (yUpperRange - yLowerRangeNoRef) * 0.04; //space for single set of reflections
    //FIX: problem with log scale for negative value e long bar
    //    spaceRef = spaceRef/100;
    double lengthRef = spaceRef * 0.75; // length of the bar
    int nVisibleRef = 0;
    for (int i = 0; i < refSet.size(); ++i) {
        if (refSet.at(i).visible) nVisibleRef++;
    }
    //yLowerRange -= nVisibleRef*spaceRef; // make space for bars
    yLowerRange = yLowerRangeNoRef - nVisibleRef*spaceRef; // make space for bars
    //qInfo() << "LOW: " << yLowerRange << yLowerRangeNoRef << spaceRef;

    //Redraw reflections
    int iVis = -1;
    for (int i = 0; i < refSet.size(); i++) {
        if (refSet.at(i).visible) {
            ++iVis;
            double ypos = yLowerRange + (nVisibleRef-iVis-1)*spaceRef;
            reDrawReflections(refSet[i].ref,refSet[i],ypos,lengthRef);
            refSet[i].yPos = ypos;
            refSet[i].lengthRef = lengthRef;
        }
    }

    yAxis->setRange(yLowerRange,yUpperRange);
    updateMinMax();

    replot();
}

void XpdViewWidget::setAction(const MouseAction &action)
{
    mAction = action;
    switch (action) {
    case NoZoom:
        setZoomMode(CustomPlotZoom::NoZoom);
        setInteractions(QCP::iRangeZoom | QCP::iSelectLegend | QCP::iSelectPlottables);
        axisRects().at(0)->setRangeZoom(Qt::Horizontal);
        unsetCursor();
        disableTracer();
        savedZoomAction = action;
        break;
    case RectangleZoom:
        setZoomMode(CustomPlotZoom::RectangleZoom);
        setInteractions(QCP::iRangeZoom);
        unsetCursor();
        disableTracer();
        savedZoomAction = action;
        break;
    case HorizontalZoom:
        setZoomMode(CustomPlotZoom::HorizontalZoom);
        setInteractions(QCP::iRangeZoom);
        axisRects().at(0)->setRangeZoom(Qt::Horizontal);
        unsetCursor();
        disableTracer();
        savedZoomAction = action;
        break;
    case Pan:
        setZoomMode(CustomPlotZoom::PanZoom);
        setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
        axisRects().at(0)->setRangeZoom(Qt::Horizontal);
        axisRects().at(0)->setRangeDrag(Qt::Horizontal);
        unsetCursor();
        disableTracer();
        savedZoomAction = action;
        break;
    case AddBackgroundPoint:
    case DeleteBackgroundPoint:
    case AddPeak:
    case AddInferredPeak:
    case DeletePeak:
        setZoomMode(CustomPlotZoom::AddDeletePoint);
        setInteractions(QCP::iRangeZoom);
        axisRects().at(0)->setRangeZoom(Qt::Horizontal);
        setCursor(Qt::CrossCursor);
        if (action == AddPeak || action == AddInferredPeak || action == DeletePeak) {
            createTracer(peaks.getPen().color());
            attachTracerTo(graph(0));
        } else {
            disableTracer();
        }
        break;
    default:
        break;
    }
}

// void XpdViewWidget::drawPeaks()
// {
//     drawGraphicItem_old(peaks);
// }

void XpdViewWidget::drawSelectedPeaks(const QVector<int> &selected)
{
    if (selected.size() == 0) {
        int id = selectedPeak.graphIndex;
        if (id >= 0) {
            graph(id)->setData({}, {});
            replot();
        }
        return;
    }
    int idPeak = peaks.graphIndex;
    if (idPeak < 0) return;

    if (selectedPeak.graphIndex < 0) {
        selectedPeak.graphIndex = graphCount();
        plotWave.append(-1);
        addGraph();
        graph()->setName(selectedPeak.getName());
        graph()->setPen(selectedPeak.getPen());
        QColor peakColor = peaks.getPen().color();
        QCPScatterStyle scatter = QCPScatterStyle(QCPScatterStyle::ssTriangleInverted,peakColor,peakColor,8);
        selectedPeak.setScatter(scatter);
        graph()->setScatterStyle(selectedPeak.getScatter());
    }

    selectedPeak.setX(selected);
    QVector<double> x(selected.size()), y(selected.size());
    for (int i = 0; i < selected.size(); i++) {
        auto row = selected.at(i);
        if (row < 0 || row >= graph(idPeak)->dataCount()) return;
        if (plotSettings.getAbscissa() == xpdutils::DVALUE)
            row = graph(idPeak)->dataCount()-1-row;
        x[i] = graph(idPeak)->data()->at(row)->key;
        y[i] = graph(idPeak)->data()->at(row)->value;
    }

    int id = selectedPeak.graphIndex;
    graph(id)->setData(x,y);

    //force resize of legend
    legend->setMaximumSize(legend->minimumOuterSizeHint());

    replot();
}

void XpdViewWidget::deleteSelectedPeaks()
{
    if (selectedPeak.ixSize() == 0) return;
    emit deleteSelectedPeaksSignal(selectedPeak.getIx());
}

void XpdViewWidget::clearSelectedPeaks()
{
    clearGraphSelection(selectedPeak);
}

void XpdViewWidget::drawSelectedRef()
{
    //TOFIX: combine with setSelectedRef in one function
    if (selectedRef.xSize() > 0) {
        if (selectedRef.graphIndex < 0) {
            selectedRef.graphIndex = graphCount();
            plotWave.append(-1);
            addGraph();
            graph()->setName(selectedRef.getName());
            graph()->setPen(selectedRef.getPen());
            QColor refColor = refSet.at(0).pen.color();
            QCPScatterStyle scatter = QCPScatterStyle(QCPScatterStyle::ssTriangleInverted,refColor,refColor,8);
            selectedRef.setScatter(scatter);
            graph()->setScatterStyle(selectedRef.getScatter());
        }
        int id = selectedRef.graphIndex;
        double ypos = refSet.at(0).yPos + refSet.at(0).lengthRef;
        graph(id)->setData(selectedRef.getX(),QVector<double>(selectedRef.xSize(),ypos));

        replot();
    }
}

void XpdViewWidget::clearSelectedRef()
{
    clearGraphSelection(selectedRef);
}

void XpdViewWidget::clearSelectedComparePoints()
{
    const auto oldGraphs = m_selectedCompareGraphs;
    for (QCPGraph *g : oldGraphs) removeTrackedGraph(g);
}

void XpdViewWidget::drawSelectedComparePoints(const QVector<double> &tth,
                                              const QVector<double> &intensity,
                                              const QVector<QColor> &colors)
{
    clearSelectedComparePoints();

    if (tth.isEmpty() || intensity.isEmpty() || tth.size() != intensity.size()) {
        replot();
        return;
    }

    const auto abscissa = plotSettings.getAbscissa();
    const double wave = plotWave.isEmpty() ? qualpf::application::defaultWavelength() : plotWave.first();
    const QColor expColor = peaks.getPen().color();

    QHash<QRgb, QVector<double>> groupedX;
    QHash<QRgb, QVector<double>> groupedY;

    for (int i = 0; i < tth.size(); ++i) {
        double x = tth[i];
        if (abscissa != xpdutils::TTHETA) {
            if (x <= 0.0) continue;
            x = xpdutils::dvalue(x, wave);
            if (abscissa == xpdutils::ONE_OVER_DVALUE) x = 1 / x;
            else if (abscissa == xpdutils::ONE_OVER_DVALUE2) x = 1 / (x * x);
        }

        const QColor c = (i < colors.size() && colors[i].isValid()) ? colors[i] : expColor;
        const QRgb key = c.rgb();
        groupedX[key].append(x);
        groupedY[key].append(intensity[i]);
    }

    for (auto it = groupedX.constBegin(); it != groupedX.constEnd(); ++it) {
        const QRgb key = it.key();
        const QColor c(key);

        QCPGraph *g = addGraph();
        plotWave.append(wave);   // keeps plotWave in sync with graph count
        g->setLineStyle(QCPGraph::lsNone);
        g->setPen(QPen(c));
        g->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssTriangleInverted, c, c, 8));
        g->setData(it.value(), groupedY.value(key));
        g->setSelectable(QCP::stNone);
        g->removeFromLegend();
        m_selectedCompareGraphs.append(g);
    }

    replot();
}

void XpdViewWidget::setSystematicAbsences(const QVector<int> &refIndex)
{
    if (refSet.size() == 0) return;

    QVector<double> x(refIndex.size());
    for (int i = 0; i < refIndex.size(); i++) {
        //x[i] = refl[0].getX(refIndex.at(i));
        x[i] = refSet[0].ref[refIndex.at(i)].x;
    }
    sysAbs.setX(x);
}

void XpdViewWidget::drawSystematicAbsences()
{
    if (refSet.size() == 0) return;

    if (sysAbs.graphIndex < 0) {
        sysAbs.graphIndex = graphCount();
        plotWave.append(-1);
        addGraph();
        graph()->setName("Systematic Absences");
        graph()->setPen(sysAbs.getPen());
        QColor refColor = refSet.at(0).pen.color();
        QCPScatterStyle scatter = QCPScatterStyle(QCPScatterStyle::ssDiamond,refColor,refColor,8);
        sysAbs.setScatter(scatter);
        graph()->setScatterStyle(sysAbs.getScatter());
        legend->setMaximumSize(legend->minimumOuterSizeHint()); //force resize of legend
    }
    int id = sysAbs.graphIndex;
    double ypos = refSet.at(0).yPos + refSet.at(0).lengthRef/2.0;
    graph(id)->setData(sysAbs.getX(),QVector<double>(sysAbs.xSize(),ypos));

    replot();
}

void XpdViewWidget::clearSystematicAbsences()
{
    int id = sysAbs.graphIndex;
    if (id < 0) return;

    removeGraph(id);
    replot();
    sysAbs.graphIndex = -1;
}

void XpdViewWidget::connectLabels(QLabel *newLabel1, QLabel *newLabel2, QLabel *newLabel3)
{
    labelWidget1 = newLabel1;
    labelWidget2 = newLabel2;
    labelWidget3 = newLabel3;
    connect(this, &CustomPlotZoom::mouseMove, this, &XpdViewWidget::myMoveEvent, Qt::UniqueConnection);
}

void XpdViewWidget::myMoveEvent(QMouseEvent *event)
{
    if (!event || property("workspaceClosing").toBool() ||
        !labelWidget1 || !labelWidget2 || !labelWidget3) return;

    const double x = xAxis->pixelToCoord(event->position().x());
    const double y = yAxis->pixelToCoord(event->position().y());
    labelWidget1->clear();
    labelWidget2->clear();
    labelWidget3->clear();
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    const auto setLabel = [this](QLabel *label, const QString &text) {
        label->setText(fontMetrics().elidedText(text, Qt::ElideRight, label->width() - 10));
    };
    const auto axis = plotSettings.getAbscissa();
    const QString twoTheta = xpdutils::abscissaString(xpdutils::TTHETA);
    setLabel(labelWidget1, QString("%1: %2 I: %3").arg(xpdutils::abscissaString(axis)).arg(x).arg(y));

    int graphIndex = -1, sampleIndex = 0;
    double distance = DBL_MAX;
    for (int i = 0; i < graphCount(); ++i) {
        const auto *curve = graph(i);
        if (!curve->visible() || curve->keyAxis() != xAxis || curve->valueAxis() != yAxis ||
            curve->data()->isEmpty()) continue;
        const int last = curve->data()->size() - 1;
        const int left = std::clamp(curve->findBegin(x, true), 0, last);
        const int right = std::min(left + 1, last);
        const int index = std::abs(curve->data()->at(left)->key - x) <
                          std::abs(curve->data()->at(right)->key - x) ? left : right;
        const auto point = curve->data()->at(index);
        const double dx = point->key - x, dy = point->value - y;
        const double candidateDistance = dx * dx + dy * dy;
        if (candidateDistance < distance) {
            distance = candidateDistance;
            sampleIndex = index;
            graphIndex = i;
        }
    }
    if (graphIndex >= 0 && graphIndex < plotWave.size()) {
        const double wave = plotWave[graphIndex];
        if (std::isfinite(wave) && wave > 0) {
            const auto point = graph(graphIndex)->data()->at(sampleIndex);
            const QString otherAxis = axis == xpdutils::TTHETA ? QStringLiteral("d") : twoTheta;
            const double otherX = axis == xpdutils::TTHETA ? xpdutils::dvalue(point->key, wave) :
                xpdutils::tthvalue(point->key, wave, axis);
            setLabel(labelWidget2, QString("Count: #%1 (%2, %3) %4: %5 Graph: %6")
                .arg(sampleIndex + 1).arg(point->key).arg(point->value).arg(otherAxis).arg(otherX)
                .arg(graph(graphIndex)->name()));
        }
    }

    // ReflectionBar coordinates have already been projected to the current
    // display axis. Empty and hidden bars are never interaction targets.
    int phaseIndex = -1;
    distance = DBL_MAX;
    for (int i = 0; i < refSet.size(); ++i) {
        const auto &bar = refSet[i];
        if (!bar.visible || bar.ref.isEmpty() || !std::isfinite(bar.yPos)) continue;
        const double candidateDistance = std::abs(bar.yPos - y);
        if (candidateDistance <= distance) {
            distance = candidateDistance;
            phaseIndex = i;
        }
    }
    if (phaseIndex < 0) return;
    const auto &bar = refSet[phaseIndex];
    const int index = findRefLocation(bar.ref, x);
    if (index < 0 || index >= bar.ref.size() || !std::isfinite(bar.wave) || bar.wave <= 0) return;
    const auto &reflection = bar.ref[index];
    const double tth = xpdutils::tthvalue(reflection.x, bar.wave, axis);
    const double d = xpdutils::dvalue(tth, bar.wave);
    setLabel(labelWidget3, QString("Refl: #%1 H: %2 K: %3 L: %4 %5: %6 d: %7 Phase: %8")
        .arg(index + 1).arg(reflection.hkl[0]).arg(reflection.hkl[1]).arg(reflection.hkl[2])
        .arg(twoTheta).arg(tth).arg(d).arg(phaseIndex));
}

void XpdViewWidget::xAxisChanged(QCPRange range)
{
    //qInfo() << "ini================================ xAxisChanged";
    //Avoid zoom and drag outside the range
    double xUpperRange,xLowerRange;
    getMinMax(xLowerRange,xUpperRange);
    QCPRange boundedRange = range;
    //qInfo() << "xAxisChanged: " << boundedRange.lower << "-" << boundedRange.upper << xLowerRange << "-" << xUpperRange;

    if (boundedRange.size() > xUpperRange - xLowerRange) {
        boundedRange = QCPRange(xLowerRange,xUpperRange);
    } else {
        double oldSize = boundedRange.size();
        if (boundedRange.lower < xLowerRange) {
            boundedRange.lower = xLowerRange;
            boundedRange.upper = xLowerRange + oldSize;
        }
        if (boundedRange.upper > xUpperRange) {
            boundedRange.upper = xUpperRange;
            boundedRange.lower = xUpperRange - oldSize;
        }
    }

    xAxis->setRange(boundedRange);

    double plotWidth = xUpperRange - xLowerRange;
    double viewportWidth = boundedRange.size() / plotWidth;

    //if (viewportWidth > 1.0)
    if (viewportWidth > 0.9999) {
        horizontalScrollBar->hide();
    }
    else
    {
        //qInfo() << "Viewport: " << viewportWidth << " plotWidth: " << plotWidth << " xLowerRange: " << xLowerRange;
        horizontalScrollBar->show();
        horizontalScrollBar->setPageStep(qRound(scrollBarLen*viewportWidth));
        horizontalScrollBar->setSingleStep((std::min)(scrollBarLen / 100, horizontalScrollBar->pageStep()));
        horizontalScrollBar->setRange(0,scrollBarLen-horizontalScrollBar->pageStep());
        //qInfo() << "Step: " << ui->horizontalScrollBar->pageStep() << "Range: " << boundedRange.size();
        double value;
        if (xAxis->rangeReversed()) {
            value = scrollBarLen*(xUpperRange - boundedRange.upper)/plotWidth;
        } else {
            value = scrollBarLen*(boundedRange.lower - xLowerRange)/plotWidth;
        }
        //qInfo() << "Value: " << value;
        horizontalScrollBar->setValue(qRound(value));
    }

    //qInfo() << "Value: " << ui->horizontalScrollBar->value() << "L: " << boundedRange.lower << "Center: " << boundedRange.center() << "U: " << range.upper;
    //qInfo() << "end================================ xAxisChanged";
}

void XpdViewWidget::horzScrollBarChanged(int value)
{
    //qInfo() << "ini================================ horzScrollChange";
    double xUpperRange,xLowerRange;
    getMinMax(xLowerRange,xUpperRange);
    double plotWidth = xUpperRange - xLowerRange;
    double lower;
    if (xAxis->rangeReversed()) {
        lower = xUpperRange - plotWidth * value /scrollBarLen - xAxis->range().size();
    } else {
        lower = plotWidth * value /scrollBarLen + xLowerRange;
    }
    //qInfo() << "HScrollChanged: " << value << "lower: " << lower << "xLower: " << xLowerRange << "range.lower: " << ui->plot->xAxis->range().lower;
    if (qAbs(xAxis->range().lower - lower) > 0.01) // if user is dragging plot, we don't want to replot twice
    {
        //qInfo() << "horzScrollBarChanged: " << value;
        double upper = lower + xAxis->range().size();
        //qInfo() << "HBar Changed, value: " << value << "L: " << lower << "U: " << upper;
        xAxis->setRange(lower,upper);
        replot();
    }
    //qInfo() << "end================================ horzScrollChange";
}

void XpdViewWidget::addDeletePoint(QPoint mousePos)
{
    int ier;

    if (mAction == AddBackgroundPoint || mAction == AddPeak || mAction == AddInferredPeak) {
        double xval = xAxis->pixelToCoord(mousePos.x());
        double yval = yAxis->pixelToCoord(mousePos.y());
        //process_action_points(mAction, xval, yval, &ier);
        emit addDeletePointSignal(mAction, xval, yval, ier);
        if (mAction == AddPeak || mAction == AddInferredPeak) {
            createTracer(peaks.getPen().color()); //reattach tracer previously deleted in vedi
            attachTracerTo(graph(0));
        }

    } else if (mAction == DeleteBackgroundPoint) {
        int id = bpoints.graphIndex;
        //double radius = qMax(5.0,bpoints.getScatter().size());
        QPointF closePoint = closestDataPoint(mousePos,id);
        if (closePoint.isNull()) return;

        // int xp = xAxis->coordToPixel(closePoint.x());
        // int yp = yAxis->coordToPixel(closePoint.y());
        //QPointF dist = mousePos - QPointF(xp,yp);

        //if (dist.manhattanLength() < radius) {
            //process_action_points(mAction, closePoint.x(), closePoint.y(), &ier);
        emit addDeletePointSignal(mAction, closePoint.x(), closePoint.y(), ier);
        //}

    } else if (mAction == DeletePeak) {
        int id = peaks.graphIndex;
        if (id >= 0) {
            double xMouseCoord = xAxis->pixelToCoord(mousePos.x());
            double closePoint = xMouseCoord;

            // int xp = xAxis->coordToPixel(closePoint);
            // if (abs(mousePos.x() - xp) < 5) {
                //process_action_points(mAction, closePoint, 0, &ier);
            emit addDeletePointSignal(mAction, closePoint, 0, ier);
                createTracer(peaks.getPen().color()); //reattach tracer previously deleted in vedi
                attachTracerTo(graph(0));
            //}
        }

    }
}

void XpdViewWidget::selectionChanged()
{
    // synchronize selection of graphs with selection of corresponding legend items:
    for (int i=0; i<graphCount(); ++i)
    {
        QCPGraph *gr = graph(i);
        if (gr->visible()) {
            QCPPlottableLegendItem *item = legend->itemWithPlottable(gr);
            if (item->selected() || gr->selected())
            {
                item->setSelected(true);
                gr->setSelection(QCPDataSelection(gr->data()->dataRange()));
            }
        }
    }
}

void XpdViewWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void XpdViewWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        if (!urlList.isEmpty()) {
            QStringList fileList;
            fileList.reserve(urlList.size());

            for (auto it = urlList.cbegin(); it != urlList.cend(); ++it) {
                fileList.append(it->toLocalFile());
            }

            //QTimer allows Qt's event loop to complete the drop operation and make the file icon disappear
            QTimer::singleShot(0, this, [this, fileList]() { emit fileDropped(fileList); });
        }
    }
}

void XpdViewWidget::setPlotSettings(const PlotSettings &newPlotSettings)
{
    plotSettings = newPlotSettings;
}

bool XpdViewWidget::updateMinMax()
{
    // Card sticks and selection overlays may extend beyond the measured domain.
    // Scroll and Reset Zoom belong to the measured curves, in display coordinates.
    bool found = false;
    QCPRange range;
    for (const auto &observed : obs) {
        if (observed.graphIndex < 0 || observed.graphIndex >= graphCount()) continue;
        bool valid = false;
        const auto bounds = graph(observed.graphIndex)->getKeyRange(valid);
        if (!valid) continue;
        if (found) range.expand(bounds); else range = bounds;
        found = true;
    }
    if (!found) return CustomPlotZoom::updateMinMax();
    setDataBounds(range.lower, range.upper);
    return true;
}

void XpdViewWidget::convertAbscissa(xpdutils::xAbscissaType from, xpdutils::xAbscissaType to)
{
    xpdutils::convertAbscissa(this, plotWave, from, to);
    for (auto &bar : refSet) {
        for (auto &ref : bar.ref) {
            const double tth = xpdutils::tthvalue(ref.x, bar.wave, from);
            double value = tth;
            if (to != xpdutils::TTHETA) {
                value = xpdutils::dvalue(tth, bar.wave);
                if (to == xpdutils::ONE_OVER_DVALUE) value = 1/value;
                else if (to == xpdutils::ONE_OVER_DVALUE2) value = 1/(value*value);
            }
            ref.x = value;
        }
        std::sort(bar.ref.begin(), bar.ref.end(), [](const auto &a, const auto &b) { return a.x < b.x; });
    }
    refreshAcceptedPhaseBars();
    updateMinMax();
}

PlotSettings XpdViewWidget::pSettings() const
{
    return plotSettings;
}

void XpdViewWidget::addObserved(const QVector<double> &xvet, const QVector<double> &yvet,
                                int visible, float wave, const QString &name)
{
    if (obs.size() <= nObserved) { //new graphic element required
        graphItem item;
        item.setColorLine(qualpf::gui::workspaceContext(this).preferences(), nObserved);
        item.wave = wave;
        obs.push_back(item);
    }
    obs[nObserved].setData(xvet,yvet);
    obs[nObserved].visible = visible;
    obs[nObserved].setName(name);
    makePlot(xvet,yvet,obs[nObserved]);
    graph()->setScatterStyle(obs[nObserved].getScatter());
    nObserved++;
}

void XpdViewWidget::getYLimits(double &yMin, double &yMax)
{
    yMin = DBL_MAX;
    yMax = -DBL_MAX;

    QVector<graphItem>::const_iterator i;
    for (i = obs.constBegin(); i != obs.constEnd(); ++i) {
        if (i->graphIndex >= 0 && i->visible) {
            if (i->min < yMin) yMin = i->min;
            if (i->max > yMax) yMax = i->max;
        }
    }

    if (back.graphIndex >= 0 && back.visible) {
        if (back.min < yMin) yMin = back.min;
        if (back.max > yMax) yMax = back.max;
    }

    if (peaks.graphIndex >= 0 && peaks.visible) {
        if (peaks.min < yMin) yMin = peaks.min;
        if (peaks.max > yMax) yMax = peaks.max;
    }

    if (calc.graphIndex >= 0 && calc.visible) {
        if (calc.min < yMin) yMin = calc.min;
        if (calc.max > yMax) yMax = calc.max;
    }

    if (diff.graphIndex >= 0 && diff.visible) {
        if (diff.min < yMin) yMin = diff.min;
        if (diff.max > yMax) yMax = diff.max;
    }

    if (cdiff.graphIndex >= 0 && cdiff.visible) {
        if (cdiff.min < yMin) yMin = cdiff.min;
        if (cdiff.max > yMax) yMax = cdiff.max;
    }

    if (smooth.graphIndex >= 0 && smooth.visible) {
        if (smooth.min < yMin) yMin = smooth.min;
        if (smooth.max > yMax) yMax = smooth.max;
    }
}

void XpdViewWidget::drawReflections(const QVector<double> &x, double y, double length, QPen pen, int &itemStart, int &itemEnd)
{
    itemStart = itemCount();
    for (int i = 0; i < x.size(); i++) {
        QCPItemLine *line = new QCPItemLine(this);
        line->setPen(pen);
        line->setHead(QCPLineEnding::esNone);
        line->start->setCoords(x[i], y);
        line->end->setCoords(x[i], y+length);
    }
    itemEnd = x.size() + itemStart - 1;
}

void XpdViewWidget::drawReflections(const QVector<refInfo> &ref, double y, double length, QPen pen, int &itemStart, int &itemEnd)
{
    itemStart = itemCount();
    for (int i = 0; i < ref.size(); i++) {
        QCPItemLine *line = new QCPItemLine(this);
        line->setPen(pen);
        line->setHead(QCPLineEnding::esNone);
        line->start->setCoords(ref[i].x, y);
        line->end->setCoords(ref[i].x, y+length);
    }
    itemEnd = ref.size() + itemStart - 1;
}

void XpdViewWidget::drawIntervals()
{
    if (!intervalLimit.visible) return;

    QPen pen1(Qt::gray);
    QPen pen2(Qt::black);
    int ip=0;
    double diff = yUpperRange - yLowerRangeNoRef;
    double offset;
    QVector<double> xval = intervalLimit.getX();
    for (int i = 0; i < xval.size(); i+=2) {
        QCPItemLine *line = new QCPItemLine(this);
        if (ip % 2 == 0) {
            line->setPen(pen1);
            offset = diff * 0.01;
        } else {
            line->setPen(pen2);
            offset = 0.0;
        }
        ip++;
        line->setHead(QCPLineEnding::esSpikeArrow);
        line->setTail(QCPLineEnding::esSpikeArrow);
        line->start->setCoords(xval[i],yLowerRangeNoRef + offset);
        line->end->setCoords(xval[i+1],yLowerRangeNoRef + offset);
    }
}

void XpdViewWidget::reDrawReflections(const QVector<refInfo> &positions, const ReflectionBar &ref, double y, double length)
{
    for (int ind = ref.itemIndexStart; ind <= ref.itemIndexEnd; ind++) {
        QCPItemLine *line = dynamic_cast<QCPItemLine *> (this->item(ind));
        int posRef = ref.itemIndexEnd - ind;
        line->start->setCoords(positions[posRef].x, y);
        line->end->setCoords(positions[posRef].x, y+length);
    }
}

// void XpdViewWidget::reDrawReflections(graphItem &ref, double y, double length)
// {
//     for (int ind = ref.itemIndexStart; ind <= ref.itemIndexEnd; ind++) {
//         QCPItemLine *line = dynamic_cast<QCPItemLine *> (this->item(ind));
//         int posRef = ref.itemIndexEnd - ind;
//         //qInfo() << "Line: " << posRef << ref.getX().at(posRef) << y << y + length;
//         line->start->setCoords(ref.getX().at(posRef), y);
//         line->end->setCoords(ref.getX().at(posRef), y+length);
//     }
// }

void XpdViewWidget::clearGraphSelection(graphItem &item)
{
    int id = item.graphIndex;
    if (id < 0) return;

    removeTrackedGraph(graph(id));
    legend->setMaximumSize(legend->minimumOuterSizeHint());
    replot();
}

void XpdViewWidget::removeTrackedGraph(QCPGraph *target)
{
    int id = -1;
    for (int i = 0; i < graphCount(); ++i) if (graph(i) == target) { id = i; break; }
    if (id < 0) return;
    m_cardPeakGraphs.removeOne(target);
    m_selectedCompareGraphs.removeOne(target);
    removeGraph(target);
    auto reindex = [id](auto &g) { if (g.graphIndex == id) g.graphIndex = -1; else if (g.graphIndex > id) --g.graphIndex; };
    for (auto &g : obs) reindex(g);
    for (auto &g : profCurves) reindex(g);
    for (auto &g : refSet) reindex(g);
    for (auto *g : {&calc, &back, &bpoints, &diff, &cdiff, &peaks, &smooth, &undpeaks,
                   &sysAbs, &selectedRef, &selectedPeak, &intervalLimit}) reindex(*g);
    if (id < plotWave.size()) plotWave.removeAt(id);
}

// void XpdViewWidget::drawGraphicItem_old(graphItem &item)
// {
//     int npoints = get_plot_size(item.gtype);

//     if (npoints > 0) {
//         float *xv = new float[npoints];
//         float *yv = new float[npoints];
//         float wave;
//         get_plot_xy(xv, yv, &wave, item.gtype);

//         QVector<double> xvet(npoints), yvet(npoints);
//         for (int i = 0; i < npoints; i++) {
//             xvet[i] = xv[i];
//             yvet[i] = yv[i];
//         }
//         delete [] xv;
//         delete [] yv;
//         int id = item.graphIndex;
//         if (id < 0) {
//             item.wave = wave;
//             makePlot(xvet,yvet,item);
//         } else {
//             graph(id)->setData(xvet,yvet,true);
//         }

//         legend->setMaximumSize(legend->minimumOuterSizeHint());
//         replot();
//     } else {
//         clearGraphSelection(item);
//     }
// }

void XpdViewWidget::drawGraphicItem(graphItem &item, const QVector<double> &xvet, const QVector<double> &yvet, double wave)
{
    if (xvet.size() > 0) {
        int id = item.graphIndex;
        if (id < 0) {
            item.wave = wave;
            makePlot(xvet,yvet,item);
        } else {
            graph(id)->setData(xvet,yvet,true);
        }

        legend->setMaximumSize(legend->minimumOuterSizeHint());
        replot();
    } else {
        clearGraphSelection(item);
    }
}

void XpdViewWidget::projectSessionCoordinates(QVector<double> &x, QVector<double> &y, double wave) const
{
    const auto abscissa = plotSettings.getAbscissa();
    if (abscissa == xpdutils::TTHETA) return;
    for (auto &value : x) {
        value = xpdutils::dvalue(value, wave);
        if (abscissa == xpdutils::ONE_OVER_DVALUE) value = 1 / value;
        else if (abscissa == xpdutils::ONE_OVER_DVALUE2) value = 1 / (value * value);
    }
    if (abscissa == xpdutils::DVALUE) { std::reverse(x.begin(), x.end()); std::reverse(y.begin(), y.end()); }
}

void XpdViewWidget::drawSessionItem(graphItem &item, QVector<double> x, QVector<double> y, double wave)
{
    projectSessionCoordinates(x, y, wave);
    applyProjectedSessionItem(item, x, y, wave);
}

void XpdViewWidget::applyProjectedSessionItem(graphItem &item, const QVector<double> &x, const QVector<double> &y, double wave)
{
    item.wave = wave;
    if (item.graphIndex >= 0 && item.graphIndex < plotWave.size()) plotWave[item.graphIndex] = wave;
    drawGraphicItem(item, x, y, wave);
}

void XpdViewWidget::setSelectedRef(const QVector<int> &selected)
{
    if (refSet.size() == 0) return;

    QVector<double> x(selected.size());
    for (int i = 0; i < selected.size(); i++) {
        x[i] = refSet[0].ref[selected.at(i)].x;
    }
    selectedRef.setX(x);
}

bool XpdViewWidget::rescalePlotEnabled() const
{
    return rescalePlot;
}

int XpdViewWidget::findRefLocation(const QVector<refInfo> &ref, double xval)
{
    const int n = ref.size();
    if (n == 0 || !std::isfinite(xval)) return -1;
    if (n == 1) return 0;
    int lower = -1, upper = n;
    const bool ascending = ref.back().x >= ref.front().x;
    while (upper - lower > 1) {
        const int middle = lower + (upper - lower) / 2;
        if ((xval >= ref[middle].x) == ascending) lower = middle;
        else upper = middle;
    }
    if (xval == ref.front().x) return 0;
    if (xval == ref.back().x) return n - 1;
    if (lower < 0) return 0;
    if (lower >= n - 1) return n - 1;
    return std::abs(xval - ref[lower].x) < std::abs(xval - ref[lower + 1].x) ? lower : lower + 1;
}

// void XpdViewWidget::makeReflections(const QVector<double> &xvet)
// {
//     int ind = 0;
//     //refl[ind].setLineStyle(Qt::NoPen);
//     refl[ind].setLineConnectionType(QCPGraph::lsNone);
//     refl[ind].graphIndex = graphCount();
//     //refl[ind].setScatter(QCPScatterStyle(QCPScatterStyle::ssDisc));
//     plotWave.append(refl[ind].wave);
//     addGraph();
//     graph()->setName(refl[ind].getName());
//     QVector<double> yvet(xvet.size(),0.0);
//     graph()->setData(xvet,yvet,false);
//     graph()->setPen(refl[ind].getPen());
//     graph()->setLineStyle(refl[ind].getLineConnectionType());
//     //graph()->setScatterStyle(refl[ind].getScatter());
//     // int pix = 10;
//     // //Covert pix in graphic unit of qpainter
//     qreal pixelPoint = 10;
//     //qreal graphPoint = graph()->valueAxis()->pixelToCoord(pixelPoint);
//     qreal graphPoint = yAxis->pixelToCoord(pixelPoint);
//     qInfo() << "PIxel: " << pixelPoint << "Graph: " << graphPoint;

//     QPainterPath customScatterPath(QPointF(0,-pixelPoint));
//     customScatterPath.lineTo(0,pixelPoint);
//     graph()->setScatterStyle(QCPScatterStyle(customScatterPath, refl[ind].getPen(), Qt::black));
// }

void XpdViewWidget::enableRescalePlot(bool value)
{
    rescalePlot = value;
}

void XpdViewWidget::setLegendVisible(bool visible)
{
    plotSettings.legendVisible = visible;
    legend->setVisible(visible);
    replot();
}

void XpdViewWidget::applyOffset(double yOffset)
{
    //Unapply the last yoffset
    double newOffset = -mLastYOffset + yOffset;

    //No offset is applied to the first graph (i=0)
    double totOffset = 0;
    for (int i = 1; i < graphCount(); i++) {
        //totOffset += exp(i*newOffset);
        totOffset = i*newOffset;
        QCPGraphDataContainer *data = graph(i)->data().data();
        for(QCPGraphDataContainer::iterator it = data->begin(); it != data->end(); it++)
        {
            it->value += totOffset;
        }
    }

    //Apply offset to the y uppper range
    yUpperRange += totOffset;
    if (refSet.size() > 0) {
        redrawPlot(false);
    } else {
        yAxis->setRange(yLowerRange,yUpperRange);
    }

    //Save last applied offset;
    mLastYOffset = yOffset;
}

void XpdViewWidget::applyAutoScale()
{
    // old method
    //Compute min and max along the yaxis in the visible range
    //Provare anche per ogni plot QCPRange range = ui->plot->graph(0)->getKeyRange(found);
    //    double yLowerRange = DBL_MAX;
    //    double yUpperRange = -DBL_MAX;
    //    double xLowerRange = ui->plot->xAxis->range().lower;
    //    double xUpperRange = ui->plot->xAxis->range().upper;
    //    for (int i = 0; i < ui->plot->graphCount(); i++) {
    //        QCPGraphDataContainer::const_iterator it = ui->plot->graph(i)->data()->constBegin();
    //        QCPGraphDataContainer::const_iterator end = ui->plot->graph(i)->data()->constEnd();;
    //        while (it != end)
    //        {
    //            if (it->key >= xLowerRange && it->key <= xUpperRange) {
    //                if (it->value < yLowerRange) yLowerRange = it->value;
    //                if (it->value > yUpperRange) yUpperRange = it->value;
    //            }
    //            it++;
    //        }
    //    }
    //    if (ui->plot->yAxis->range().upper != yUpperRange) {
    //        //double yLower = ui->plot->yAxis->range().lower;
    //        ui->plot->yAxis->setRange(yLower(),yUpperRange);
    //        ui->plot->replot();
    //        qInfo() << "Autoscale: " << yUpperRange;
    //    }

    //altro metodo
    double Lower = xAxis->range().lower;
    double Upper = xAxis->range().upper;
    //yLowerRange = Lower;
    double yUpperAutoScale = -DBL_MAX;
    double yLowerAutoScale = DBL_MAX;
    for (int i = 0; i < graphCount(); i++) {
        if (!graph(i)->data()->isEmpty() && graph(i)->visible()) {
            //int index_left = ui->plot->graph(i)->findBegin(Lower, false);
            //if (ui->plot->graph(0)->data()->at(index_left)->key < Lower) index_left++;
            //int index_right = ui->plot->graph(i)->findEnd(Upper, false) - 1;
            //double max = max_element(ui->plot->graph(i)->data()->at(index_left),ui->plot->graph(i)->data()->at(index_right));
            //qInfo() << "G: " << i << "Index L: " << index_left << "Index R: " << index_right << ui->plot->graph(i)->data()->size();
            QCPGraphDataContainer::const_iterator begin = graph(i)->data()->findBegin(Lower, false);
            QCPGraphDataContainer::const_iterator end = graph(i)->data()->findBegin(Upper, false) - 1;
            // end > begin for selection between two points (empty range!)
            if (begin <= end) {
                //warning
                //                const QCPGraphData* maxData = std::max_element(begin, end,
                //                                              [](QCPGraphData a, QCPGraphData b){return a.value < b.value;});
                const auto maxData = std::max_element(begin, end,
                                                      [](const QCPGraphData& a, const QCPGraphData& b){return a.value < b.value;});
                const auto minData = std::min_element(begin, end,
                                                      [](const QCPGraphData& a, const QCPGraphData& b){return a.value < b.value;});
                if (yUpperAutoScale < maxData->value) yUpperAutoScale = maxData->value;
                if (yLowerAutoScale > minData->value) yLowerAutoScale = minData->value;
            }
        }
    }
    if (yAxis->range().upper != yUpperAutoScale) {
        bool lowerChanged = (yAxis->range().lower != yLowerAutoScale);
        double yLowerRangeSav;
        if (lowerChanged) {
            yLowerRangeSav = yLowerRange;
            yLowerRangeNoRef = yLowerAutoScale;
        }
        //TOFIX: improve this part, yUpperRange and yLowerRange should not be changed to avoid problem at reset
        double yUppperRangeSav = yUpperRange;
        yUpperRange = yUpperAutoScale;
        redrawPlot(false);
        yUpperRange = yUppperRangeSav;
        if (lowerChanged) {
            yLowerRange = yLowerRangeSav;
        }
    }
}

void XpdViewWidget::connectHorizontalScrollBar(QScrollBar *newHorizontalScrollBar)
{
    horizontalScrollBar = newHorizontalScrollBar;
    horizontalScrollBar->setPageStep(scrollBarLen);
    horizontalScrollBar->hide();
    connect(horizontalScrollBar, &QScrollBar::valueChanged, this, &XpdViewWidget::horzScrollBarChanged);
    connect(xAxis, QOverload<const QCPRange &>::of(&QCPAxis::rangeChanged), this, &XpdViewWidget::xAxisChanged);
}

void XpdViewWidget::connectAddDeletePoints()
{
    connect(this, &CustomPlotZoom::addDeletePointAction, this, &XpdViewWidget::addDeletePoint);
}

void XpdViewWidget::connectSelectionChanged()
{
    connect(this, &CustomPlotZoom::selectionChangedByUser, this, &XpdViewWidget::selectionChanged);
}

void XpdViewWidget::setCardPeaks(const QVector<CardPeakData> &peaks)
{
    const auto oldGraphs = m_cardPeakGraphs;
    for (QCPGraph *g : oldGraphs)
        removeTrackedGraph(g);

    m_cardPeaks = peaks;
    drawCardPeaks();
    replot();
}

void XpdViewWidget::drawCardPeaks()
{
    if (m_cardPeaks.isEmpty()) return;

    const auto abscissa = plotSettings.getAbscissa();

    for (const CardPeakData &cpd : m_cardPeaks) {
        QVector<double> xData = abscissa == xpdutils::TTHETA ? cpd.tth : cpd.d;
        if (xData.isEmpty() || cpd.intensity.size() != xData.size()) continue;
        if (abscissa == xpdutils::ONE_OVER_DVALUE)
            for (auto &x : xData) x = 1 / x;
        else if (abscissa == xpdutils::ONE_OVER_DVALUE2)
            for (auto &x : xData) x = 1 / (x * x);

        QCPGraph *g = addGraph();
        plotWave.append(cpd.wave);   // keeps plotWave in sync with graph count
        g->setName(cpd.id);
        g->setPen(QPen(cpd.color, 1.5));
        g->setLineStyle(QCPGraph::lsImpulse);
        g->setData(xData, cpd.intensity);
        g->setSelectable(QCP::stNone);
        m_cardPeakGraphs.append(g);
    }
}

void XpdViewWidget::addPhaseReflections(const CardType &card, const QColor &color)
{
    const auto status = presentPhaseReflections(card, color);
    if (!status) emit plotError(QString::fromStdString(status.error().reason + ": " + status.error().message));
}

qualpf::Status XpdViewWidget::presentPhaseReflections(const CardType &card, const QColor &color)
{
    using namespace qualpf::application::presentation;
    CardOverlayInput input;
    input.id = card.getId().toStdString();
    input.chemical_name = card.getChemicalName().toStdString();
    input.mineral_name = card.getMineralName().toStdString();
    const auto twoTheta = card.getTth();
    const auto dSpacing = card.getD();
    input.two_theta.assign(twoTheta.begin(), twoTheta.end());
    input.d_spacing.assign(dSpacing.begin(), dSpacing.end());
    input.display_wavelength = plotWave.isEmpty() ? qualpf::application::defaultWavelength() : plotWave.first();
    auto projected = project_card_overlay(input, qualpf::gui::QtPlotSink::abscissa(*this));
    if (!projected) return qualpf::Status{projected.error()};
    PlotUpdate update;
    update.snapshot.abscissa = qualpf::gui::QtPlotSink::abscissa(*this);
    update.snapshot.overlays.push_back(projected.value());
    update.commands.push_back({CommandKind::upsert, 1, std::move(projected).value()});
    qualpf::gui::QtPlotSink sink(this);
    sink.setCardColor(color);
    return sink.apply(update);
}

void XpdViewWidget::applyProjectedCard(const qualpf::application::presentation::PlotSeries &series, const QColor &color)
{
    const QString id = QString::fromStdString(series.identity);
    if (series.x.empty()) {
        removePhaseReflections(id);
        return;
    }

    ReflectionBar rb;
    rb.id      = id;
    rb.visible = series.visibility == qualpf::application::Visibility::visible;
    rb.wave    = series.wavelength;

    for (auto x : series.x) {
        refInfo r;
        r.hkl[0] = r.hkl[1] = r.hkl[2] = 0;
        r.x = x;
        rb.ref.push_back(r);
    }

    rb.pen = QPen(color, 1);
    rb.setName(QString::fromStdString(series.label));
    rb.itemIndexStart = -1;
    rb.itemIndexEnd   = -1;

    for (auto &existing : refSet) {
        if (existing.id != id) continue;
        if (existing.ref.size() == rb.ref.size() && std::equal(existing.ref.begin(), existing.ref.end(),
                rb.ref.begin(), [](const auto &a, const auto &b) { return a.x == b.x; }) &&
                existing.visible == rb.visible && existing.wave == rb.wave &&
                existing.pen == rb.pen && existing.name == rb.name) return;
        clearReflectionItems();
        existing.ref = rb.ref;
        existing.visible = rb.visible;
        existing.wave = rb.wave;
        existing.pen = rb.pen;
        existing.name = rb.name;
        if (labelWidget3) labelWidget3->clear();
        refreshAcceptedPhaseBars();
        return;
    }
    clearReflectionItems();
    refSet.push_back(rb);

    refreshAcceptedPhaseBars();
}

void XpdViewWidget::removePhaseReflections(const QString &id)
{
    if (labelWidget3) labelWidget3->clear();
    for (int k = refSet.size() - 1; k >= 0; k--) {
        if (refSet[k].id == id) {
            clearReflectionItems();
            // Remove the dummy legend graph and keep plotWave in sync
            const int gIdx = refSet[k].graphIndex;
            if (gIdx >= 0 && gIdx < graphCount()) {
                removeTrackedGraph(graph(gIdx));
            }
            refSet.removeAt(k);
        }
    }
    refreshAcceptedPhaseBars();
}

void XpdViewWidget::refreshAcceptedPhaseBars()
{
    if (graphCount() == 0) return; // no pattern displayed; bars drawn on next drawPlot()

    double spaceRef = (yUpperRange - yLowerRangeNoRef) * 0.04;
    double lengthRef = spaceRef * 0.75;

    clearReflectionItems();

    // Recompute y range
    int nVisibleRef = 0;
    for (const auto &r : refSet) if (r.visible) nVisibleRef++;
    yLowerRange = yLowerRangeNoRef - nVisibleRef * spaceRef;

    // Re-draw all reflection bars at updated positions
    int iVis = -1;
    for (int i = 0; i < refSet.size(); i++) {
        if (!refSet.at(i).visible) continue;
        ++iVis;
        double ypos = yLowerRange + (nVisibleRef - iVis - 1) * spaceRef;
        QPen mPen(refSet[i].pen);
        drawReflections(refSet[i].ref, ypos, lengthRef, mPen,
                        refSet[i].itemIndexStart, refSet[i].itemIndexEnd);
        refSet[i].yPos = ypos;
        refSet[i].lengthRef = lengthRef;
    }

    // Add dummy legend graph for phases that don't have one yet (graphIndex == -1).
    // This happens when a phase is accepted while a pattern is already displayed
    // (drawPlot() was already called and won't run again until the next pattern load).
    for (int i = 0; i < refSet.size(); i++) {
        if (refSet[i].graphIndex >= 0) continue;
        QPen mPen(refSet[i].pen);
        refSet[i].graphIndex = graphCount();
        plotWave.append(refSet[i].wave);
        addGraph();
        graph()->setPen(mPen);
        graph()->setName(refSet[i].name);
        QPainterPath customScatterPath(QPointF(0,-10));
        customScatterPath.lineTo(0,10);
        graph()->setScatterStyle(QCPScatterStyle(customScatterPath, mPen));
        graph()->setLineStyle(QCPGraph::lsNone);
    }
    legend->setMaximumSize(legend->minimumOuterSizeHint());

    yAxis->setRange(yLowerRange, yUpperRange);
    updateMinMax();
    replot();
}

void XpdViewWidget::clearReflectionItems()
{
    // Collect before removing; QCustomPlot item indices shift on each removal.
    QList<QCPAbstractItem *> items;
    for (auto &bar : refSet) {
        for (int i = bar.itemIndexStart; i >= 0 && i <= bar.itemIndexEnd && i < itemCount(); ++i)
            if (!items.contains(item(i))) items.append(item(i));
        bar.itemIndexStart = bar.itemIndexEnd = -1;
    }
    for (auto *entry : items) removeItem(entry);
}
