#include "gui/state/workspace_context.h"
#include <QGuiApplication>
#include "gui/platform/maintenancetool.h"

#include <QCoreApplication>

#include <QMessageBox>
#include <QDebug>


MaintenanceTool::MaintenanceTool(QObject *parent)
    : QObject{parent},
    m_state(MaintenanceTool::NotRunning),
    m_hasUpdate(false),
    platform_(qualpf::gui::workspaceContext(this).platform()),
    m_latestVersion("")
{
    m_process=platform_.makeProcess();
}

void MaintenanceTool::checkUpdate()
{
    startMaintenanceTool(MaintenanceTool::CheckUpdate);
}

void MaintenanceTool::startMaintenanceTool(StartMode mode)
{
    auto request=qualpf::application::maintenanceRequest(platform_,mode==CheckUpdate);
    if(mode==CheckUpdate) {
        if(!m_process->running()) {
            setUpdateDetails(QString()); setHasUpdate(false);
            m_process->start(std::move(request),{[this] { processStarted(); },
                [this](auto result) { processFinished(std::move(result)); },
                [this](int error) { processError(error); }});
        } else qDebug()<<"Already started.";
    } else m_process->detach(std::move(request));
}

// State of the maintenance tool
MaintenanceTool::ProcessState MaintenanceTool::state() const
{
    return m_state;
}

void MaintenanceTool::setState(ProcessState state)
{
    if (m_state == state) return;
    m_state = state;
    emit stateChanged(m_state);
}

void MaintenanceTool::notifyAboutUpdate()
{
    QMessageBox box;
    box.setTextFormat(Qt::RichText);
    box.setIcon(QMessageBox::Information);

    if (hasUpdate()) {
        QString text = tr("Would you like to download the update now?");

        QString title
            = "<h3>" + tr("Version %1 of %2 has been released!").arg(m_latestVersion, QGuiApplication::applicationDisplayName()) + "</h3>";

        box.setText(title);
        box.setInformativeText(text);
        box.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        box.setDefaultButton(QMessageBox::Yes);

        if (box.exec() == QMessageBox::Yes)
        {

        }

    } else {
        box.setStandardButtons(QMessageBox::Close);
        box.setInformativeText(tr("No updates are available for the moment"));
        box.setText("<h3>"
                    + tr("Congratulations! You are running the "
                         "latest version of %1")
                          .arg(QGuiApplication::applicationDisplayName())
                    + "</h3>");

        box.exec();
    }
}

// Check if there's an update
bool MaintenanceTool::hasUpdate() const
{
    return m_hasUpdate;
}

void MaintenanceTool::setHasUpdate(bool hasUpdate)
{
    if (m_hasUpdate == hasUpdate) return;
    m_hasUpdate = hasUpdate;
    emit hasUpdateChanged(m_hasUpdate);
}

QString MaintenanceTool::updateDetails() const
{
    return m_updateDetails;
}

void MaintenanceTool::setUpdateDetails(const QString &updateDetails)
{
    if (m_updateDetails == updateDetails) return;
    m_updateDetails = updateDetails;
    emit updateDetailsChanged(m_updateDetails);
}

// Process started
void MaintenanceTool::processStarted()
{
    setState(MaintenanceTool::Running);
}

// Process finished
void MaintenanceTool::processFinished(qualpf::application::ProcessResult result)
{
    const auto outcome=qualpf::application::maintenanceOutcome(result);
    if(result.exitCode==0) {
        qDebug()<<"out>"<<QString::fromStdString(result.standardOutput);
        if(outcome.available) {
            if(!outcome.latestVersion.empty()) m_latestVersion=QString::fromStdString(outcome.latestVersion);
            setUpdateDetails(QString::fromStdString(outcome.details)); setHasUpdate(true);
        } else qDebug()<<"No updates available.";
        notifyAboutUpdate();
    } else if(outcome.warning) {
        qDebug()<<"err>"<<QString::fromStdString(result.standardError);
        QMessageBox::warning(nullptr,"Check for Update","An error occurred while checking for a software update");
    }
    setState(MaintenanceTool::NotRunning);
}
void MaintenanceTool::processError(int)
{
    setState(MaintenanceTool::NotRunning);
}
