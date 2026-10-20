#pragma once
#include "qualpf/application/product_files.h"
#include <QJsonObject>
namespace qualpf::infrastructure::legacy_fixture {
Result<QJsonObject> encodeProjectV3(application::ProductHandle, const QString &created);
Result<application::ProductState> decodeProjectV3(const QJsonObject &, application::SearchBackend);
} // namespace qualpf::infrastructure
