#include "gui/dialogs/aboutdialog.h"
#include "config.h"

#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QUrl>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}
QByteArray read(const QString &path)
{
    QFile file(path);
    check(file.open(QIODevice::ReadOnly), "license source/resource must be readable");
    return file.readAll();
}
template<class T> T *child(QObject &parent, const char *name)
{
    auto *value = parent.findChild<T *>(QString::fromLatin1(name));
    check(value != nullptr, name);
    return value;
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    try {
        AboutDialog about(QStringLiteral("C:/temporary/IC/qualpf.ini"),
                          QStringLiteral("Release build\nC++20\nQt ") + qVersion());
        // Native Windows fonts/styles without opening a desktop test window.
        about.setAttribute(Qt::WA_DontShowOnScreen);
        about.show();
        app.processEvents();
        check(APP_VERSION == "1.0.0", "independent release version");
        check(child<QLabel>(about, "publisherLabel")->text().contains(APP_PUBLISHER), "independent publisher");
        check(child<QLabel>(about, "licenseSummary")->text().contains("GPL-3.0-only"), "precise application license");
        check(child<QLabel>(about, "websiteLabel")->text().contains(APP_HOMEPAGE), "QualPF project URL");
        check(child<QLabel>(about, "contactsLabel")->text().contains(APP_SUPPORT), "QualPF support URL");
        check(child<QLabel>(about, "settingsFileLabel")->text() == "C:/temporary/IC/qualpf.ini", "settings identity is a supplied read-only description");
        check(!QFile::exists(":/images/images/qualx.png") &&
              !QFile::exists(":/images/images/about.png") &&
              !QFile::exists(":/images/images/logo_CNR_compatto.png") &&
              !QFile::exists(":/images/images/logo_IC.png"), "legacy brand resources must not be embedded");
        check(QFile::exists(":/branding/qualpf.png"), "independent application mark");
        for (const auto *label : about.findChildren<QLabel *>()) {
            check(label->width() > 0 && label->height() > 0, "visible About label has space");
            const auto bounds = QRect(label->mapTo(&about, QPoint()), label->size());
            check(about.rect().contains(bounds), "About layout must keep labels inside the dialog");
        }
        if (argc > 1) {
            const QString output = QString::fromLocal8Bit(argv[1]);
            check(QDir().mkpath(QFileInfo(output).absolutePath()), "create screenshot directory");
            check(about.grab().save(output), "save scaled About screenshot");
        }
        child<QPushButton>(about, "legalNoticesButton")->click();
        app.processEvents();
        auto *legal = child<QDialog>(about, "legalNoticesDialog");
        auto *tabs = child<QTabWidget>(*legal, "legalNoticesTabs");
        check(tabs->count() == 3, "offline application, origin and third-party documents");
        const QString sourceRoot = QString::fromUtf8(QUALPF_ABOUT_SOURCE_ROOT);
        const auto review = QJsonDocument::fromJson(read(sourceRoot + "/deploy/rights-review.json")).object();
        const auto replaced = review.value("inherited_images").toArray();
        check(replaced.size() == 29, "all 29 inherited action images have replacement coverage");
        for (const auto &value : replaced) {
            const auto entry = value.toObject();
            check(entry.value("removed_from_source").toBool(), "old action image removed from current source");
            const QString oldName = QFileInfo(entry.value("path").toString()).fileName();
            check(!QFile::exists(":/images/images/" + oldName), "old action resource must not be embedded");
            const QString name = QFileInfo(entry.value("replacement").toString()).completeBaseName();
            const QString url = ":/icons/" + name + ".png";
            check(QFile::exists(":/icons/" + name + "@2x.png"), "replacement includes high density resource");
            const QIcon icon(url);
            for (const auto mode : {QIcon::Normal, QIcon::Disabled}) {
                const auto pixmap = icon.pixmap(QSize(16, 16), app.devicePixelRatio(), mode);
                check(!pixmap.isNull(), "replacement icon decodes in enabled and disabled modes");
                check(pixmap.width() == qRound(16 * app.devicePixelRatio()), "replacement renders at requested screen density");
            }
        }
        check(QIcon(":/icons/legend-show.png").pixmap(24).toImage() !=
              QIcon(":/icons/legend-hide.png").pixmap(24).toImage(), "legend states have distinct artwork");
        const auto license = read(sourceRoot + "/LICENSE");
        check(read(":/legal/LICENSE") == license, "embedded GPL must match the canonical source text");
        check(child<QTextBrowser>(*legal, "LICENSE")->toPlainText().contains("Conveying Non-Source Forms"), "full GPL is available offline");
        check(child<QTextBrowser>(*legal, "NOTICE.md")->toPlainText().contains("GPL-3.0-only"), "origin notice is rendered offline");
        auto *origin = child<QTextBrowser>(*legal, "NOTICE.md");
        origin->setSource(QUrl(QStringLiteral("qrc:/legal/SOURCE_CODE.md")));
        check(origin->toPlainText().contains("qualpf-1.0.0-Windows-Dependencies.tar.gz") &&
              origin->toPlainText().contains("qualpf-1.0.0-Linux-Dependencies.tar.gz") &&
              origin->toPlainText().contains("SHA256SUMS"), "source retrieval instructions open offline");
        check(read(":/legal/SOURCE_CODE.md") == read(sourceRoot + "/SOURCE_CODE.md"),
              "embedded source retrieval instructions match the delivered text");
        auto *thirdParty = child<QTextBrowser>(*legal, "THIRD_PARTY_NOTICES.md");
        check(thirdParty->toPlainText().contains("QCustomPlot 2.1.1"), "third-party notice is rendered offline");
        for (const auto &name : {"LGPL-3.0-only.txt", "MPL-2.0.txt", "Eigen-BSD.txt",
                                  "BSL-1.0.txt", "spglib-BSD-3-Clause.txt", "GCC-exception-3.1.txt"}) {
            const auto suffix = QStringLiteral("LICENSES/") + QString::fromLatin1(name);
            check(read(":/legal/" + suffix) == read(sourceRoot + "/" + suffix), "supplemental licenses remain verbatim offline");
        }
        thirdParty->setSource(QUrl(QStringLiteral("qrc:/legal/LICENSES/MPL-2.0.txt")));
        check(thirdParty->toPlainText().contains("Mozilla Public License"), "linked supplemental text opens without network access");
        about.setWebsiteUrl("https://example.invalid/?a=1&b=2", "<project>");
        check(child<QLabel>(about, "websiteLabel")->text().contains("&lt;project&gt;"), "link labels must escape supplied text");
        legal->close();
        about.close();
        std::cout << "PASS: independent About, offline legal documents and scaled layout\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
