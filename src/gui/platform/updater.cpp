#include <QMessageBox>
#include <QApplication>
#include <QDesktopServices>

#include "gui/platform/updater.h"
#include "gui/state/workspace_context.h"
#include <QUrl>

Updater::Updater(QObject *parent)
    : QObject{parent}, provider_(qualpf::gui::workspaceContext(this).updates()), session_(provider_.makeSession())
{
    m_url = "";
    m_openUrl = "";
    m_changelog = "";
    m_downloadUrl = "";
    m_latestVersion = "";
    m_notifyOnUpdate = true;
    m_notifyOnFinish = false;
    m_updateAvailable = false;
    m_moduleName = qApp->applicationName();
    m_moduleVersion = qApp->applicationVersion();


#if defined Q_OS_WIN
    m_platform = "windows";
#elif defined Q_OS_LINUX
    m_platform = "linux";
#endif

    setUserAgentString(QString("%1/%2 (Qt; Updater)").arg(qApp->applicationName(), qApp->applicationVersion()));
}

Updater::~Updater()
{

}

QString Updater::url() const
{
    return m_url;
}

QString Updater::openUrl() const
{
    return m_openUrl;
}

QString Updater::changelog() const
{
    return m_changelog;
}

QString Updater::moduleName() const
{
    return m_moduleName;
}

QString Updater::platformKey() const
{
    return m_platform;
}

QString Updater::downloadUrl() const
{
    return m_downloadUrl;
}

QString Updater::latestVersion() const
{
    return m_latestVersion;
}

QString Updater::moduleVersion() const
{
    return m_moduleVersion;
}

QString Updater::userAgentString() const
{
    return m_userAgentString;
}

bool Updater::notifyOnUpdate() const
{
    return m_notifyOnUpdate;
}

bool Updater::notifyOnFinish() const
{
    return m_notifyOnFinish;
}

bool Updater::updateAvailable() const
{
    return m_updateAvailable;
}

void Updater::checkForUpdates()
{
    session_->get({url().toStdString(), userAgentString().toStdString()},
                  [this](auto reply) { onReply(std::move(reply)); });
}

void Updater::setUrl(const QString &url)
{
    m_url = url;
}

void Updater::setModuleName(const QString &name)
{
    m_moduleName = name;
}

void Updater::setNotifyOnUpdate(const bool notify)
{
    m_notifyOnUpdate = notify;
}

void Updater::setNotifyOnFinish(const bool notify)
{
    m_notifyOnFinish = notify;
}

void Updater::setUserAgentString(const QString &agent)
{
    m_userAgentString = agent;
}

void Updater::setModuleVersion(const QString &version)
{
    m_moduleVersion = version;
}

void Updater::setPlatformKey(const QString &platformKey)
{
    m_platform = platformKey;
}

void Updater::onReply(qualpf::application::UpdateReply reply)
{
    const auto outcome = qualpf::application::evaluateUpdateReply(provider_, reply,
        platformKey().toStdString(), moduleVersion().toStdString());
    if (!outcome.redirect.empty()) {
        setUrl(QString::fromStdString(outcome.redirect));
        checkForUpdates();
        return;
    }
    if (outcome.hasDocument) {
        m_openUrl = QString::fromStdString(outcome.document.openUrl);
        m_changelog = QString::fromStdString(outcome.document.changelog);
        m_downloadUrl = QString::fromStdString(outcome.document.downloadUrl);
        m_latestVersion = QString::fromStdString(outcome.document.latestVersion);
    }
    setUpdateAvailable(outcome.available);
    emit checkingFinished(url());
}

void Updater::setUpdateAvailable(const bool available)
{
    m_updateAvailable = available;

    QMessageBox box;
    box.setTextFormat(Qt::RichText);
    box.setIcon(QMessageBox::Information);

    if (updateAvailable() && (notifyOnUpdate() || notifyOnFinish()))
    {
        QString text = tr("Would you like to download the update now?");

        QString title
            = "<h3>" + tr("Version %1 of %2 has been released!").arg(latestVersion(), moduleName()) + "</h3>";

        box.setText(title);
        box.setInformativeText(text);
        box.setStandardButtons(QMessageBox::No | QMessageBox::Yes);
        box.setDefaultButton(QMessageBox::Yes);

        if (box.exec() == QMessageBox::Yes)
        {
            if (!openUrl().isEmpty())
                QDesktopServices::openUrl(QUrl(openUrl()));
            else
                QDesktopServices::openUrl(QUrl(downloadUrl()));
        }
    }
    else if (notifyOnFinish())
    {
        box.setStandardButtons(QMessageBox::Close);
        box.setInformativeText(tr("No updates are available for the moment"));
        box.setText("<h3>"
                    + tr("Congratulations! You are running the "
                         "latest version of %1")
                          .arg(moduleName())
                    + "</h3>");

        box.exec();
    }

}
