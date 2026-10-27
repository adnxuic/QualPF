#include "platform_services.h"
#include "infrastructure/resources/fileutils.h"
#include <QProcess>
#include <QCoreApplication>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QDateTime>
#include <QSysInfo>
#if USE_CONFIG_H
#include "config.h"
#endif
namespace qualpf::infrastructure {
namespace {
class QtProcessSession final : public application::ProcessSession {
    QProcess process_;
    application::ProcessCallbacks callbacks_;
public:
    QtProcessSession() {
        QObject::connect(&process_,&QProcess::started,&process_,[this] { if(callbacks_.started) callbacks_.started(); });
        QObject::connect(&process_,&QProcess::finished,&process_,[this](int code,QProcess::ExitStatus status) {
            qDebug()<<"exitCode="<<code<<", exitStatus="<<status;
            application::ProcessResult result{code,int(status),QString::fromLocal8Bit(process_.readAllStandardOutput()).toStdString(),QString::fromLocal8Bit(process_.readAllStandardError()).toStdString()};
            if(callbacks_.finished) callbacks_.finished(std::move(result));
        });
        QObject::connect(&process_,&QProcess::errorOccurred,&process_,[this](QProcess::ProcessError error) {
            qDebug()<<error; if(callbacks_.error) callbacks_.error(int(error));
        });
    }
    ~QtProcessSession() override { callbacks_={}; QObject::disconnect(&process_,nullptr,nullptr,nullptr); }
    bool running() const override { return process_.state()!=QProcess::NotRunning; }
    void start(application::ProcessRequest request,application::ProcessCallbacks callbacks) override {
        callbacks_=std::move(callbacks); QStringList args;
        for(const auto &arg:request.arguments) args.append(QString::fromStdString(arg));
        process_.start(QString::fromStdString(request.executable),args);
    }
    void detach(application::ProcessRequest request) override {
        QStringList args; for(const auto &arg:request.arguments) args.append(QString::fromStdString(arg));
        QProcess::startDetached(QString::fromStdString(request.executable),args);
    }
};
}
std::string QtPlatformServices::environmentValue(const std::string &name) const { return qEnvironmentVariable(name.c_str()).toStdString(); }
std::unique_ptr<application::ProcessSession> QtPlatformServices::makeProcess() { return std::make_unique<QtProcessSession>(); }
std::string QtPlatformServices::maintenanceExecutable() const {
    QDir parent(QCoreApplication::applicationDirPath()); parent.cdUp();
    qDebug()<<"ParentDir: "<<parent;
#ifdef Q_OS_WIN
    const auto path=parent.absoluteFilePath("maintenancetool.exe");
#else
    const auto path=parent.absoluteFilePath("maintenancetool");
#endif
    qDebug()<<"PATH: "<<path; return path.toStdString();
}
application::ProgramLocation QtPlatformServices::locateProgram(application::ProgramRequest request) {
    QSettings settings; const auto key=QString::fromStdString(request.settingsKey),name=QString::fromStdString(request.name);
    if(settings.contains(key)) {
        const auto candidate=settings.value(key).toString();
        if(QFile::exists(candidate)) return {true,candidate.toStdString()};
        settings.remove(key);
    }
    const auto files=fileutils::findFilesInPATH({name});
    if(!files.isEmpty()) { settings.setValue(key,files.first()); return {true,files.first().toStdString()}; }
    if(!request.folder.empty()) {
        const auto path=QDir(QString::fromStdString(request.folder)).filePath(name);
        if(QFile::exists(path)) { settings.setValue(key,path); return {true,path.toStdString()}; }
    }
    return {};
}
bool QtPlatformServices::fileExists(const std::string &path) const { return QFile::exists(QString::fromStdString(path)); }
application::FileInformation QtPlatformServices::fileInformation(const std::string &path) const {
    const QFileInfo file(QString::fromStdString(path));
    application::FileInformation result;
    result.exists = file.exists();
    if (result.exists) {
        const auto modified = file.lastModified();
        if (modified.isValid()) result.lastModifiedMilliseconds = modified.toMSecsSinceEpoch();
    }
    return result;
}
bool QtPlatformServices::makeDirectory(const std::string &path) { return QDir().mkpath(QString::fromStdString(path)); }
std::string QtPlatformServices::newDatabaseDirectory() const {
    const auto base = QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QDir::separator() + "QualXDB";
    int index = 1;
    while (true) {
        const auto candidate = base + QDir::separator() + QStringLiteral("DB%1").arg(index, 2, 10, QChar('0'));
        if (!QDir(candidate).exists()) return candidate.toStdString();
        ++index;
    }
}
application::BuildPlatformInfo QtPlatformServices::buildInformation() const {
    application::BuildPlatformInfo info;
    info.creationDate = QFileInfo(QCoreApplication::applicationFilePath()).lastModified().toString(Qt::TextDate).toStdString();
    info.architecture = QSysInfo::buildCpuArchitecture().toStdString();
#if USE_CONFIG_H
    info.buildType = BUILD_TYPE.toStdString();
    info.compiler = QFileInfo(CPP_COMPILER).baseName().toStdString();
#endif
    info.qtVersion = QT_VERSION_STR;
    return info;
}
std::string QtPlatformServices::documentationPath() {
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates={appDir.filePath("../docs/qualpf_manual.pdf"),appDir.filePath("../../docs/qualpf_manual.pdf"),appDir.filePath("../share/qualpf/docs/qualpf_manual.pdf"),appDir.filePath("docs/qualpf_manual.pdf")};
    for(const auto &path:candidates) if(QFile::exists(path)) {
        auto openPath=QFileInfo(path).absoluteFilePath();
        if(qEnvironmentVariableIsSet("APPIMAGE")) {
            if (!documentationDirectory_)
                documentationDirectory_ = std::make_unique<QTemporaryDir>(
                    QDir::tempPath() + "/qualpf-help-XXXXXX");
            if (!documentationDirectory_->isValid()) return {};
            const auto copy = documentationDirectory_->filePath("qualpf_manual.pdf");
            if (!QFile::exists(copy) && !QFile::copy(openPath, copy)) return {};
            openPath = copy;
        }
        return openPath.toStdString();
    }
    return {};
}
}
