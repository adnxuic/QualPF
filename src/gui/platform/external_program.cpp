#include "gui/platform/external_program.h"

#include <QDebug>


#include <QMessageBox>

namespace qualpf::gui {
/**
 * @brief Finds an external program by searching in QSettings, system PATH, and a specific folder.
 * @param screen If true, display a QMessageBox in case of error; otherwise, log to stderr.
 * @param appName The name of the program/executable to search for (e.g., "python").
 * @param keySettingsName The key name in QSettings where the found path should be stored.
 * @param folderName An additional folder to search in; can be empty.
 * @param path [out] Will be set to the found path if the program is located.
 * @return true if found, false otherwise.
 */
bool findExternalProgram(application::PlatformServices &platform, bool screen, const QString &appName, const QString &keySettingsName, const QString &folderName, QString &path)
{
    auto found=platform.locateProgram({appName.toStdString(),keySettingsName.toStdString(),folderName.toStdString()});
    if(found.found) { path=QString::fromStdString(found.path); return true; }

    // 4. Error reporting
    const QString msg = folderName.isEmpty()
        ? QObject::tr("The program %1 was not found.\nPlease make sure it is installed and available in the system PATH.").arg(appName)
        : QObject::tr("The program %1 was not found.\nPlease install it or copy it to the folder:\n%2").arg(appName, folderName);
    if (screen) {
        QMessageBox::warning(nullptr, QObject::tr("Program not found"), msg);
    } else {
        qCritical() << msg;
    }

    return false;
}

}
