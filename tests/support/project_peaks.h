#pragma once
#include "gui/model/experimentalpeaks.h"
#include "infrastructure/io/project_peaks.h"
#include "infrastructure/serialization/preprocessing_config.h"
namespace qualpf::gui {
inline QJsonObject encode_project_peaks(const ExperimentalPeaks &peaks) {
    return infrastructure::encode_project_peaks(peaks.productValue());
}
inline std::optional<ExperimentalPeaks> decode_project_peaks(const QJsonObject &object, int version,
                                                             QString &error) {
    auto value = infrastructure::decode_project_peaks(object, version, error);
    if (!value)
        return {};
    return ExperimentalPeaks::fromProductValue(*value);
}
} // namespace qualpf::gui
