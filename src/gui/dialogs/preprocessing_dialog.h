#pragma once
#include "gui/state/preprocessing_edit.h"
#include <QPointer>
#include <memory>
class MainWindow;
class QDialog;
class QCPGraph;
namespace qualpf::gui {
class QtPlotSink;
// Owns only the private preview and a disposable plot overlay.
class PreprocessingDialog {
  public:
    // Default C++; only the explicit Legacy selector enters compatibility.
    static bool enabled();
    PreprocessingDialog(QDialog &, MainWindow *);
    ~PreprocessingDialog();
    application::PreprocessingRequest request;
    bool show_background = false;
    bool preview();
    const gui::SessionStateView *state() const { return edit_->preview(); }
    std::size_t candidateCount() const { return edit_->candidateCount(); }
    void rejectPreview(const QString &reason);
    bool commit();
    void cancel();

  private:
    void clearPlot();
    QDialog &dialog_;
    QPointer<MainWindow> window_;
    std::unique_ptr<QtPlotSink> sink_;
    std::unique_ptr<gui::PreprocessingEdit> edit_;
};
} // namespace qualpf::gui
