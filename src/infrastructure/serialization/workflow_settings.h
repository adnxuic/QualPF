#pragma once
#include "qualpf/application/workflow_settings.h"
#include <QJsonObject>
namespace qualpf::infrastructure {
QJsonObject encodeWorkflowSettings(const application::ProjectSettings &);
Result<application::ProjectSettings> decodeWorkflowSettings(const QJsonObject &);
}
