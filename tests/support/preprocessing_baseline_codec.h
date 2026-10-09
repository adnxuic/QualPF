#pragma once
// Test-only independent serialization authority extracted from 9cb9c310.
// Original encoder SHA256: 0174adeef2bb7733d9cf43a9eaae204f4c474b1ead54cdf3b34c59daa9accca3
#include "infrastructure/serialization/preprocessing_config.h"
#include <QJsonArray>
namespace qualpf::testing {
using application::PreprocessingConfiguration;
QJsonObject encodePreprocessingBaseline(const PreprocessingConfiguration &c) {
    QJsonObject out{{"version", 2}};
    if (c.candidate_failure)
        out["candidate_failure"] = QString::fromStdString(*c.candidate_failure);
    if (c.smoothing_applied)
        out["smoothing_applied"] = *c.smoothing_applied;
    if (c.peak_candidates) {
        const auto &pool = *c.peak_candidates;
        QJsonArray rows;
        for (const auto &p : pool.peaks) {
            QJsonObject row{
                {"id", QString::number(p.id.value)}, {"angle", p.two_theta.value}, {"height", p.intensity}};
            if (p.d_spacing)
                row["d"] = p.d_spacing->value;
            if (p.delta_d)
                row["delta_d"] = p.delta_d->value;
            if (p.fwhm)
                row["fwhm"] = p.fwhm->value;
            if (p.original_intensity)
                row["original_height"] = *p.original_intensity;
            if (p.integrated_intensity)
                row["area"] = *p.integrated_intensity;
            rows.append(row);
        }
        QJsonObject object{{"dataset", QString::number(pool.dataset.value)},
                           {"precision", int(pool.source_precision)},
                           {"scale", int(pool.intensity_scale)},
                           {"rows", rows}};
        if (pool.effective_wavelength)
            object["wave"] = pool.effective_wavelength->value;
        out["candidate_pool"] = object;
    }
    if (c.background_points) {
        QJsonArray points;
        for (auto p : *c.background_points)
            points.append(QJsonArray{p.x, p.y});
        out["background_points"] = points;
    }
    if (!c.background_filters.empty()) {
        QJsonArray filters;
        for (auto f : c.background_filters)
            filters.append(QJsonObject{{"minimum", f.minimum},
                                       {"maximum", f.maximum},
                                       {"iterations", f.iterations},
                                       {"half_window", f.half_window}});
        out["background_filters"] = filters;
    }
    if (c.selection) {
        const auto &s = *c.selection;
        out["selection"] = QJsonObject{{"minimum", s.minimum},
                                       {"maximum", s.maximum},
                                       {"threshold", s.threshold_percent},
                                       {"maximum_peaks", s.maximum_peaks},
                                       {"append", s.append}};
    }
    if (c.background) {
        const auto &o = *c.background;
        out["background"] = QJsonObject{{"method", int(o.method)},
                                        {"automatic", o.automatic},
                                        {"coefficients", o.coefficients},
                                        {"iterations", o.iterations},
                                        {"half_window", o.half_window}};
        if (o.bruckner_range) {
            auto object = out["background"].toObject();
            object["minimum"] = o.bruckner_range->minimum.value;
            object["maximum"] = o.bruckner_range->maximum.value;
            out["background"] = object;
        }
    }
    if (c.smoothing) {
        const auto &o = *c.smoothing;
        out["smoothing"] = QJsonObject{{"method", int(o.method)},
                                       {"half_window", o.half_window},
                                       {"degree", o.polynomial_degree},
                                       {"derivative", o.derivative}};
    }
    if (c.peaks) {
        const auto &o = *c.peaks;
        out["peaks"] =
            QJsonObject{{"threshold", o.threshold_percent}, {"slope", o.slope},
                        {"range", o.search_range},          {"half_window", o.half_window},
                        {"degree", o.polynomial_degree},    {"maximum", o.maximum_peaks},
                        {"minimum", o.minimum_peaks},       {"automatic_window", o.automatic_window}};
        if (o.detection_range) {
            auto object = out["peaks"].toObject();
            object["detection_minimum"] = o.detection_range->minimum.value;
            object["detection_maximum"] = o.detection_range->maximum.value;
            out["peaks"] = object;
        }
    }
    return out;
}
}
