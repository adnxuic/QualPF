#pragma once
#include "qualpf/application/workspace_ports.h"
#include <QJsonObject>
#include <QString>
namespace qualpf::infrastructure {
Result<application::ProductState> decodeProject(const QJsonObject &, application::SearchBackend,
                                                const application::MetadataObject &defaults);
Result<QJsonObject> encodeProject(application::ProductHandle, const application::MetadataObject &defaults,
                                  const QString &created);
Result<application::ProductState> readProject(const QString &, application::SearchBackend,
                                              const application::MetadataObject &defaults);
class ProjectFileReader final : public application::ProjectSourceReader {
  public:
    Result<application::ProductState> read(const std::string &path, application::SearchBackend backend,
                                           const application::MetadataObject &defaults) const override {
        return readProject(QString::fromStdString(path), backend, defaults);
    }
};
Status writeProject(const QString &, const QJsonObject &);
} // namespace qualpf::infrastructure
