#pragma once

#include <QVector>
#include <optional>
#include "qualpf/application/product_values.h"

struct ExperimentalPeaks {
    QVector<double> d;
    QVector<double> deltaD;
    QVector<double> tth;
    QVector<double> intensity;     // current (residual) intensity, modified by performResidualSearch
    QVector<double> intensityOrig; // original intensity at load time, never subtracted
    QVector<double> fwhm;
    std::optional<QVector<double>> integratedIntensity; // Absent in historical/native exports.
    double          wave  = 0.0;
    bool            valid = false;

    qualpf::application::ProductPeaks productValue() const {
        qualpf::application::ProductPeaks value;
        value.d.assign(d.begin(), d.end()); value.deltaD.assign(deltaD.begin(), deltaD.end());
        value.tth.assign(tth.begin(), tth.end()); value.intensity.assign(intensity.begin(), intensity.end());
        value.intensityOrig.assign(intensityOrig.begin(), intensityOrig.end()); value.fwhm.assign(fwhm.begin(), fwhm.end());
        if (integratedIntensity) value.integratedIntensity.emplace(integratedIntensity->begin(), integratedIntensity->end());
        value.wave = wave; value.valid = valid;
        return value;
    }
    static ExperimentalPeaks fromProductValue(const qualpf::application::ProductPeaks &value) {
        ExperimentalPeaks result;
        result.d = {value.d.begin(), value.d.end()}; result.deltaD = {value.deltaD.begin(), value.deltaD.end()};
        result.tth = {value.tth.begin(), value.tth.end()}; result.intensity = {value.intensity.begin(), value.intensity.end()};
        result.intensityOrig = {value.intensityOrig.begin(), value.intensityOrig.end()}; result.fwhm = {value.fwhm.begin(), value.fwhm.end()};
        if (value.integratedIntensity) result.integratedIntensity.emplace(value.integratedIntensity->begin(), value.integratedIntensity->end());
        result.wave = value.wave; result.valid = value.valid;
        return result;
    }

    void clear()
    {
        d.clear(); deltaD.clear(); tth.clear();
        intensity.clear(); intensityOrig.clear(); fwhm.clear();
        integratedIntensity.reset();
        wave = 0.0; valid = false;
    }
};
