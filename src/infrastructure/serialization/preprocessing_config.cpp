#include "infrastructure/serialization/preprocessing_config.h"
#include "qualpf/domain/validation.h"
#include <QJsonArray>
#include "infrastructure/serialization/metadata_values.h"
#include <cmath>
namespace qualpf::infrastructure {
using namespace application;
QJsonObject encodePreprocessing(const PreprocessingConfiguration &c) {
    return infrastructure::jsonValue(MetadataValue{preprocessingValues(c)}).toObject();
}
Result<PreprocessingConfiguration> decodePreprocessing(const QJsonObject &root) {
    const auto error = Error{ErrorCode::invalid_input, "preprocessing.configuration",
                             "Invalid preprocessing configuration", "invalid_configuration"};
    PreprocessingConfiguration out;
    if (root.isEmpty())
        return Result<PreprocessingConfiguration>{out}; // Old project: unknown.
    if (root["version"] != QJsonValue(1) && root["version"] != QJsonValue(2))
        return Result<PreprocessingConfiguration>{error};
    auto integer = [](const QJsonObject &o, const char *k, int lo, int hi) {
        auto v = o[k];
        return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() == std::floor(v.toDouble()) &&
               v.toDouble() >= lo && v.toDouble() <= hi;
    };
    auto real = [](const QJsonObject &o, const char *k, double lo, double hi) {
        auto v = o[k];
        return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() >= lo && v.toDouble() <= hi;
    };
    if (root.contains("candidate_failure")) {
        if (root["candidate_failure"] != QJsonValue("nonfinite_candidate_measurement") ||
            root.contains("candidate_pool"))
            return Result<PreprocessingConfiguration>{error};
        out.candidate_failure = root["candidate_failure"].toString().toStdString();
    }
    if (root.contains("smoothing_applied")) {
        if (!root["smoothing_applied"].isBool())
            return Result<PreprocessingConfiguration>{error};
        out.smoothing_applied = root["smoothing_applied"].toBool();
    }
    if (root.contains("candidate_pool")) {
        if (root["version"] != QJsonValue(2) || !root["candidate_pool"].isObject())
            return Result<PreprocessingConfiguration>{error};
        auto object = root["candidate_pool"].toObject();
        if (!object["rows"].isArray() || !real(object, "wave", 1e-12, 8) ||
            !integer(object, "precision", 0, 2) || !integer(object, "scale", 0, 2))
            return Result<PreprocessingConfiguration>{error};
        PeakSet pool;
        bool ok = false;
        pool.dataset = {object["dataset"].toString().toULongLong(&ok)};
        if (!ok)
            return Result<PreprocessingConfiguration>{error};
        pool.effective_wavelength = WavelengthAngstrom{object["wave"].toDouble()};
        pool.source_precision = SourcePrecision(object["precision"].toInt());
        pool.intensity_scale = IntensityScale(object["scale"].toInt());
        for (auto value : object["rows"].toArray()) {
            if (!value.isObject())
                return Result<PreprocessingConfiguration>{error};
            auto row = value.toObject();
            for (auto key : {"angle", "height", "d", "delta_d", "fwhm", "original_height"})
                if (!row[key].isDouble() || !std::isfinite(row[key].toDouble()))
                    return Result<PreprocessingConfiguration>{error};
            Peak peak;
            peak.id = {row["id"].toString().toULongLong(&ok)};
            if (!ok)
                return Result<PreprocessingConfiguration>{error};
            peak.two_theta = TwoThetaDegrees{row["angle"].toDouble()};
            peak.intensity = row["height"].toDouble();
            peak.original_intensity = row["original_height"].toDouble();
            peak.d_spacing = DSpacingAngstrom{row["d"].toDouble()};
            peak.delta_d = DSpacingAngstrom{row["delta_d"].toDouble()};
            peak.fwhm = AngularWidthDegrees{row["fwhm"].toDouble()};
            if (row.contains("area")) {
                if (!row["area"].isDouble() || !std::isfinite(row["area"].toDouble()))
                    return Result<PreprocessingConfiguration>{error};
                peak.integrated_intensity = row["area"].toDouble();
            }
            if (!pool.peaks.empty() && peak.two_theta.value < pool.peaks.back().two_theta.value)
                return Result<PreprocessingConfiguration>{error};
            pool.peaks.push_back(peak);
        }
        if (!validate(pool).empty())
            return Result<PreprocessingConfiguration>{error};
        out.peak_candidates = std::move(pool);
    }
    if (root.contains("background_points")) {
        if (!root["background_points"].isArray())
            return Result<PreprocessingConfiguration>{error};
        out.background_points.emplace();
        for (auto point : root["background_points"].toArray()) {
            if (!point.isArray())
                return Result<PreprocessingConfiguration>{error};
            auto xy = point.toArray();
            if (xy.size() != 2 || !xy[0].isDouble() || !xy[1].isDouble() ||
                !std::isfinite(float(xy[0].toDouble())) || !std::isfinite(float(xy[1].toDouble())) ||
                (!out.background_points->empty() && xy[0].toDouble() < out.background_points->back().x))
                return Result<PreprocessingConfiguration>{error};
            out.background_points->push_back({float(xy[0].toDouble()), float(xy[1].toDouble())});
        }
    }
    if (root.contains("background_filters")) {
        if (!root["background_filters"].isArray())
            return Result<PreprocessingConfiguration>{error};
        for (auto value : root["background_filters"].toArray()) {
            auto o = value.toObject();
            if (!real(o, "minimum", 0, 180) || !real(o, "maximum", 0, 180) ||
                o["minimum"].toDouble() > o["maximum"].toDouble() || !integer(o, "iterations", 0, 10000) ||
                !integer(o, "half_window", 1, 100000))
                return Result<PreprocessingConfiguration>{error};
            out.background_filters.push_back({o["minimum"].toDouble(), o["maximum"].toDouble(),
                                              o["iterations"].toInt(), o["half_window"].toInt()});
        }
    }
    if (root.contains("selection")) {
        if (!root["selection"].isObject())
            return Result<PreprocessingConfiguration>{error};
        auto o = root["selection"].toObject();
        if (!real(o, "minimum", 0, 180) || !real(o, "maximum", 0, 180) ||
            o["minimum"].toDouble() > o["maximum"].toDouble() || !real(o, "threshold", 0, 100) ||
            !integer(o, "maximum_peaks", -1, 1000000) || !o["append"].isBool())
            return Result<PreprocessingConfiguration>{error};
        out.selection =
            PeakSelection{o["minimum"].toDouble(), o["maximum"].toDouble(), float(o["threshold"].toDouble()),
                          o["maximum_peaks"].toInt(), o["append"].toBool()};
    }
    if (root.contains("background")) {
        if (!root["background"].isObject())
            return Result<PreprocessingConfiguration>{error};
        auto o = root["background"].toObject();
        if (!integer(o, "method", 0, 6) || !o["automatic"].isBool() || !integer(o, "coefficients", 0, 36) ||
            !integer(o, "iterations", 0, 100000) || !integer(o, "half_window", 0, 100000))
            return Result<PreprocessingConfiguration>{error};
        out.background = preprocessing::BackgroundOptions{
            preprocessing::BackgroundMethod(o["method"].toInt()), o["automatic"].toBool(),
            o["coefficients"].toInt(), o["iterations"].toInt(), o["half_window"].toInt()};
        if (o.contains("minimum") || o.contains("maximum")) {
            if (!real(o, "minimum", 0, 180) || !real(o, "maximum", 0, 180) ||
                o["minimum"].toDouble() > o["maximum"].toDouble())
                return Result<PreprocessingConfiguration>{error};
            out.background->bruckner_range = AngularRange{TwoThetaDegrees{o["minimum"].toDouble()},
                                                          TwoThetaDegrees{o["maximum"].toDouble()}};
        }
    }
    if (root.contains("smoothing")) {
        if (!root["smoothing"].isObject())
            return Result<PreprocessingConfiguration>{error};
        auto o = root["smoothing"].toObject();
        if (!integer(o, "method", 0, 1) || !integer(o, "half_window", 1, 100000) ||
            !integer(o, "degree", 0, 20) || !integer(o, "derivative", 0, 0))
            return Result<PreprocessingConfiguration>{error};
        out.smoothing = preprocessing::SmoothingOptions{preprocessing::SmoothingMethod(o["method"].toInt()),
                                                        o["half_window"].toInt(), o["degree"].toInt(),
                                                        o["derivative"].toInt()};
    }
    if (root.contains("peaks")) {
        if (!root["peaks"].isObject())
            return Result<PreprocessingConfiguration>{error};
        auto o = root["peaks"].toObject();
        if (!real(o, "threshold", 0, 100) || !real(o, "slope", 0, 1e6) || !real(o, "range", -1, 180) ||
            !integer(o, "half_window", 1, 100) || !integer(o, "degree", 3, 20) ||
            !integer(o, "maximum", -1, 1000000) || !integer(o, "minimum", -1, 1000000) ||
            !o["automatic_window"].isBool())
            return Result<PreprocessingConfiguration>{error};
        preprocessing::PeakOptions p;
        p.threshold_percent = float(o["threshold"].toDouble());
        p.slope = float(o["slope"].toDouble());
        p.search_range = float(o["range"].toDouble());
        p.half_window = o["half_window"].toInt();
        p.polynomial_degree = o["degree"].toInt();
        p.maximum_peaks = o["maximum"].toInt();
        p.minimum_peaks = o["minimum"].toInt();
        p.automatic_window = o["automatic_window"].toBool();
        if (o.contains("detection_minimum") || o.contains("detection_maximum")) {
            if (!real(o, "detection_minimum", 0, 180) || !real(o, "detection_maximum", 0, 180) ||
                o["detection_minimum"].toDouble() > o["detection_maximum"].toDouble())
                return Result<PreprocessingConfiguration>{error};
            p.detection_range = AngularRange{TwoThetaDegrees{o["detection_minimum"].toDouble()},
                                             TwoThetaDegrees{o["detection_maximum"].toDouble()}};
        }
        out.peaks = p;
    }
    return Result<PreprocessingConfiguration>{out};
}
} // namespace qualpf::application
