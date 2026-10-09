#include "qualpf/application/startup/application_initialization_service.h"
#include "tests/support/workspace_fixture.h"
#include "gui/mainwindow.h"
#include "infrastructure/settings/progkeysettings.h"
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>
#include <stdexcept>

class ProductWorkflowProbe {
  public:
    static QString status(const MainWindow &window) { return window.statusLabel1->text(); }
    static bool saved(const MainWindow &window) { return window.projectFileSaved; }
};
namespace {
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
QByteArray bytes(const QString &path) {
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "Read target");
    return file.readAll();
}
void seed(const QString &path, const QByteArray &data) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "Seed target");
}
void action(MainWindow &window, const QString &path, const QString &format, bool cancel, bool background = false) {
    const auto source = AppState::searchSession().snapshot();
    const auto saved = ProductWorkflowProbe::saved(window);
    const auto remembered = QSettings{}.value(DEFAULT_DIR_KEY);
    bool visited = false, pathMatched = false;
    QTimer driver;
    QObject::connect(&driver, &QTimer::timeout, [&] {
        if (auto *dialog = qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            visited = true;
            driver.stop();
            if (cancel) {
                dialog->reject();
            } else {
                dialog->setOption(QFileDialog::DontConfirmOverwrite);
                dialog->setDirectory(QFileInfo(path).absolutePath());
                QTimer::singleShot(200, dialog, [dialog, format, path, &pathMatched] {
                    dialog->selectNameFilter(format == "cif" ? "CIF files (*.cif)" : "XY files (*.xy)");
                    dialog->selectFile(QFileInfo(path).fileName());
                    if (auto *edit = dialog->findChild<QLineEdit *>("fileNameEdit"))
                        edit->setText(QFileInfo(path).fileName());
                    QTimer::singleShot(50, dialog, [dialog, path, &pathMatched] {
                        pathMatched = !dialog->selectedFiles().isEmpty() &&
                            QFileInfo(dialog->selectedFiles().first()).absoluteFilePath() == QFileInfo(path).absoluteFilePath();
                        if (pathMatched) QMetaObject::invokeMethod(dialog, "accept", Qt::DirectConnection);
                        else dialog->reject();
                    });
                });
            }
        }
    });
    driver.start(10);
    check(QMetaObject::invokeMethod(&window, background ? "onActionBackgroundExportTriggered"
                                                       : "onActionExportDiffractionPattern", Qt::DirectConnection),
          "Real MainWindow export slot");
    check(visited, "Real SaveDialog");
    check(cancel || pathMatched, "Settled SaveDialog requested path");
    check(AppState::searchSession().snapshot() == source && ProductWorkflowProbe::saved(window) == saved,
          "Export/cancel leaves session/project untouched");
    if (cancel)
        check(QSettings{}.value(DEFAULT_DIR_KEY) == remembered, "Cancel leaves remembered directory untouched");
}
} // namespace
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        check(argc == 3, "Output directory, repository required");
        const auto out = QString::fromLocal8Bit(argv[1]), root = QString::fromLocal8Bit(argv[2]);
        check(QDir().mkpath(out), "Output directory");
        QTemporaryDir settings(out + "/settings-XXXXXX");
        QCoreApplication::setOrganizationName("Phase24B");
        QCoreApplication::setApplicationName("GuiExportControls");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settings.path());
        qualpf::application::ApplicationInitializationService{}.initialize(
            AppState::presentation(), qualpf::application::PresentationMode::automatic_gui);
        MainWindow::setPathDataFiles(root + "/share/qualx/files");
        MainWindow window(AppState::context());
        seed(out + "/input.xy", "10 1\n20 2\n");
        check(window.loadDiffractionPatterns({out + "/input.xy"}, false), "Canonical two-point input");
        auto &session = AppState::searchSession();
        action(window, out + "/cancel.xy", "xy", true);
        action(window, out + "/cancel.cif", "cif", true);
        action(window, out + "/cancel-background.xy", "xy", true, true);
        check(!QFile::exists(out + "/cancel.xy") && !QFile::exists(out + "/cancel.cif") &&
                  !QFile::exists(out + "/cancel-background.xy"), "No file from cancelled dialogs");
        using namespace qualpf;
        const auto baseline = *session.snapshot();
        for (int boundary = 0; boundary < 3; ++boundary) {
            auto next = baseline;
            if (boundary < 2) {
                next.pattern.two_theta.front() = TwoThetaDegrees{boundary == 0 ? 0.0 : -1.0};
                next.pattern.corrected_two_theta = next.pattern.two_theta;
                next.pattern.calculation_range = AngularRange{next.pattern.two_theta.front(), next.pattern.two_theta.back()};
            } else {
                next.pattern.radiation.lines.front().wavelength = WavelengthAngstrom{1e-300};
            }
            check(bool(session.commit(session.snapshot(), next)), "Canonical B2 boundary commit");
            const auto path = out + "/rejected-" + QString::number(boundary) + ".cif";
            seed(path, "original");
            action(window, path, "cif", false);
            const auto reason = boundary == 0 ? "nonfinite_d_spacing" : boundary == 1 ? "nonpositive_d_spacing" : "invalid_wavelength";
            check(bytes(path) == "original" && ProductWorkflowProbe::status(window).contains(reason),
                  "GUI approved rejection reports exact reason and preserves target");
        }
        check(bool(session.commit(session.snapshot(), baseline)), "Restore canonical source");
        action(window, out + "/committed.xy", "xy", false);
        const auto committed = bytes(out + "/committed.xy");
        session.beginLegacyEdit("phase24b-private-preview");
        auto preview = *session.snapshot();
        preview.pattern.observed = {1000, 2000};
        check(bool(session.stageLegacyInput(session.snapshot(), preview)), "Private numerical preview staged");
        action(window, out + "/private-edit.xy", "xy", false);
        check(bytes(out + "/private-edit.xy") == committed, "XY exports committed state during private preview");
        seed(out + "/private-edit.cif", "original");
        action(window, out + "/private-edit.cif", "cif", false);
        check(bytes(out + "/private-edit.cif") == "original" &&
                  ProductWorkflowProbe::status(window).contains("private_editor_active"), "GUI CIF private edit guard");
        session.endLegacyEdit("phase24b-private-preview", false);
        seed(out + "/summary.json", QJsonDocument(QJsonObject{{"status", "PASS"}, {"cancel_dialogs", 3},
             {"B2_rejections", 3}, {"private_preview_observed_export", "canonical committed"},
             {"private_preview_CIF", "private_editor_active"}, {"session_project_unchanged", true}}).toJson());
        std::cout << "PASS real SaveDialog cancellation, GUI B2 errors/target preservation, committed/private preview semantics\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
