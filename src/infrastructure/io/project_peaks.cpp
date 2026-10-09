#include "infrastructure/io/project_peaks.h"

#include <QJsonArray>
#include <cmath>
#include <limits>

namespace qualpf::infrastructure {

bool validate_project_arrays(const QJsonObject &root, QString &error) {
    error.clear();
    const auto array = [&](const QJsonObject &object, const char *key, bool floatStorage) {
        if (!object[key].isArray())
            return false;
        for (const auto &value : object[key].toArray()) {
            if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
                (floatStorage && std::abs(value.toDouble()) > std::numeric_limits<float>::max()))
                return false;
        }
        return true;
    };
    if (root.contains("diffraction_data")) {
        const auto d = root["diffraction_data"].toObject();
        if (!array(d, "x", true) || !array(d, "y", true) ||
            d["x"].toArray().size() != d["y"].toArray().size()) {
            error = "Invalid diffraction x/y arrays";
            return false;
        }
        for (const char *key : {"has_back", "back_subtracted", "kalpha2_subtracted",
                                "original_kalpha2_subtracted", "intensity_scaled", "calculation_range_known"})
            if (d.contains(key) && !d[key].isBool()) {
                error = QString("Invalid processing flag: %1").arg(key);
                return false;
            }
        for (const char *key : {"yb", "wave", "ratio"}) {
            if (d.contains(key) && !array(d, key, true)) {
                error = QString("Invalid diffraction array: %1").arg(key);
                return false;
            }
        }
        if (d.contains("tmin") != d.contains("tmax") ||
            (d.contains("tmin") &&
             (!d["tmin"].isDouble() || !d["tmax"].isDouble() || !std::isfinite(d["tmin"].toDouble()) ||
              !std::isfinite(d["tmax"].toDouble()) || d["tmin"].toDouble() >= d["tmax"].toDouble()))) {
            error = "Invalid calculation range";
            return false;
        }
        if (d.contains("calculation_range_known") && !d["calculation_range_known"].toBool() &&
            d.contains("tmin")) {
            error = "Unknown calculation range cannot carry explicit limits";
            return false;
        }
        const double first = d.contains("background_first") ? d["background_first"].toDouble(-1) : 0;
        const auto count = d["x"].toArray().size(), backgroundCount = d["yb"].toArray().size();
        if (d.contains("background_first") &&
            (!d["background_first"].isDouble() || !std::isfinite(first) || first < 0 ||
             first != std::floor(first) || first > count || backgroundCount > count - first)) {
            error = "Invalid background sample window";
            return false;
        }
        if (d.contains("original_observed") &&
            (!array(d, "original_observed", false) || d["original_observed"].toArray().size() != count)) {
            error = "Invalid original intensity";
            return false;
        }
        if (d["wave"].toArray().size() != d["ratio"].toArray().size() ||
            (d["has_back"].toBool() && (!d["yb"].isArray() || backgroundCount > count))) {
            error = "Diffraction background or wavelength array lengths differ";
            return false;
        }
    }
    for (const char *key : {"results", "phases"}) {
        if (!root.contains(key))
            continue;
        if (!root[key].isArray()) {
            error = "Invalid card list";
            return false;
        }
        for (const auto &value : root[key].toArray()) {
            const auto card = value.toObject();
            if (!value.isObject() || !array(card, "d", false) || !array(card, "intensity", false) ||
                card["d"].toArray().size() != card["intensity"].toArray().size()) {
                error = "Invalid card reflection arrays";
                return false;
            }
            if (card.contains("scale") &&
                (!card["scale"].isDouble() || !std::isfinite(card["scale"].toDouble()))) {
                error = "Invalid previous scale";
                return false;
            }
            if (card["fom_calculated"].toBool()) {
                for (const char *field : {"fom", "fom_peakpos", "fom_intensity", "scale"}) {
                    if (!card[field].isDouble() || !std::isfinite(card[field].toDouble())) {
                        error = "Invalid saved card score or previous scale";
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

QJsonObject encode_project_peaks(const application::ProductPeaks &peaks) {
    const auto array = [](const std::vector<double> &values) {
        QJsonArray result;
        for (double value : values)
            result.append(value);
        return result;
    };
    QJsonObject result{{"wave", peaks.wave},
                       {"valid", peaks.valid},
                       {"tth", array(peaks.tth)},
                       {"d", array(peaks.d)},
                       {"delta_d", array(peaks.deltaD)},
                       {"fwhm", array(peaks.fwhm)},
                       {"intensity", array(peaks.intensityOrig)},
                       {"current_intensity", array(peaks.intensity)}};
    if (peaks.integratedIntensity)
        result["integrated_intensity"] = array(*peaks.integratedIntensity);
    return result;
}

std::optional<application::ProductPeaks> decode_project_peaks(const QJsonObject &object, int version,
                                                              QString &error) {
    error.clear();
    application::ProductPeaks peaks;
    if (version != 1 && version != 2) {
        error = "Unsupported project version";
        return std::nullopt;
    }
    const auto column = [&](const char *key, std::vector<double> &values, bool nonnegative = true) {
        if (!object[key].isArray()) {
            error = QString("Missing peak array: %1").arg(key);
            return false;
        }
        for (const QJsonValue &value : object[key].toArray()) {
            if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
                (nonnegative && value.toDouble() < 0)) {
                error = QString("Invalid peak value: %1").arg(key);
                return false;
            }
            values.push_back(value.toDouble());
        }
        return true;
    };
    if (!column("tth", peaks.tth) || !column("d", peaks.d) || !column("delta_d", peaks.deltaD) ||
        !column("fwhm", peaks.fwhm) || !column("intensity", peaks.intensityOrig))
        return std::nullopt;
    if (object.contains("integrated_intensity")) {
        peaks.integratedIntensity.emplace();
        if (!column("integrated_intensity", *peaks.integratedIntensity, false) ||
            peaks.integratedIntensity->size() != peaks.tth.size()) {
            error = "Invalid integrated intensity column";
            return std::nullopt;
        }
    }
    if (version == 2) {
        if (!column("current_intensity", peaks.intensity))
            return std::nullopt;
        if (!object["valid"].isBool()) {
            error = "Missing peak validity";
            return std::nullopt;
        }
        peaks.valid = object["valid"].toBool();
    } else {
        peaks.intensity = peaks.intensityOrig;
        peaks.valid = !peaks.tth.empty();
    }
    for (auto size : {peaks.d.size(), peaks.deltaD.size(), peaks.fwhm.size(), peaks.intensity.size(),
                      peaks.intensityOrig.size()}) {
        if (size != peaks.tth.size()) {
            error = "Peak array lengths differ";
            return std::nullopt;
        }
    }
    if (!object["wave"].isDouble() || !std::isfinite(object["wave"].toDouble())) {
        error = "Invalid peak wavelength";
        return std::nullopt;
    }
    peaks.wave = object["wave"].toDouble();
    if ((!peaks.tth.empty() || peaks.valid) && peaks.wave <= 0) {
        error = "Peak wavelength must be positive";
        return std::nullopt;
    }
    for (qsizetype i = 0; i < peaks.tth.size(); ++i) {
        if (!peaks.valid || peaks.d[i] <= 0 ||
            (i && (peaks.tth[i] < peaks.tth[i - 1] || peaks.d[i] > peaks.d[i - 1]))) {
            error = "Invalid peak validity or order";
            return std::nullopt;
        }
    }
    return peaks;
}

} // namespace qualpf::infrastructure
