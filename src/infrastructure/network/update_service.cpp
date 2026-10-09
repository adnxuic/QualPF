#include "update_service.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace qualpf::infrastructure {
namespace {
class QtUpdateSession final : public application::UpdateSession {
    QNetworkAccessManager manager_;
public:
    void get(application::UpdateRequest input, std::function<void(application::UpdateReply)> callback) override {
        QNetworkRequest request(QUrl(QString::fromStdString(input.url)));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        if (!input.userAgent.empty()) request.setRawHeader("User-Agent", QByteArray::fromStdString(input.userAgent));
        auto *reply = manager_.get(request);
        QObject::connect(reply, &QNetworkReply::finished, &manager_,
            [reply, callback = std::move(callback)] {
                application::UpdateReply result;
                result.redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl().toString().toStdString();
                result.networkError = reply->error() != QNetworkReply::NoError;
                if (result.networkError) result.errorDescription = reply->errorString().toStdString();
                if (result.redirect.empty() && !result.networkError) result.body = reply->readAll().toStdString();
                reply->deleteLater();
                if (callback) callback(std::move(result));
            });
    }
};
std::vector<int> versionParts(const QString &version) {
    std::vector<int> result;
    for (const auto &part : version.split('.')) result.push_back(part.toInt());
    return result;
}
}
std::unique_ptr<application::UpdateSession> QtUpdateServiceProvider::makeSession() {
    return std::make_unique<QtUpdateSession>();
}
application::UpdateDocument QtUpdateServiceProvider::decode(const std::string &body,
                                                           const std::string &platformKey,
                                                           const std::string &currentVersion) const {
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(body));
    application::UpdateDocument value;
    if (document.isNull()) return value;
    value.valid = true;
    const auto platform = document.object().value("updates").toObject().value(QString::fromStdString(platformKey)).toObject();
    value.openUrl = platform.value("open-url").toString().toStdString();
    value.changelog = platform.value("changelog").toString().toStdString();
    value.downloadUrl = platform.value("download-url").toString().toStdString();
    value.latestVersion = platform.value("latest-version").toString().toStdString();
    value.latestParts = versionParts(QString::fromStdString(value.latestVersion));
    value.currentParts = versionParts(QString::fromStdString(currentVersion));
    return value;
}
} // namespace qualpf::infrastructure
