#include "qualpf/application/platform_services.h"
#include <regex>
#include <sstream>
namespace qualpf::application {
ProcessRequest maintenanceRequest(const PlatformServices &platform,bool check) {
    return {platform.maintenanceExecutable(),{check ? "check-updates" : "--start-updater"}};
}
MaintenanceOutcome maintenanceOutcome(const ProcessResult &result) {
    MaintenanceOutcome out;
    if (result.exitCode==1) { out.warning=true; return out; }
    if (result.exitCode!=0) return out;
    out.notify=true;
    const auto &text=result.standardOutput;
    if(text.empty() || text.find("no updates available")!=std::string::npos || text.find("Warning:")!=std::string::npos) return out;
    out.available=true; out.details=text;
    std::istringstream lines(text); std::string line; bool enabled=false;
    const std::regex version("version=\"([^\"]+)\"");
    while(std::getline(lines,line)) {
        if(line.starts_with("<updates>")) enabled=true;
        else if(line.starts_with("</updates>")) break;
        else if(enabled) { std::smatch match; if(std::regex_search(line,match,version)) out.latestVersion=match[1]; }
    }
    return out;
}
}
