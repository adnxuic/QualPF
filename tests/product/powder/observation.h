#pragma once
#include "gui/state/search_session.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <bit>
#include <stdexcept>

namespace phase25b {
inline void check(bool value, const char *reason) {
    if (!value) throw std::runtime_error(reason);
}
inline QJsonArray bits(const std::vector<double> &values) {
    QJsonArray out;
    for (double value : values)
        out.append(QString::number(std::bit_cast<std::uint64_t>(value),16).rightJustified(16,'0'));
    return out;
}
inline QJsonObject pattern(const qualpf::Pattern &p) {
    std::vector<double> x, corrected;
    for (auto value : p.two_theta) x.push_back(value.value);
    if (p.corrected_two_theta) for (auto value : *p.corrected_two_theta) corrected.push_back(value.value);
    auto flag=[](std::optional<bool> value)->QJsonValue { return value ? QJsonValue(*value) : QJsonValue(); };
    QJsonArray waves;
    for (auto line : p.radiation.lines)
        waves.append(QJsonObject{{"wavelength_bits",bits({line.wavelength.value})},{"weight_bits",bits({line.relative_weight})}});
    QJsonObject out{{"x",bits(x)},{"x0",bits(corrected)},{"y",bits(p.observed)},
        {"raw_y",p.original_observed ? bits(*p.original_observed) : QJsonArray{}},{"waves",waves},
        {"source",QString::fromStdString(p.source_id)},{"id",double(p.id.value)},
        {"source_precision",int(p.source_precision)},{"intensity_scale",int(p.intensity_scale)},
        {"radtype",p.radiation.kind ? int(*p.radiation.kind) : -1},
        {"scaled",flag(p.processing.intensity_scaled)},{"subtracted",flag(p.processing.background_subtracted)},
        {"alpha2",flag(p.processing.kalpha2_subtracted)},{"raw_alpha2",flag(p.original_kalpha2_subtracted)},
        {"background",bool(p.background)},{"calculated",bool(p.calculated)},
        {"net",bool(p.background_subtracted_signal)},{"weights",bool(p.weights)}};
    if (p.calculation_range) out["range"]=bits({p.calculation_range->minimum.value,p.calculation_range->maximum.value});
    return out;
}
inline void save(const QString &path,const QJsonObject &value) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Exclusive output");
    const auto bytes=QJsonDocument(value).toJson();
    check(file.write(bytes)==bytes.size(),"Complete observation");
}
inline void seed(qualpf::gui::SessionView &session) {
    qualpf::Pattern p;
    p.id={1}; p.source_id="seed";
    for (int i=0;i<32;++i) { p.two_theta.emplace_back(10.+i*.1); p.observed.push_back(20.+i); }
    p.radiation.lines={{qualpf::WavelengthAngstrom{1.2},1}};
    p.original_observed=p.observed;
    p.calculation_range=qualpf::AngularRange{p.two_theta.front(),p.two_theta.back()};
    check(bool(session.openPattern(session.snapshot(),std::move(p))),"Seed pattern");
    auto next=*session.snapshot();
    next.peaks.valid=true; next.peaks.wave=1.2;
    next.peaks.tth={11}; next.peaks.d={6}; next.peaks.deltaD={.1};
    next.peaks.intensity={50}; next.peaks.intensityOrig={50}; next.peaks.fwhm={.1};
    CardType card; card.setId("previous");
    next.candidates={card}; next.phases={card};
    next.preprocessing.smoothing_applied=true;
    check(bool(session.commit(session.snapshot(),std::move(next))),"Seed retained state");
}
}
