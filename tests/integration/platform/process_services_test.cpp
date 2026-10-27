#include "qualpf/application/platform_services.h"
#include "infrastructure/platform/platform_services.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryDir>
#include <QSettings>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <iostream>
#include <stdexcept>
using namespace qualpf::application;
void check(bool ok,const char *reason) { if(!ok) throw std::runtime_error(reason); }
#ifdef Q_OS_LINUX
void checkDocumentationChild() {
    const QDir root(QDir::cleanPath(QCoreApplication::applicationDirPath() + "/.."));
    const auto source = root.absoluteFilePath("docs/qualpf_manual.pdf");
    const auto download = root.absoluteFilePath("home/Downloads/qualpf_manual.pdf");
    check(QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) ==
              root.absoluteFilePath("home/Downloads"), "Downloads must be isolated before the regression runs");
    QString firstCopy, secondCopy;
    {
        qualpf::infrastructure::QtPlatformServices first, second;
        qunsetenv("APPIMAGE");
        check(QFileInfo(QString::fromStdString(first.documentationPath())).canonicalFilePath() ==
                  QFileInfo(source).canonicalFilePath(), "Ordinary installs use their installed manual");
        qputenv("APPIMAGE", "/isolated/qualpf.AppImage");
        firstCopy = QString::fromStdString(first.documentationPath());
        secondCopy = QString::fromStdString(second.documentationPath());
        check(!firstCopy.isEmpty() && !secondCopy.isEmpty() && firstCopy != secondCopy,
              "AppImage providers own unique help directories");
        check(firstCopy.startsWith(QDir::tempPath() + "/qualpf-help-") && QFile::exists(firstCopy),
              "AppImage manual is outside the mount in an owned temporary directory");
        check(QString::fromStdString(first.documentationPath()) == firstCopy, "Repeated Help keeps the live copy");
        QFile original(download); check(original.open(QIODevice::ReadOnly) && original.readAll() == "user PDF",
                                        "Existing Downloads manual remains byte-identical");
        check(QFile::remove(source), "Simulate AppImage unmount");
        QFile copied(firstCopy); check(copied.open(QIODevice::ReadOnly) && copied.readAll() == "packaged PDF",
                                     "Temporary help survives removal of its package source");
    }
    check(!QFile::exists(firstCopy) && !QFile::exists(secondCopy), "Provider cleanup removes only its own copies");
}

