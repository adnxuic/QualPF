#pragma once
#include <QDoubleSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QWheelEvent>
#include <functional>

// R1: a numerical request is owned independently of formatted widget text.
// Only textEdited/committed user text or input-scoped stepping creates a bound.
class RangeBoundSpinBox final : public QDoubleSpinBox {
public:
    explicit RangeBoundSpinBox(QWidget *parent=nullptr) : QDoubleSpinBox(parent) {
        setKeyboardTracking(false);
        lineEdit()->installEventFilter(this);
        connect(lineEdit(),&QLineEdit::textEdited,this,[this] {
            if (!synchronizing_) { modified_=true; pendingText_=true; publishProperties(); }
        });
        connect(this,&QDoubleSpinBox::editingFinished,this,[this] { commitPendingText(); });
    }
    std::function<void(double)> userRequest;
    double requestedValue() const { return requested_; }
    bool userModified() const { return modified_; }
    void initialize(double value) {
        requested_=value;modified_=false;pendingText_=false;synchronize(value);publishProperties();
    }
    void synchronize(double value) {
        const QSignalBlocker spin(this), text(lineEdit());
        synchronizing_=true;setValue(value);synchronizing_=false;
    }
    void checkedGraphRequest(double value, bool modified) {
        requested_=value;modified_=modified_ || modified;pendingText_=false;synchronize(value);publishProperties();
    }
    void commitPendingText() {
        if (synchronizing_ || !pendingText_) return;
        interpretText();requested_=value();pendingText_=false;publishProperties();
        if (userRequest) userRequest(requested_);
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override {
        if (event->type()==QEvent::KeyPress) keyStep_=isStep(static_cast<QKeyEvent*>(event)->key());
        return QDoubleSpinBox::eventFilter(object,event);
    }
    void keyPressEvent(QKeyEvent *event) override {
        keyStep_=isStep(event->key());QDoubleSpinBox::keyPressEvent(event);keyStep_=false;
    }
    void mousePressEvent(QMouseEvent *event) override { mouseStep_=true;QDoubleSpinBox::mousePressEvent(event); }
    void mouseReleaseEvent(QMouseEvent *event) override { QDoubleSpinBox::mouseReleaseEvent(event);mouseStep_=false; }
    void wheelEvent(QWheelEvent *event) override { wheelStep_=true;QDoubleSpinBox::wheelEvent(event);wheelStep_=false; }
    void stepBy(int steps) override {
        const bool user=!synchronizing_ && (keyStep_ || mouseStep_ || wheelStep_);
        QDoubleSpinBox::stepBy(steps);
        if (user) {
            modified_=true;pendingText_=false;requested_=value();publishProperties();
            if (userRequest) userRequest(requested_);
        }
        keyStep_=false;
    }
private:
    static bool isStep(int key) { return key==Qt::Key_Up || key==Qt::Key_Down || key==Qt::Key_PageUp || key==Qt::Key_PageDown; }
    void publishProperties() {
        setProperty("rangeRequested",requested_);setProperty("rangeUserModified",modified_);setProperty("rangePendingText",pendingText_);
    }
    double requested_=0;
    bool modified_=false,pendingText_=false,synchronizing_=false,keyStep_=false,mouseStep_=false,wheelStep_=false;
};
