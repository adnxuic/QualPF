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
#include <iostream>
#include <stdexcept>
using namespace qualpf::application;
void check(bool ok,const char *reason) { if(!ok) throw std::runtime_error(reason); }
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    if(app.arguments().contains("--process-child")) {
        std::cout<<"<updates>\n <update name=\"QualPF\" version=\"2.3.4\"/>\n</updates>\n"; return 0;
    }
    try {
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
