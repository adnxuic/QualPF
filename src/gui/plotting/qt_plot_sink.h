#pragma once

#include "qualpf/application/presentation/plot_snapshot.h"
#include <QColor>
#include <QPointer>
#include <stdexcept>

class XpdViewWidget;
class QCPGraph;

namespace qualpf::gui {
class PlotSinkFailure : public std::runtime_error {
  public:
    explicit PlotSinkFailure(Error error) : std::runtime_error(error.message), error_(std::move(error)) {}
    const Error &error() const noexcept { return error_; }
  private:
    Error error_;
};

// GUI-thread executor only. All coordinate, residual, background and peak
// values have already been projected into an owning Application DTO.
class QtPlotSink final : public application::presentation::PlotSink {
  public:
    explicit QtPlotSink(XpdViewWidget *view);
    void accept(const application::presentation::PlotCommand &) override;
    [[nodiscard]] Status apply(const application::presentation::PlotUpdate &);
    [[nodiscard]] Status validate(const application::presentation::PlotUpdate &) const;
    void setCardColor(const QColor &color) { card_color_ = color; }
    static application::presentation::Abscissa abscissa(const XpdViewWidget &);
  private:
    QPointer<XpdViewWidget> view_;
    QPointer<QCPGraph> preview_;
    QColor card_color_;
    bool replacing_ = false;
};
} // namespace qualpf::gui
