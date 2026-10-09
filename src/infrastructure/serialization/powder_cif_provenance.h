#pragma once
#include "qualpf/application/import/powder_cif_provenance.h"
#include <QJsonObject>
#include <QJsonValue>
namespace qualpf::infrastructure {
using application::PowderCifProvenance;
QJsonObject encodePowderCifProvenance(const PowderCifProvenance &);
Result<std::shared_ptr<const PowderCifProvenance>> decodePowderCifProvenance(const QJsonValue &, const std::string &sourceId);
}
namespace qualpf::gui {
using infrastructure::encodePowderCifProvenance;
using infrastructure::decodePowderCifProvenance;
}
