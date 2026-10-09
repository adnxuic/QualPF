#pragma once
#include "qualpf/application/preprocessing_configuration.h"
#include <QJsonObject>
namespace qualpf::infrastructure {
using namespace application;
QJsonObject encodePreprocessing(const PreprocessingConfiguration &);
Result<PreprocessingConfiguration> decodePreprocessing(const QJsonObject &);
}
