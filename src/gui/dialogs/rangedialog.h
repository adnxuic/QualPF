#ifndef RANGEDIALOG_H
#define RANGEDIALOG_H
#include <QDialog>
#include <QAbstractButton>
#include <QPointer>
#include "gui/dialogs/preprocessing_dialog.h"
#include "qualpf/application/presentation/plot_snapshot.h"
class CustomPlotZoom;
namespace Ui { class RangeDialog; }
class RangeDialog : public QDialog {
    Q_OBJECT
public:
    explicit RangeDialog(QWidget *parent=nullptr);
    ~RangeDialog() override;
    void setRange(CustomPlotZoom *plot);
    bool isClosed() const { return closed_; }
private slots:
    void on_buttonBox_clicked(QAbstractButton *button);
    void setMinimum(double value);
    void setMaximum(double value);
    void lcursorPositionChanged(double pos);
    void rcursorPositionChanged(double pos);
    void accept() override;
    void reject() override;
protected:
    void closeEvent(QCloseEvent *) override;
    void showEvent(QShowEvent *) override;
private:
    void finish(bool accepted);
    bool synchronizeCursors();
    bool flushCursorPair();
    void queueCursorFlush();
    void coordinateError(const qualpf::Error &);
    bool checkDisplayContext();
    Ui::RangeDialog *ui;
    const bool legacyOwner_;
    bool initialized_=false,closed_=false,rangeChanged_=false,synchronizing_=false;
    bool queued_=false,leftEdited_=false,rightEdited_=false,coordinateError_=false;
    double minValue=0,maxValue=0,minValueSav=0,maxValueSav=0;
    double displayLeft_=0,displayRight_=0,leftCanonical_=0,rightCanonical_=0;
    qualpf::application::presentation::Abscissa axis_;
    std::optional<qualpf::WavelengthAngstrom> wave_;
    QPointer<CustomPlotZoom> mPlot;
    std::unique_ptr<qualpf::gui::PreprocessingDialog> cppEdit;
};
#endif
