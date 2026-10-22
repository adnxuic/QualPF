#include "gui/dialogs/aboutdialog.h"
#include "config.h"

#include <QDialogButtonBox>
#include <QResource>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QLabel *textLabel(const QString &text, const char *name, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QString::fromLatin1(name));
    label->setWordWrap(true);
    auto policy = label->sizePolicy();
    policy.setVerticalPolicy(QSizePolicy::Maximum);
    label->setSizePolicy(policy);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    label->setOpenExternalLinks(true);
    return label;
}
}

AboutDialog::AboutDialog(const QString &settingsFileName, const QString &buildInformation,
                         QWidget *parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("aboutDialog"));
    setWindowTitle(tr("About %1").arg(APP_DISPLAY_NAME));
    auto *layout = new QVBoxLayout(this);
    auto *header = new QHBoxLayout;
    auto *icon = new QLabel(this);
    icon->setObjectName(QStringLiteral("applicationIcon"));
    const qreal ratio = devicePixelRatioF();
    QPixmap pixmap(QStringLiteral(":/branding/qualpf.png"));
    pixmap = pixmap.scaled(qRound(96 * ratio), qRound(96 * ratio),
                           Qt::KeepAspectRatio, Qt::SmoothTransformation);
    pixmap.setDevicePixelRatio(ratio);
    icon->setPixmap(pixmap);
    icon->setFixedSize(96, 96);
    header->addWidget(icon, 0, Qt::AlignTop);
    auto *heading = new QVBoxLayout;
    auto *title = textLabel(tr("%1 %2").arg(APP_DISPLAY_NAME, APP_VERSION), "productTitle", this);
    auto font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.5);
    font.setBold(true);
    title->setFont(font);
    heading->addWidget(title);
    heading->addWidget(textLabel(APP_DESCRIPTION, "productDescription", this));
    heading->addWidget(textLabel(tr("Independently maintained and published by %1.")
                                    .arg(APP_PUBLISHER), "publisherLabel", this));
    header->addLayout(heading, 1);
    layout->addLayout(header);
    layout->addWidget(textLabel(APP_COPYRIGHT, "copyrightLabel", this));
    layout->addWidget(textLabel(tr("Derived from QualX3, originally developed at IC-CNR, Bari. "
        "Original authorship is retained. No institutional endorsement or official "
        "QualX distribution status is claimed."), "originLabel", this));
    layout->addWidget(textLabel(tr("%1. You may copy, modify and redistribute the covered "
        "application under this license. There is no warranty, to the extent permitted "
        "by law. Third-party components and databases retain their own terms.")
        .arg(APP_LICENSE), "licenseSummary", this));

    auto *details = new QFormLayout;
    details->setRowWrapPolicy(QFormLayout::WrapLongRows);
    websiteLabel_ = textLabel({}, "websiteLabel", this);
    contactsLabel_ = textLabel({}, "contactsLabel", this);
    citationLabel_ = textLabel({}, "citationLabel", this);
    setWebsiteUrl(APP_HOMEPAGE);
    setContactsUrl(APP_SUPPORT);
    setCitationUrl(QStringLiteral("https://doi.org/10.1017/S0885715617000240"),
                   tr("QualX research references"));
    details->addRow(tr("Project:"), websiteLabel_);
    details->addRow(tr("Support:"), contactsLabel_);
    details->addRow(tr("Upstream citation:"), citationLabel_);
    auto *settings = textLabel(settingsFileName, "settingsFileLabel", this);
    settings->setTextFormat(Qt::PlainText);
    details->addRow(tr("User settings file:"), settings);
    auto *build = textLabel(buildInformation, "buildInformation", this);
    build->setTextFormat(Qt::PlainText);
    details->addRow(tr("Build:"), build);
    layout->addLayout(details);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    auto *legal = buttons->addButton(tr("License and notices"), QDialogButtonBox::HelpRole);
    legal->setObjectName(QStringLiteral("legalNoticesButton"));
    connect(legal, &QPushButton::clicked, this, &AboutDialog::showLegalNotices);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
    resize(660, layout->totalHeightForWidth(660));
}

void AboutDialog::showLegalNotices()
{
    auto *dialog = new QDialog(this);
    dialog->setObjectName(QStringLiteral("legalNoticesDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setAttribute(Qt::WA_DontShowOnScreen, testAttribute(Qt::WA_DontShowOnScreen));
    dialog->setWindowTitle(tr("%1 license and notices").arg(APP_DISPLAY_NAME));
    auto *layout = new QVBoxLayout(dialog);
    auto *tabs = new QTabWidget(dialog);
    tabs->setObjectName(QStringLiteral("legalNoticesTabs"));
    const auto addDocument = [&](const QString &title, const QString &path) {
        auto *browser = new QTextBrowser(tabs);
        browser->setOpenExternalLinks(true);
        browser->setObjectName(path);
        if (path == QStringLiteral("LICENSE")) {
            const QResource resource(QStringLiteral(":/legal/LICENSE"));
            if (resource.isValid())
                browser->setPlainText(QString::fromUtf8(resource.uncompressedData()));
            else
                browser->setPlainText(tr("The embedded license could not be loaded."));
        } else {
            browser->setSource(QUrl(QStringLiteral("qrc:/legal/") + path));
        }
        tabs->addTab(browser, title);
    };
    addDocument(tr("GPLv3"), QStringLiteral("LICENSE"));
    addDocument(tr("Origin and changes"), QStringLiteral("NOTICE.md"));
    addDocument(tr("Third-party notices"), QStringLiteral("THIRD_PARTY_NOTICES.md"));
    layout->addWidget(tabs);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->resize(780, 540);
    dialog->show();
}

void AboutDialog::setWebsiteUrl(const QString &url, const QString &text)
{
    websiteLabel_->setText(linkHtml(url, text));
}
void AboutDialog::setContactsUrl(const QString &url, const QString &text)
{
    contactsLabel_->setText(linkHtml(url, text));
}
void AboutDialog::setCitationUrl(const QString &url, const QString &text)
{
    citationLabel_->setText(linkHtml(url, text));
}
QString AboutDialog::linkHtml(const QString &url, const QString &text)
{
    return QStringLiteral("<a href=\"%1\">%2</a>")
        .arg(url.toHtmlEscaped(), (text.isEmpty() ? url : text).toHtmlEscaped());
}
