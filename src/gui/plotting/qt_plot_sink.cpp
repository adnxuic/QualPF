#include "gui/plotting/qt_plot_sink.h"
#include "gui/plotting/xpdviewwidget.h"
#include <QThread>
#include <algorithm>
#include <cmath>

namespace qualpf::gui {
using namespace application;
using namespace application::presentation;
namespace {
Error invalid(std::string reason) {
    return {ErrorCode::invalid_input, "plot.sink", "Cannot apply the plot update", std::move(reason)};
}
QVector<double> values(const std::vector<double> &v) { return {v.begin(), v.end()}; }
}
QtPlotSink::QtPlotSink(XpdViewWidget *view) : view_(view) {}
Abscissa QtPlotSink::abscissa(const XpdViewWidget &view) {
    return static_cast<Abscissa>(view.pSettings().getAbscissa());
}
Status QtPlotSink::validate(const PlotUpdate &update) const {
    if (!view_) return Status{invalid("view_destroyed")};
    if (view_->property("workspaceClosing").toBool()) return Status{invalid("workspace_closing")};
    if (view_->thread() != QThread::currentThread()) return Status{invalid("wrong_thread")};
    for (const auto &command : update.commands) {
        if (command.kind < CommandKind::rescale || command.kind > CommandKind::redraw)
            return Status{invalid("invalid_command")};
        if (command.kind != CommandKind::upsert && command.kind != CommandKind::erase) {
            if (command.series) return Status{invalid("unexpected_series")};
            continue;
        }
        if (!command.series) return Status{invalid("missing_series")};
        const auto &s = *command.series;
        if (s.kind < SeriesKind::observed || s.kind > SeriesKind::preprocessing_preview || s.identity.empty())
            return Status{invalid("invalid_series_identity")};
        if (s.kind != SeriesKind::accepted_card && s.x.size() != s.y.size())
            return Status{invalid("invalid_series_dimensions")};
        if (!std::isfinite(s.wavelength) || !std::all_of(s.x.begin(), s.x.end(), [](double v) { return std::isfinite(v); }) ||
            !std::all_of(s.y.begin(), s.y.end(), [](double v) { return std::isfinite(v); }))
            return Status{invalid("nonfinite_series")};
        if (s.visibility < Visibility::disabled || s.visibility > Visibility::visible)
            return Status{invalid("invalid_visibility")};
        if (s.kind != SeriesKind::accepted_card && s.kind != SeriesKind::preprocessing_preview &&
            s.kind != SeriesKind::observed && s.kind != SeriesKind::peaks &&
            s.kind != SeriesKind::background && s.kind != SeriesKind::background_points &&
            s.kind != SeriesKind::calculated && s.kind != SeriesKind::difference &&
            s.kind != SeriesKind::smoothing && s.kind != SeriesKind::unindexed_peaks)
            return Status{invalid("unsupported_layer")};
    }
    return Status{std::monostate{}};
}
Status QtPlotSink::apply(const PlotUpdate &update) {
    const auto checked = validate(update);
    if (!checked) return checked;
    try {
        PlotDispatcher{}.dispatch(update, *this);
        return Status{std::monostate{}};
    } catch (const PlotSinkFailure &failure) { return Status{failure.error()}; }
}
void QtPlotSink::accept(const PlotCommand &command) {
    PlotUpdate one;
    one.commands.push_back(command);
    const auto checked = validate(one);
    if (!checked) throw PlotSinkFailure(checked.error());
    auto &view = *view_;
    switch (command.kind) {
    case CommandKind::rescale: view.enableRescalePlot(command.rescale); return;
    case CommandKind::replace_canvas: view.setGraphicArea(); replacing_ = true; return;
    case CommandKind::redraw: view.drawPlot(); replacing_ = false; return;
    default: break;
    }
    const auto &s = *command.series;
    const bool erase = command.kind == CommandKind::erase;
    if (s.kind == SeriesKind::accepted_card) {
        if (erase) view.removePhaseReflections(QString::fromStdString(s.identity));
        else view.applyProjectedCard(s, card_color_);
        return;
    }
    if (s.kind == SeriesKind::preprocessing_preview) {
        if (preview_) { view.removeTrackedGraph(preview_); view.replot(); preview_.clear(); }
        if (!erase) {
            preview_ = view.addGraph();
            view.plotWave.append(s.wavelength);
            preview_->setName(QString::fromStdString(s.label));
            preview_->setPen(QPen(QColor(230, 120, 30)));
            if (s.preview_geometry == PreviewGeometry::peak_impulses) preview_->setLineStyle(QCPGraph::lsImpulse);
            preview_->setData(values(s.x), values(s.y));
            view.replot();
        }
        return;
    }
    const auto kind = static_cast<graphItem::ItemType>(s.kind);
    if (replacing_ && !erase) {
        view.addProjectedPlot(values(s.x), values(s.y), kind, int(s.visibility), s.wavelength,
                              QString::fromStdString(s.label));
        return;
    }
    graphItem *item = nullptr;
    switch (s.kind) {
    // Preserve the existing guard: a Session refresh never creates the primary
    // observed graph before the first compatibility replacement.
    case SeriesKind::observed: if (!view.obs.empty()) item = &view.obs.front(); break;
    case SeriesKind::peaks: item = &view.peaks; break;
    case SeriesKind::background: item = &view.back; break;
    case SeriesKind::background_points: item = &view.bpoints; break;
    case SeriesKind::calculated: item = &view.calc; break;
    case SeriesKind::difference: item = &view.diff; break;
    case SeriesKind::smoothing: item = &view.smooth; break;
    case SeriesKind::unindexed_peaks: item = &view.undpeaks; break;
    default: throw PlotSinkFailure(invalid("unsupported_layer"));
    }
    if (item) view.applyProjectedSessionItem(*item, erase ? QVector<double>{} : values(s.x),
                                             erase ? QVector<double>{} : values(s.y), s.wavelength);
}
} // namespace qualpf::gui
