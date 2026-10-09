#pragma once
#include <QString>
#include "qualpf/application/platform_services.h"
namespace qualpf::gui {
bool findExternalProgram(application::PlatformServices &, bool screen, const QString &appName, const QString &keySettingsName,
                         const QString &folderName, QString &path);
}
