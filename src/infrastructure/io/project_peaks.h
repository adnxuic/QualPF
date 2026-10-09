#pragma once

#include "qualpf/application/product_values.h"
#include <QJsonObject>
#include <QString>
#include <optional>

namespace qualpf::infrastructure {

// v1 only recorded surviving original intensities. They cannot be used to
// reconstruct a historical residual. v2 records both columns, including empty
// residuals. Decode into a value before publishing any application state.
QJsonObject encode_project_peaks(const application::ProductPeaks &peaks);
std::optional<application::ProductPeaks> decode_project_peaks(const QJsonObject &object, int version,
                                                              QString &error);
// Validate array dimensions before the GUI calls the unbounded legacy ABI.
bool validate_project_arrays(const QJsonObject &root, QString &error);

} // namespace qualpf::infrastructure
