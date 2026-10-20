#include "gui/plotting/xpdviewwidget.h"
#include "gui/presentation/project_view_values.h"
#include <QSignalBlocker>
#include <array>

namespace {
using namespace qualpf::application;
using qualpf::gui::projectColor;
using qualpf::gui::projectRgba;
constexpr std::pair<Qt::PenStyle, const char *> penStyles[]{
    {Qt::NoPen, "none"},           {Qt::SolidLine, "solid"},      {Qt::DashLine, "dash"},
    {Qt::DotLine, "dot"},          {Qt::DashDotLine, "dash_dot"}, {Qt::DashDotDotLine, "dash_dot_dot"},
    {Qt::CustomDashLine, "custom"}};
constexpr std::pair<Qt::PenCapStyle, const char *> capStyles[]{
    {Qt::FlatCap, "flat"}, {Qt::SquareCap, "square"}, {Qt::RoundCap, "round"}};
constexpr std::pair<Qt::PenJoinStyle, const char *> joinStyles[]{{Qt::MiterJoin, "miter"},
                                                                 {Qt::BevelJoin, "bevel"},
                                                                 {Qt::RoundJoin, "round"},
                                                                 {Qt::SvgMiterJoin, "svg_miter"}};
constexpr std::pair<QCPGraph::LineStyle, const char *> connections[]{{QCPGraph::lsNone, "none"},
                                                                     {QCPGraph::lsLine, "line"},
                                                                     {QCPGraph::lsStepLeft, "step_left"},
                                                                     {QCPGraph::lsStepRight, "step_right"},
                                                                     {QCPGraph::lsStepCenter, "step_center"},
                                                                     {QCPGraph::lsImpulse, "impulse"}};
constexpr std::pair<QCPScatterStyle::ScatterShape, const char *> scatters[]{
    {QCPScatterStyle::ssNone, "none"},
    {QCPScatterStyle::ssDot, "dot"},
    {QCPScatterStyle::ssCross, "cross"},
    {QCPScatterStyle::ssPlus, "plus"},
    {QCPScatterStyle::ssCircle, "circle"},
    {QCPScatterStyle::ssDisc, "disc"},
    {QCPScatterStyle::ssSquare, "square"},
    {QCPScatterStyle::ssDiamond, "diamond"},
    {QCPScatterStyle::ssStar, "star"},
    {QCPScatterStyle::ssTriangle, "triangle"},
    {QCPScatterStyle::ssTriangleInverted, "triangle_inverted"},
    {QCPScatterStyle::ssCrossSquare, "cross_square"},
    {QCPScatterStyle::ssPlusSquare, "plus_square"},
    {QCPScatterStyle::ssCrossCircle, "cross_circle"},
    {QCPScatterStyle::ssPlusCircle, "plus_circle"},
    {QCPScatterStyle::ssPeace, "peace"}};
constexpr std::pair<ColorWidget::ColorType, const char *> backgrounds[]{
    {ColorWidget::TYPE_BLACK, "black"},
    {ColorWidget::TYPE_WHITE, "white"},
    {ColorWidget::TYPE_FLAT, "flat"},
    {ColorWidget::TYPE_HGRAD, "horizontal"},
    {ColorWidget::TYPE_VGRAD, "vertical"},
    {ColorWidget::TYPE_DGRAD, "diagonal"},
    {ColorWidget::TYPE_DGRAD2, "reverse_diagonal"},
    {ColorWidget::TYPE_RGRAD, "radial"}};
constexpr std::pair<xpdutils::xAbscissaType, const char *> axes[]{
    {xpdutils::TTHETA, "two_theta"},
    {xpdutils::DVALUE, "d_spacing"},
    {xpdutils::ONE_OVER_DVALUE, "inverse_d"},
    {xpdutils::ONE_OVER_DVALUE2, "inverse_d_squared"}};
template <class T, std::size_t N> std::string token(T v, const std::pair<T, const char *> (&entries)[N]) {
    for (const auto &[e, name] : entries)
        if (e == v)
            return name;
    return entries[0].second;
}
template <class T, std::size_t N>
T native(const std::string &s, const std::pair<T, const char *> (&entries)[N]) {
    for (const auto &[e, name] : entries)
        if (s == name)
            return e;
    return entries[0].first;
}
ProjectPen portable(const QPen &p) {
    ProjectPen out;
    out.rgba = projectRgba(p.color());
    out.width = p.widthF();
    out.style = token(p.style(), penStyles);
    const auto dash = p.dashPattern();
    out.dash_pattern.assign(dash.begin(), dash.end());
    out.cap = token(p.capStyle(), capStyles);
    out.join = token(p.joinStyle(), joinStyles);
    out.cosmetic = p.isCosmetic();
    return out;
}
QPen native(const ProjectPen &p) {
    QPen out(projectColor(p.rgba));
    out.setWidthF(p.width);
    out.setStyle(native(p.style, penStyles));
    out.setCapStyle(native(p.cap, capStyles));
    out.setJoinStyle(native(p.join, joinStyles));
    out.setCosmetic(p.cosmetic);
    if (p.style == "custom")
        out.setDashPattern(QVector<double>(p.dash_pattern.begin(), p.dash_pattern.end()));
    return out;
}
ProjectCurveStyle portable(const graphItem &item) {
    ProjectCurveStyle out;
    out.pen = portable(item.getPen());
    const auto scatter = item.getScatter();
    out.scatter = token(scatter.shape(), scatters);
    out.scatter_size = scatter.size();
    out.scatter_pen = portable(scatter.pen());
    out.scatter_pen_defined = scatter.isPenDefined();
    out.scatter_fill = scatter.brush().style() != Qt::NoBrush;
    out.scatter_rgba = projectRgba(scatter.brush().color());
    out.connection = token(item.getLineConnectionType(), connections);
    out.visible = item.visible;
    out.label = item.getName().toStdString();
    return out;
}
void apply(graphItem &item, const ProjectCurveStyle &v) {
    item.setPen(native(v.pen));
    item.visible = v.visible;
    item.setName(QString::fromStdString(v.label));
    item.setLineConnectionType(native(v.connection, connections));
    QCPScatterStyle scatter;
    scatter.setShape(native(v.scatter, scatters));
    scatter.setSize(v.scatter_size);
    if (v.scatter_pen_defined)
        scatter.setPen(native(v.scatter_pen));
    scatter.setBrush(v.scatter_fill ? QBrush(projectColor(v.scatter_rgba)) : QBrush(Qt::NoBrush));
    item.setScatter(scatter);
}
} // namespace
qualpf::application::ProjectViewState XpdViewWidget::captureProjectView(ProjectViewState out) const {
    out.axis = token(plotSettings.getAbscissa(), axes);
    out.y_scale = plotSettings.yScaleType == QCPAxis::stLinear ? "linear" : "logarithmic";
    const auto xr = xAxis->range(), yr = yAxis->range();
    if (xr.lower < xr.upper && yr.lower < yr.upper && (out.y_scale != "logarithmic" || yr.lower > 0))
        out.bounds = std::array{xr.lower, xr.upper, yr.lower, yr.upper};
    out.offset_enabled = plotSettings.hasOffset;
    out.offset = plotSettings.yOffset;
    out.legend = plotSettings.legendVisible;
    out.x_tick_labels = plotSettings.xTickLabels;
    out.y_tick_labels = plotSettings.yTickLabels;
    out.background_style = token(plotSettings.backColorType, backgrounds);
    out.layer_style = token(plotSettings.layerColorType, backgrounds);
    out.background_colors = {projectRgba(plotSettings.backColor), projectRgba(plotSettings.backColor1),
                             projectRgba(plotSettings.backColor2)};
    out.layer_colors = {projectRgba(plotSettings.layerColor), projectRgba(plotSettings.layerColor1),
                        projectRgba(plotSettings.layerColor2)};
    out.pens["x_axis"] = portable(plotSettings.xAxisPen);
    out.pens["y_axis"] = portable(plotSettings.yAxisPen);
    out.pens["x_axis_secondary"] = portable(plotSettings.xAxis2Pen);
    out.pens["y_axis_secondary"] = portable(plotSettings.yAxis2Pen);
    out.pens["x_grid"] = portable(plotSettings.xGridPen);
    out.pens["y_grid"] = portable(plotSettings.yGridPen);
    out.curves.clear();
    for (qsizetype i = 0; i < obs.size(); ++i)
        out.curves["observed/" + std::to_string(i)] = portable(obs[i]);
    const std::pair<const char *, const graphItem *> items[]{
        {"calculated", &calc},        {"background", &back},          {"background_points", &bpoints},
        {"difference", &diff},        {"cumulative", &cdiff},         {"peaks", &peaks},
        {"smoothing", &smooth},       {"unindexed_peaks", &undpeaks}, {"absences", &sysAbs},
        {"intervals", &intervalLimit}};
    for (const auto &[key, item] : items)
        out.curves[key] = portable(*item);
    for (const auto &ref : refSet) {
        auto &c = out.curves["phase/" + ref.id.toStdString()];
        c.pen = portable(ref.pen);
        c.visible = ref.visible;
        c.label = ref.name.toStdString();
    }
    return out;
}
void XpdViewWidget::applyProjectView(const ProjectViewState &v) {
    const QSignalBlocker xBlock(xAxis), yBlock(yAxis);
    const auto oldAxis = plotSettings.getAbscissa();
    const auto axis = native(v.axis, axes);
    if (oldAxis != axis && !obs.empty())
        convertAbscissa(oldAxis, axis);
    plotSettings.setAbscissa(axis);
    plotSettings.yScaleType = v.y_scale == "linear" ? QCPAxis::stLinear : QCPAxis::stLogarithmic;
    plotSettings.hasOffset = v.offset_enabled;
    plotSettings.yOffset = v.offset;
    plotSettings.legendVisible = v.legend;
    plotSettings.xTickLabels = v.x_tick_labels;
    plotSettings.yTickLabels = v.y_tick_labels;
    plotSettings.backColorType = native(v.background_style, backgrounds);
    plotSettings.layerColorType = native(v.layer_style, backgrounds);
    plotSettings.backColor = projectColor(v.background_colors[0]);
    plotSettings.backColor1 = projectColor(v.background_colors[1]);
    plotSettings.backColor2 = projectColor(v.background_colors[2]);
    plotSettings.layerColor = projectColor(v.layer_colors[0]);
    plotSettings.layerColor1 = projectColor(v.layer_colors[1]);
    plotSettings.layerColor2 = projectColor(v.layer_colors[2]);
    const std::pair<const char *, QPen *> pens[]{{"x_axis", &plotSettings.xAxisPen},
                                                 {"y_axis", &plotSettings.yAxisPen},
                                                 {"x_axis_secondary", &plotSettings.xAxis2Pen},
                                                 {"y_axis_secondary", &plotSettings.yAxis2Pen},
                                                 {"x_grid", &plotSettings.xGridPen},
                                                 {"y_grid", &plotSettings.yGridPen}};
    for (const auto &[key, p] : pens)
        if (auto found = v.pens.find(key); found != v.pens.end())
            *p = native(found->second);
    plotSettings.applyTo(this);
    xAxis->setRangeReversed(axis == xpdutils::DVALUE);
    auto restore = [&](const std::string &key, graphItem &item) {
        if (auto found = v.curves.find(key); found != v.curves.end())
            apply(item, found->second);
        if (item.graphIndex >= 0 && item.graphIndex < graphCount()) {
            auto *g = graph(item.graphIndex);
            g->setPen(item.getPen());
            g->setScatterStyle(item.getScatter());
            g->setLineStyle(item.getLineConnectionType());
            g->setVisible(item.visible);
            g->setName(item.getName());
        }
    };
    for (qsizetype i = 0; i < obs.size(); ++i)
        restore("observed/" + std::to_string(i), obs[i]);
    const std::pair<const char *, graphItem *> items[]{
        {"calculated", &calc},        {"background", &back},          {"background_points", &bpoints},
        {"difference", &diff},        {"cumulative", &cdiff},         {"peaks", &peaks},
        {"smoothing", &smooth},       {"unindexed_peaks", &undpeaks}, {"absences", &sysAbs},
        {"intervals", &intervalLimit}};
    for (const auto &[key, item] : items)
        restore(key, *item);
    for (auto &ref : refSet)
        if (auto found = v.curves.find("phase/" + ref.id.toStdString()); found != v.curves.end()) {
            ref.pen = native(found->second.pen);
            ref.visible = found->second.visible;
            ref.name = QString::fromStdString(found->second.label);
        }
    applyOffset(v.offset_enabled ? v.offset : 0);
    if (!obs.empty())
        redrawPlot(false);
    if (v.bounds) {
        xAxis->setRange((*v.bounds)[0], (*v.bounds)[1]);
        yAxis->setRange((*v.bounds)[2], (*v.bounds)[3]);
    }
    replot();
}
