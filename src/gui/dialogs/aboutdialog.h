#pragma once

#include <QDialog>

class QLabel;

// Read-only presentation: callers supply descriptions, never a canonical owner.
class AboutDialog : public QDialog
{
    Q_OBJECT
public:
    AboutDialog(const QString &settingsFileName, const QString &buildInformation,
                QWidget *parent = nullptr);
    void setWebsiteUrl(const QString &url, const QString &text = QString());
    void setContactsUrl(const QString &url, const QString &text = QString());
    void setCitationUrl(const QString &url, const QString &text = QString());

private:
    QLabel *websiteLabel_;
    QLabel *contactsLabel_;
    QLabel *citationLabel_;
    void showLegalNotices();
    static QString linkHtml(const QString &url, const QString &text = QString());
};
