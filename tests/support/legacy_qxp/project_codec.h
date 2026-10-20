#pragma once
#include "qualpf/application/product_files.h"
#include <QJsonObject>
#include <QString>
#include "legacy_ports.h"
namespace qualpf::infrastructure::legacy_fixture {
Result<application::ProductState> decodeProject(const QJsonObject &, application::SearchBackend,
                                                const application::MetadataObject &defaults);
Result<QJsonObject> encodeProject(application::ProductHandle, const application::MetadataObject &defaults,
                                  const QString &created);
Result<QJsonObject> encodeProject(const application::ProjectSerializationInput &, const application::MetadataObject &defaults,
                                  const QString &created);
Result<QJsonObject> encodeLegacyProject(application::ProductHandle, const application::MetadataObject &defaults,
                                  const QString &created);
Result<QJsonObject> encodeLegacyProject(const application::ProjectSerializationInput &, const application::MetadataObject &defaults,
                                  const QString &created);
Result<application::ProductState> readProject(const QString &, application::SearchBackend,
                                              const application::MetadataObject &defaults);
class ProjectFileReader final : public ProjectSourceReader {
  public:
    Result<application::ProductState> read(const std::string &path, application::SearchBackend backend,
                                           const application::MetadataObject &defaults) const override {
        return readProject(QString::fromStdString(path), backend, defaults);
    }
};
Status writeProject(const QString &, const QJsonObject &);
} // namespace qualpf::infrastructure
