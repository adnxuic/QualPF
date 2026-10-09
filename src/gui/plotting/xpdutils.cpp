#include "gui/plotting/customplotzoom.h"
#include "gui/plotting/xpdutils.h"
#include "qualpf/application/plot_coordinates.h"
#include <QVector>
#include <cmath>

namespace xpdutils {
static_assert(int(qualpf::application::PlotAxis::two_theta) == TTHETA &&
              int(qualpf::application::PlotAxis::d_spacing) == DVALUE &&
              int(qualpf::application::PlotAxis::inverse_d) == ONE_OVER_DVALUE &&
              int(qualpf::application::PlotAxis::inverse_d_squared) == ONE_OVER_DVALUE2);
double dvalue(double ttheta, double wave) { return qualpf::application::plotDSpacing(ttheta, wave); }
double tthvalue(double d, double wave) { return qualpf::application::plotTwoThetaFromD(d, wave); }
double tthvalue(double x, double wave, xAbscissaType axis) {
    return qualpf::application::plotTwoTheta(x, wave, static_cast<qualpf::application::PlotAxis>(axis));
}

QVector<double> dvalue(const QVector<double> &ttheta, double wave) {
    QVector<double> dvector(ttheta.size());
    for (int i = 0; i < ttheta.size(); i++) {
        int j = ttheta.size() - i - 1;
        dvector[i] = dvalue(ttheta.at(j), wave);
    }
    return dvector;
}

QVector<double> tthvalue(const QVector<double> &d, double wave)
{
    QVector<double> tthvector(d.size());
    for (int i = 0; i < d.size(); i++) {
        tthvector[i] = tthvalue(d.at(i), wave);
    }
    return tthvector;
}

QVector<double> tthvalue_safe(const QVector<double> &d, double wave)
{
    QVector<double> tthvector;
    tthvector.reserve(d.size()); // opzionale, migliora performance

    for (int i = 0; i < d.size(); i++) {
        double tth = tthvalue(d.at(i), wave);
        if (!std::isnan(tth)) {
            tthvector.append(tth);
        }
        // Se è NaN, non viene aggiunto
    }
    return tthvector;
}

void tthetaToD(QVector<double> &xvet, QVector<double> &yvet, double wave) {
    QVector<double> xvetc = xvet;
    QVector<double> yvetc = yvet;
    for (int i = 0; i < xvetc.size(); i++) {
        int j = xvetc.size() - i - 1;
        xvet[i] = dvalue(xvetc.at(j),wave);
        yvet[i] = yvetc.at(j);
    }
}

void convertTtheta2D(CustomPlotZoom *plot, const QVector<double>& wave, xAbscissaType dType) {
    //Covert all plots
    for (int i = 0; i < plot->graphCount(); i++) {
        for (auto it = plot->graph(i)->data()->begin(); it != plot->graph(i)->data()->end(); it++) {
            it->key = qualpf::application::plotFromTwoTheta(it->key, wave.at(i), static_cast<qualpf::application::PlotAxis>(dType));
        }
        if (dType == xpdutils::DVALUE) {
            std::reverse(plot->graph(i)->data()->begin(),plot->graph(i)->data()->end());
        }
    }

    plot->xAxis->setLabel(abscissaString(dType));
}

void convertD2Ttheta(CustomPlotZoom *plot, const QVector<double>& wave, xAbscissaType dType) {
    //Convert all plots
    for (int i = 0; i < plot->graphCount(); i++) {
        for (auto it = plot->graph(i)->data()->begin(); it != plot->graph(i)->data()->end(); it++) {
            it->key = qualpf::application::plotFromSpacing(it->key, wave.at(i), static_cast<qualpf::application::PlotAxis>(dType));
        }
        if (dType == DVALUE) std::reverse(plot->graph(i)->data()->begin(),plot->graph(i)->data()->end());
    }

    plot->xAxis->setLabel(abscissaString(xpdutils::TTHETA));
}

void convertAbscissa(CustomPlotZoom *plot, const QVector<double> &wave, xpdutils::xAbscissaType from, xpdutils::xAbscissaType to)
{
    if (from == xpdutils::TTHETA) {
        convertTtheta2D(plot, wave, to);
    } else {
        if (to == xpdutils::TTHETA) {
            convertD2Ttheta(plot, wave, from);
        } else {
            convertD(plot, from, to);
        }
    }
}

QString abscissaString(xAbscissaType aType)
{
    switch (aType) {
    case TTHETA: return "2"+QString(QChar(0x03B8));
    case DVALUE: return "d";
    case ONE_OVER_DVALUE: return "1/d";
    case ONE_OVER_DVALUE2: return "q";
    default: return QString();
    }
}

void convertD(CustomPlotZoom *plot, xAbscissaType from, xAbscissaType to)
{
    if (from == DVALUE || from == ONE_OVER_DVALUE || from == ONE_OVER_DVALUE2) {
        for (int i = 0; i < plot->graphCount(); ++i) {
            for (auto it = plot->graph(i)->data()->begin(); it != plot->graph(i)->data()->end(); ++it)
                it->key = qualpf::application::plotBetweenSpacings(it->key,
                    static_cast<qualpf::application::PlotAxis>(from), static_cast<qualpf::application::PlotAxis>(to));
            if (from == DVALUE || to == DVALUE)
                std::reverse(plot->graph(i)->data()->begin(), plot->graph(i)->data()->end());
        }
    }
    plot->xAxis->setLabel(abscissaString(to));
}

}

