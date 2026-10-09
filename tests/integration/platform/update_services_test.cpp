#include "qualpf/application/update_service.h"
#include "infrastructure/network/update_service.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <iostream>
#include <stdexcept>

void check(bool value, const char *reason) { if (!value) throw std::runtime_error(reason); }
bool priorComparison(const QString &x, const QString &y) {
    const auto a = x.split('.'), b = y.split('.');
    for (int i = 0; i < qMin(a.size(), b.size()); ++i) {
        const int left = a[i].toInt(), right = b[i].toInt();
        if (left > right) return true;
        if (right > left) return false;
    }
    return b.size() < a.size();
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        using namespace qualpf::application;
        qualpf::infrastructure::QtUpdateServiceProvider provider;
        const QStringList versions{"", "1", "1.0", "1.0.0", "2.3.4", "0.1", "1.bad", "1.+2", "1. 2 ", "2147483648.1", "-1.0", "1..0"};
        for (const auto &latest : versions) for (const auto &current : versions) {
            QJsonObject platform{{"latest-version", latest}};
            const auto body = QJsonDocument(QJsonObject{{"updates", QJsonObject{{"fixture", platform}}}}).toJson();
            const auto outcome = evaluateUpdateReply(provider, {{}, false, body.toStdString()}, "fixture", current.toStdString());
            check(outcome.hasDocument && outcome.available == priorComparison(latest, current), "Existing Qt version comparison, including malformed components");
        }
        check(!evaluateUpdateReply(provider, {{}, false, "invalid"}, "fixture", "1").hasDocument, "Invalid document");
        check(!evaluateUpdateReply(provider, {{}, true, "{}"}, "fixture", "1").hasDocument, "Network error precedence");
        check(evaluateUpdateReply(provider, {"next", true, "invalid"}, "fixture", "1").redirect == "next", "Redirect precedence");

        QTcpServer server;
        check(server.listen(QHostAddress::LocalHost, 0), "Local update server");
        int requests = 0; bool userAgent = false; QByteArray received;
        QObject::connect(&server, &QTcpServer::newConnection, &server, [&] {
            auto *socket = server.nextPendingConnection();
            auto bytes = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, bytes] {
                *bytes += socket->readAll();
                if (!bytes->contains("\r\n\r\n")) return;
                ++requests; received += *bytes;
                userAgent = userAgent || bytes->toLower().contains("user-agent: qualpf-update-fixture");
                QByteArray response;
                if (bytes->startsWith("GET /redirect ")) {
                    response = "HTTP/1.1 302 Found\r\nLocation: /document\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                } else {
                    const QByteArray body = R"({"updates":{"fixture":{"open-url":"open","changelog":"changes","download-url":"download","latest-version":"2.3.4"}}})";
                    response = "HTTP/1.1 200 OK\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
                }
                socket->write(response); socket->disconnectFromHost(); bytes->clear();
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        });
        auto session = provider.makeSession();
        QEventLoop loop; QTimer timeout; timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        int completions = 0; UpdateReply reply;
        session->get({"http://127.0.0.1:" + std::to_string(server.serverPort()) + "/redirect", "QualPF-Update-Fixture"},
            [&](UpdateReply value) { ++completions; reply = std::move(value); loop.quit(); });
        timeout.start(10000); loop.exec(); session.reset();
        if (completions != 1 || requests != 2 || !userAgent || reply.networkError)
            std::cerr << "completions=" << completions << " requests=" << requests << " agent=" << userAgent
                      << " error=" << reply.networkError << " " << reply.errorDescription << " redirect=" << reply.redirect << " headers=" << received.constData() << '\n';
        check(completions == 1 && requests == 2 && userAgent && !reply.networkError, "Asynchronous redirect, original header and single completion");
        const auto outcome = evaluateUpdateReply(provider, reply, "fixture", "1.0");
        check(outcome.hasDocument && outcome.available && outcome.document.latestVersion == "2.3.4" &&
              outcome.document.openUrl == "open" && outcome.document.changelog == "changes" &&
              outcome.document.downloadUrl == "download", "Owned reply survives transport destruction");
        std::cout << "PASS: update redirect, transport ownership, JSON and version behavior\n";
    } catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
