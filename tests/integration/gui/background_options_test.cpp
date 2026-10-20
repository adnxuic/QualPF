#include "gui/mainwindow.h"
#include "gui/state/preprocessing_product.h"
#include "qualpf/application/export/pattern_export_input.h"
#include "tests/support/workspace_fixture.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QDir>
#include <QDockWidget>
#include <QFontDatabase>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <cmath>
#include <iostream>
#include <source_location>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application;
static void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) throw std::runtime_error("Background GUI at line " + std::to_string(at.line()));
}
template<class T> static T *widget(QObject &parent, const char *name) {
    auto *value = parent.findChild<T *>(QString::fromLatin1(name)); check(value != nullptr); return value;
}
struct Background final : preprocessing::IBackgroundEstimator {
    mutable int calls = 0;
    bool fail = false;
    AlgorithmIdentity identity() const override { return {"qualpf.background.cpp", 1}; }
    Result<preprocessing::BackgroundResult> estimate(const Pattern &p, const preprocessing::BackgroundOptions &,
        WavelengthAngstrom, std::optional<std::span<const preprocessing::BackgroundPoint>> points,
        const ExecutionContext &) const override {
        ++calls; check(!points && p.processing.background_subtracted == false);
        if (fail) return Result<preprocessing::BackgroundResult>{Error{ErrorCode::numerical_failure,"test","Test failure"}};
        preprocessing::BackgroundResult r; r.values.assign(p.observed.size(), 7);
        r.points = {{float(p.two_theta.front().value),7},{float(p.two_theta.back().value),7}};
        return Result<preprocessing::BackgroundResult>{std::move(r)};
    }
};
int main(int argc, char **argv) try {
    QApplication app(argc, argv);
    const QString capture = argc == 3 && std::string(argv[1]) == "--capture" ? QString::fromLocal8Bit(argv[2]) : QString();
#ifdef Q_OS_WIN
    if (!capture.isEmpty()) {
        const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        if (font >= 0) app.setFont(QFont(QFontDatabase::applicationFontFamilies(font).front(), 9));
    }
#endif
    QTemporaryDir temporary; check(temporary.isValid());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    app.setOrganizationName("QualPF-tests"); app.setApplicationName("background-options");
    auto background = std::make_shared<Background>(); AlgorithmModules modules; modules.background = background;
    auto algorithms = resolveAlgorithms(AlgorithmSpec{}, modules); check(bool(algorithms));
    ProductState state; state.algorithms = algorithms.value(); auto &p = state.pattern;
    p.id = {81}; p.source_id = "analytic-background"; p.processing = {false,false,false};
    p.radiation.lines = {{WavelengthAngstrom{1.54},1}};
    for (int i=0; i<=100; ++i) { p.two_theta.emplace_back(20+.02*i); p.observed.push_back(i == 0 ? 1 : 10); }
    p.original_observed = p.observed; p.background = SampledSignal{std::vector<double>(101,5),SampleWindow{0}};
    p.calculation_range = AngularRange{p.two_theta.front(),p.two_theta.back()};
    state.preprocessing.background_points = std::vector<preprocessing::BackgroundPoint>{{20,5},{22,5}};
    state.preprocessing.background_filters.push_back({20,22,1,4});
    auto &session = AppState::workspace().session(); check(bool(session.commit(session.snapshot(),state)));
    MainWindow window(AppState::context()); window.show(); window.enableActions(MainWindow::PatternAction);
    check(window.refreshPatternPresentation()); window.refreshPreprocessingDisplay();
    auto *generate = widget<QAction>(window,"actionBackground");
    auto *options = widget<QAction>(window,"actionBackgroundOptions");
    auto *subtract = widget<QAction>(window,"actionSubtract_Background");
    const auto preprocessing = widget<QMenu>(window,"menuPattern")->actions();
    check(preprocessing.indexOf(options) == preprocessing.indexOf(generate)+1 && !preprocessing.contains(subtract));
    check(widget<QMenu>(window,"menuView")->actions().contains(subtract));
    check(subtract->isCheckable() && !subtract->isChecked() && subtract->isEnabled());
    auto *plot = window.xpdViewer();
    if (!capture.isEmpty()) {
        check(QDir().mkpath(capture)); window.resize(1000,650);
        for (auto *dock : window.findChildren<QDockWidget *>()) dock->hide();
        QApplication::processEvents();
    }
    const auto snap = [&](const char *name) { if (!capture.isEmpty()) check(window.grab().save(capture + "/" + name)); };
    snap("background-default.png");
    check(plot->bpoints.graphIndex >= 0 && !plot->back.visible);
    const auto source = session.snapshot();
    const auto exported = application::exporting::prepare_pattern_export(source->pattern, {}, qualpf::exporting::PatternFormat::xy_observed);
    check(bool(exported));
    subtract->trigger(); check(subtract->isChecked() && session.snapshot() == source);
    check(plot->graph(plot->obs.front().graphIndex)->data()->begin()->value == -4);
    check(plot->yAxis->range().lower <= -4);
    check(application::exporting::prepare_pattern_export(session.snapshot()->pattern, {}, qualpf::exporting::PatternFormat::xy_observed).value().observed == exported.value().observed);
    snap("background-net.png");
    check(plot->bpoints.graphIndex < 0 || plot->graph(plot->bpoints.graphIndex)->data()->isEmpty());
    subtract->trigger(); check(!subtract->isChecked() && session.snapshot() == source);
    check(plot->graph(plot->obs.front().graphIndex)->data()->begin()->value == 1);
    check(plot->bpoints.graphIndex >= 0);
    generate->trigger(); const auto generated = session.snapshot();
    check(background->calls == 1 && generated != source && !subtract->isChecked());
    check(generated->pattern.observed == source->pattern.observed && generated->pattern.processing.background_subtracted == false);
    check(generated->pattern.background->values.front() == 7 && generated->preprocessing.background_filters.empty());
    check(generated->preprocessing.background_points->front().y == 7 && generated->provenance.subtractedBackgroundHistory.empty());
    for (int i=0;i<8;++i) subtract->trigger();
    check(session.snapshot() == generated && !subtract->isChecked());
    subtract->trigger(); background->fail = true; generate->trigger();
    check(session.snapshot() == generated && subtract->isChecked()); background->fail = false;
    options->trigger(); auto *dialog = window.findChild<BackgroundDialog *>(); check(dialog && dialog->isVisible());
    check(!subtract->isChecked());
    QApplication::processEvents();
    if (!capture.isEmpty()) check(dialog->grab().save(capture + "/background-options.png"));
    widget<QComboBox>(*dialog,"typeComboBox")->setCurrentIndex(1);
    widget<QDialogButtonBox>(*dialog,"buttonBox")->button(QDialogButtonBox::Cancel)->click();
    check(session.snapshot()->pattern.observed == generated->pattern.observed &&
        session.snapshot()->pattern.background->values == generated->pattern.background->values);
    options->trigger(); widget<QComboBox>(*dialog,"typeComboBox")->setCurrentIndex(1);
    auto *ok = widget<QDialogButtonBox>(*dialog,"buttonBox")->button(QDialogButtonBox::Ok);
    QApplication::processEvents(); ok->setFocus(); ok->click();
    check(!dialog->isVisible() && session.snapshot()->preprocessing.background->method == preprocessing::BackgroundMethod::polynomial);
    subtract->trigger(); widget<QAction>(window,"actionAdd_Background_Point")->trigger();
    check(!subtract->isChecked());
    auto partial = *session.snapshot(); partial.pattern.background = SampledSignal{{7,7},SampleWindow{2}};
    check(bool(session.commit(session.snapshot(),std::move(partial)))); check(!subtract->isEnabled());
    QTimer::singleShot(0, [&] {
        for (auto *w : QApplication::topLevelWidgets())
            if (auto *box = qobject_cast<QMessageBox *>(w); box && box->isVisible()) box->button(QMessageBox::Yes)->click();
    });
    window.close(); check(window.isClosing());
    std::cout << "PASS: background actions, draft options, signed display and atomic generation\n";
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