void checkDocumentationFixture() {
    QTemporaryDir fixture; check(fixture.isValid(), "Help fixture directory");
    QDir root(fixture.path());
    for (const auto *folder : {"bin", "docs", "home/Downloads", "home/.config"})
        check(root.mkpath(folder), "Help fixture folders");
    const auto executable = root.filePath("bin/platform-test");
    check(QFile::copy(QCoreApplication::applicationFilePath(), executable), "Isolated Help test executable");
    for (const auto &entry : {qMakePair(QString("docs/qualpf_manual.pdf"), QByteArray("packaged PDF")),
                             qMakePair(QString("home/Downloads/qualpf_manual.pdf"), QByteArray("user PDF")),
                             qMakePair(QString("home/.config/user-dirs.dirs"), QByteArray("XDG_DOWNLOAD_DIR=\"$HOME/Downloads\"\n"))}) {
        QFile file(root.filePath(entry.first));
        check(file.open(QIODevice::WriteOnly) && file.write(entry.second) == entry.second.size(), "Help fixture bytes");
    }
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert("HOME", root.filePath("home"));
    environment.insert("XDG_CONFIG_HOME", root.filePath("home/.config"));
    QProcess child; child.setProcessEnvironment(environment);
    child.start(executable, {"--documentation-child"});
    check(child.waitForFinished(10000), "Isolated Help check finishes");
    if (child.exitCode() != 0) std::cerr << child.readAllStandardError().toStdString();
    check(child.exitStatus() == QProcess::NormalExit && child.exitCode() == 0, "Isolated Help behavior");
}
#endif
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--process-child")) {
        std::cout<<"<updates>\n <update name=\"QualPF\" version=\"2.3.4\"/>\n</updates>\n"; return 0;
    }
    try {
#ifdef Q_OS_LINUX
        if (app.arguments().contains("--documentation-child")) { checkDocumentationChild(); return 0; }
        checkDocumentationFixture();
#endif
        qualpf::infrastructure::QtPlatformServices platform;
        const auto request=maintenanceRequest(platform,true), detached=maintenanceRequest(platform,false);
        check(request.arguments==std::vector<std::string>{"check-updates"} && detached.arguments==std::vector<std::string>{"--start-updater"},"Exact maintenance modes");
        for(const auto &output:{std::string{},std::string("no updates available"),std::string("Warning: fixture")}) {
            const auto out=maintenanceOutcome({0,0,output,{}});
            check(out.notify && !out.available && !out.warning,"No-update notification behavior");
        }
        check(maintenanceOutcome({1,0,{},{}}).warning && !maintenanceOutcome({2,0,{},{}}).notify,"Maintenance failure behavior");
        auto session=platform.makeProcess();
        int started=0,finished=0,errors=0; ProcessResult result;
        QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
        QObject::connect(&timeout,&QTimer::timeout,&loop,&QEventLoop::quit);
        session->start({QCoreApplication::applicationFilePath().toStdString(),{"--process-child"}},
            {[&]{++started;},[&](ProcessResult value){++finished;result=std::move(value);loop.quit();},[&](int){++errors;loop.quit();}});
        timeout.start(10000);loop.exec();
        check(started==1 && finished==1 && errors==0 && !session->running(),"Single process completion on owner event loop");
        session.reset();
        const auto outcome=maintenanceOutcome(result);
        check(outcome.notify && outcome.available && outcome.latestVersion=="2.3.4" && outcome.details==result.standardOutput,"Owned process result and XML version interpretation");
        QTemporaryDir directory; check(directory.isValid(),"Program lookup fixture");
        QCoreApplication::setOrganizationName("QualPF.Platform.Test");QCoreApplication::setApplicationName("qualpf");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,directory.path());
        const auto first=directory.path()+"/first", second=directory.path()+"/second";
        check(platform.makeDirectory(first.toStdString()) && platform.makeDirectory(second.toStdString()),"Directory adapter");
        for(const auto &folder:{first,second}) { QFile file(folder+"/qualpf-fixture");check(file.open(QIODevice::WriteOnly),"Program fixture");file.write("fixture"); }
        const auto oldPath=qgetenv("PATH"); qputenv("PATH",first.toUtf8());
        struct RestorePath { QByteArray old;~RestorePath(){qputenv("PATH",old);} } restore{oldPath};
        QSettings settings;settings.setValue("external/tool",second+"/qualpf-fixture");
        auto found=platform.locateProgram({"qualpf-fixture","external/tool",second.toStdString()});
        check(found.found && found.path==(second+"/qualpf-fixture").toStdString(),"Saved executable precedes PATH");
        settings.setValue("external/tool",directory.path()+"/missing");
        found=platform.locateProgram({"qualpf-fixture","external/tool",second.toStdString()});
        check(found.found && found.path==(first+"/qualpf-fixture").toStdString(),"Stale setting removal and PATH priority");
        settings.remove("external/tool");qputenv("PATH",{});
        found=platform.locateProgram({"qualpf-fixture","external/tool",second.toStdString()});
        check(found.found && found.path==(second+"/qualpf-fixture").toStdString(),"Folder fallback");
        check(!platform.locateProgram({"missing","external/missing",{}}).found,"Missing program remains failure");
        check(platform.fileExists(found.path),"Owned lookup path");
        const auto info = platform.fileInformation(found.path);
        const QFileInfo prior(QString::fromStdString(found.path));
        check(info.exists && info.lastModifiedMilliseconds &&
              QDateTime::fromMSecsSinceEpoch(*info.lastModifiedMilliseconds).toString("dd/MM/yyyy HH:mm:ss") ==
                  prior.lastModified().toString("dd/MM/yyyy HH:mm:ss"), "Report timestamp preserves prior local display");
        const auto missing = platform.fileInformation((directory.path()+"/missing").toStdString());
        check(!missing.exists && !missing.lastModifiedMilliseconds, "Missing file has no timestamp");
        std::cout<<"PASS: process modes, single delivery, owned results, lookup precedence and update interpretation\n";
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
