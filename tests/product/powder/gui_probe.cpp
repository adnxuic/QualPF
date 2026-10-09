#include "infrastructure/serialization/metadata_values.h"
#include "qualpf/application/startup/application_initialization_service.h"
#include "tests/support/workspace_fixture.h"
#include "gui/dialogs/wavedialog.h"
#include "gui/mainwindow.h"
#include "infrastructure/settings/progkeysettings.h"
#include "observation.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <iostream>

#if defined(_MSC_VER)
#include <atomic>
extern "C" __declspec(noinline) void phase25_marker(const char *name) {
    (void)name;
    std::atomic_signal_fence(std::memory_order_seq_cst);
}
#else
extern "C" __attribute__((noinline)) void phase25_marker(const char *name) {
    asm volatile("" : : "r"(name) : "memory");
}
#endif
class ProductWorkflowProbe {
  public:
    static bool saved(const MainWindow &w) { return w.projectFileSaved; }
    static QString status(const MainWindow &w) { return w.statusLabel1->text(); }
    static void save(MainWindow &w,const QString &path) { w.saveProject(path); }
    static void load(MainWindow &w,const QString &path) { w.loadProject(path); }
};
namespace {
QJsonArray recentEntries() {
    QSettings settings; QJsonArray values;
    const int count=settings.beginReadArray(QUALX_RECENT_FILES_KEY);
    for (int i=0;i<count;++i) {
        settings.setArrayIndex(i);
        values.append(QJsonObject{{"file",settings.value("FileName").toString()},
                                  {"type",settings.value("FileType").toString()}});
    }
    settings.endArray(); return values;
}
QAction *powderAction(MainWindow &w) {
    for (auto *a : w.findChildren<QAction *>())
        if (a->text().startsWith("Import Powder CIF (Legacy)")) return a;
    throw std::runtime_error("Real powder QAction unavailable");
}
QJsonObject import(MainWindow &w,const QString &input,bool recent,bool cancel,const QString &waveMode) {
    const auto before=AppState::searchSession().snapshot();
    const auto oldRecent=recentEntries(); const auto oldDir=QSettings{}.value(DEFAULT_DIR_KEY);
    const auto saved=ProductWorkflowProbe::saved(w);
    bool visited=false,settled=false,wave=false,unexpected=false;
    QTimer driver;
    QObject::connect(&driver,&QTimer::timeout,[&] {
        if (auto *dialog=qobject_cast<QFileDialog *>(QApplication::activeModalWidget())) {
            if (visited) return;
            visited=true;
            if (cancel) { dialog->reject(); return; }
            dialog->setDirectory(QFileInfo(input).absolutePath());
            QTimer::singleShot(200,dialog,[dialog,input,&settled] {
                dialog->selectFile(QFileInfo(input).fileName());
                if (auto *edit=dialog->findChild<QLineEdit *>("fileNameEdit")) edit->setText(QFileInfo(input).fileName());
                QTimer::singleShot(50,dialog,[dialog,input,&settled] {
                    settled=!dialog->selectedFiles().isEmpty() &&
                        QFileInfo(dialog->selectedFiles().first()).absoluteFilePath()==QFileInfo(input).absoluteFilePath();
                    if (settled) QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);
                    else dialog->reject();
                });
            });
        } else if (auto *dialog=qobject_cast<WaveDialog *>(QApplication::activeModalWidget())) {
            wave=true;
            if (waveMode=="reject") dialog->reject(); else dialog->accept();
        } else if (auto *dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget())) {
            unexpected=true; dialog->reject();
        }
    });
    driver.start(10);
    phase25_marker(recent ? "recent-root" : "powder-action-root");
    if (recent) {
        QAction *found=nullptr;
        for (auto *a : w.findChildren<QAction *>())
            if (!a->data().toString().isEmpty() &&
                QFileInfo(a->data().toString()).absoluteFilePath()==QFileInfo(input).absoluteFilePath()) found=a;
        phase25b::check(found,"Real typed recent action"); found->trigger();
    } else powderAction(w)->trigger();
    phase25_marker("product-end"); driver.stop();
    phase25b::check(!unexpected,"Unexpected product dialog");
    phase25b::check(recent || visited,"Actual file dialog");
    phase25b::check(recent || cancel || settled,"Actual file selection settled");
    const auto after=AppState::searchSession().snapshot();
    QJsonObject out{{"file_dialog",visited},{"file_dialog_settled",settled},{"wave_dialog",wave},
        {"wave_mode",waveMode},{"changed",after!=before},{"revision_delta",double(after->revision-before->revision)},
        {"saved_before",saved},{"saved_after",ProductWorkflowProbe::saved(w)},
        {"recent_before",oldRecent},{"recent_after",recentEntries()},
        {"recent_unchanged",oldRecent==recentEntries()},{"directory_unchanged",oldDir==QSettings{}.value(DEFAULT_DIR_KEY)},
        {"status",ProductWorkflowProbe::status(w)},{"metadata",qualpf::infrastructure::jsonValue(after->projectMetadata)},
        {"peaks",after->peaks.tth.size()},{"candidates",after->candidates.size()},{"phases",after->phases.size()},
        {"smoothing_applied",after->preprocessing.smoothing_applied.value_or(true)}};
    if (after!=before) out["pattern"]=phase25b::pattern(after->pattern);
    if (cancel || (wave && waveMode=="reject"))
        phase25b::check(after==before && ProductWorkflowProbe::saved(w)==saved &&
            oldRecent==recentEntries() && oldDir==QSettings{}.value(DEFAULT_DIR_KEY),"GUI Cancel full atomicity");
    return out;
}
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    try {
        phase25b::check(argc>=5,"Mode input output repository");
        const QString mode=QString::fromLocal8Bit(argv[1]),input=QString::fromLocal8Bit(argv[2]);
        const QString output=QString::fromLocal8Bit(argv[3]),root=QString::fromLocal8Bit(argv[4]);
        QTemporaryDir settings(QFileInfo(output).absolutePath()+"/settings-XXXXXX");
        phase25b::check(settings.isValid(),"Private GUI settings");
        QCoreApplication::setOrganizationName("Phase25B"); QCoreApplication::setApplicationName("PowderCutover");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
        qualpf::application::ApplicationInitializationService{}.initialize(
            AppState::presentation(),qualpf::application::PresentationMode::automatic_gui);
        MainWindow::setPathDataFiles(root+"/share/qualx/files");
        MainWindow window(AppState::context());
        phase25b::seed(AppState::searchSession());
        const QString initialProject=QFileInfo(output).absolutePath()+"/seed.qxp";
        ProductWorkflowProbe::save(window,initialProject);
        phase25b::check(ProductWorkflowProbe::saved(window),"Saved prior project");
        const QString waveMode=argc>5 ? QString::fromLocal8Bit(argv[5]) : "accept";
        auto result=import(window,input,false,mode=="cancel",waveMode);
        if (result["changed"].toBool()) {
            if (mode=="recent") result["recent"]=import(window,input,true,false,waveMode);
            else if (mode=="recent-cancel") {
                ProductWorkflowProbe::save(window,QFileInfo(output).absolutePath()+"/before-cancel.qxp");
                result["recent"]=import(window,input,true,false,"reject");
            } else if (mode=="recent-failure") {
                phase25b::check(QFileInfo(input).absolutePath()==QFileInfo(output).absolutePath() &&
                    QFileInfo(input).fileName().startsWith("detached-"),"Only corrupt detached test source");
                ProductWorkflowProbe::save(window,QFileInfo(output).absolutePath()+"/before-failure.qxp");
                QFile damaged(input);
                phase25b::check(damaged.open(QIODevice::WriteOnly|QIODevice::Truncate),"Private recent error source");
                damaged.write("data_bad\nloop_\n_pd_meas_2theta_scan\n_pd_meas_counts_total\n10\n");
                damaged.close();
                result["recent"]=import(window,input,true,false,waveMode);
            } else if (mode=="lifecycle") {
                phase25b::check(QFileInfo(input).absolutePath()==QFileInfo(output).absolutePath() &&
                    QFileInfo(input).fileName().startsWith("detached-"),"Only remove detached test source");
                const auto savedState=AppState::searchSession().snapshot();
                const auto savedPattern=phase25b::pattern(savedState->pattern);
                const auto savedMetadata=savedState->projectMetadata["powder_cif_provenance"];
                const auto project=QFileInfo(output).absolutePath()+"/roundtrip.qxp";
                phase25_marker("project-save-root"); ProductWorkflowProbe::save(window,project);
                phase25b::check(ProductWorkflowProbe::saved(window),"Save powder project");
                phase25b::check(QFile::remove(input),"Remove detached source");
                AppState::searchSession().reset();
                phase25_marker("project-reopen-root"); ProductWorkflowProbe::load(window,project);
                phase25_marker("product-end");
                const auto reopened=AppState::searchSession().snapshot();
                phase25b::check(phase25b::pattern(reopened->pattern)==savedPattern,"Exact project Pattern/source/wave");
                phase25b::check(reopened->projectMetadata["powder_cif_provenance"]==savedMetadata,"Exact project provenance");
                phase25b::check(bool(reopened->powderCifProvenance),"Owning typed provenance restored");
                result["project_roundtrip_exact"]=true; result["source_absent"]=!QFile::exists(input);
                result["reopened_pattern"]=phase25b::pattern(reopened->pattern);
            }
        }
        window.grab().save(QFileInfo(output).absolutePath()+"/gui.png");
        phase25b::save(output,result);
        return 0;
    } catch (const std::exception &error) { std::cerr<<error.what()<<'\n'; return 1; }
}
