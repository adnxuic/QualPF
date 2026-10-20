#include "project_codec.h"
#include "project_v3.h"
#include "core/io/powder_cif/binary32.h"
#include "infrastructure/io/project_peaks.h"
#include "infrastructure/serialization/metadata_values.h"
#include "infrastructure/serialization/powder_cif_provenance.h"
#include "infrastructure/serialization/preprocessing_config.h"
#include "infrastructure/serialization/workflow_settings.h"
#include "infrastructure/serialization/quantification.h"
#include "qualpf/application/search_engine.h"
#include "qualpf/domain/radiation.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <bit>
#include <cmath>
namespace qualpf::infrastructure::legacy_fixture {
namespace {
std::vector<double> column(const QJsonArray &array) {
    std::vector<double> values;
    values.reserve(array.size());
    for (const auto &value : array)
        values.push_back(value.toDouble());
    return values;
}
application::ProductCard cardFromJson(const QJsonObject &o, double wave) {
    application::ProductCard c;
    c.id = o["id"].toString().toStdString();
    c.chemicalName = o["name"].toString().toStdString();
    c.mineralName = o["mineral"].toString().toStdString();
    c.chemicalFormula = o["formula"].toString().toStdString();
    c.quality = o["quality"].toString().toStdString();
    c.rir = o["rir"].toString().toStdString();
    c.spaceGroup = o["spg"].toString().toStdString();
    c.d = column(o["d"].toArray());
    c.intensity = column(o["intensity"].toArray());
    if (wave > 0.0)
        for (double d : c.d) {
            const auto angle =
                io::powder_cif::binary32::certifiedAsin64(std::bit_cast<std::uint64_t>(0.5 * wave / d));
            const double tth = 2.0 * std::bit_cast<double>(angle) * (180 / 3.14159265358979323846);
            if (!std::isnan(tth))
                c.tth.push_back(tth);
        }
    c.scale = o["scale"].toDouble(1.0);
    if (o["fom_calculated"].toBool()) {
        c.fomCalculated = true;
        c.fom = o["fom"].toDouble();
        c.fomPeakPos = o["fom_peakpos"].toDouble();
        c.fomIntensity = o["fom_intensity"].toDouble();
        c.scale = o["scale"].toDouble();
    }
    return c;
}
} // namespace
Result<application::ProductState> decodeProject(const QJsonObject &root, application::SearchBackend backend,
                                                const application::MetadataObject &defaults) {
    const auto failed = [](std::string message) {
        return Result<application::ProductState>{
            Error{ErrorCode::invalid_input, "project", std::move(message), "invalid_project"}};
    };
    if (root["version"] == QJsonValue(3)) return decodeProjectV3(root,backend);
    QString arrayError;
    if (!validate_project_arrays(root, arrayError))
        return Result<application::ProductState>{
            Error{ErrorCode::invalid_input, "project", arrayError.toStdString(), "project_warning"}};
    const int version = root["version"].toInt(-1);
    if (version != 1 && version != 2) {
        return Result<application::ProductState>{
            Error{ErrorCode::unsupported, "project", "Unsupported project version.", "project_warning"}};
    }
    application::ProductPeaks restoredPeaks;
    if (root.contains("peaks")) {
        QString error;
        auto decoded = decode_project_peaks(root["peaks"].toObject(), version, error);
        if (!decoded) {
            return Result<application::ProductState>{
                Error{ErrorCode::invalid_input, "project.peaks", error.toStdString(), "project_warning"}};
        }
        restoredPeaks = std::move(*decoded);
    }

    application::ProductState next;
    next.backend = backend;
    next.peaks = std::move(restoredPeaks);
    if (root.contains("preprocessing") && !root["preprocessing"].isObject()) {
        return failed("Invalid preprocessing configuration.");
    }
    auto preprocessing = qualpf::infrastructure::decodePreprocessing(root["preprocessing"].toObject());
    if (!preprocessing) {
        return Result<application::ProductState>{preprocessing.error()};
    }
    next.preprocessing = std::move(preprocessing).value();
    const double wave =
        next.peaks.wave > 0 ? next.peaks.wave : qualpf::radiation::default_wavelength_angstrom;
    const auto dd = root["diffraction_data"].toObject();
    QJsonObject projectMetadata = dd;
    for (const char *key : {"x", "y", "wave", "ratio"})
        projectMetadata.remove(key);
    projectMetadata["residual_current_known"] =
        version == 2 && dd.value("residual_current_known").toBool(true);
    QJsonObject searchOptions = root["search_options"].toObject();
    if (searchOptions.isEmpty())
        searchOptions = jsonValue(application::MetadataValue(defaults)).toObject();
    QJsonObject restraints = root["restraints"].toObject();
    if (root.contains("automatic_delta"))
        next.automaticDelta = root["automatic_delta"].toDouble();
    if (root.contains("peak_tolerance"))
        next.peakTolerance = root["peak_tolerance"].toDouble();
    auto &pattern = next.pattern;
    pattern.id = {1};
    pattern.source_id = dd["filename"].toString().toStdString();
    pattern.source_precision = qualpf::SourcePrecision::float32_abi;
    if (dd.contains("radtype")) {
        const auto kind = dd["radtype"].toInt(-1);
        if (kind >= 0 && kind <= 2)
            pattern.radiation.kind = qualpf::RadiationKind(kind);
    }
    for (const auto key : {"source_precision", "intensity_scale"}) {
        if (!dd.contains(key))
            continue;
        const auto value = dd[key];
        if (!value.isDouble() || value.toDouble() != value.toInt(-1) || value.toInt() < 0 ||
            value.toInt() > 2) {
            return failed("Invalid pattern precision/scale metadata.");
        }
        if (QString(key) == "source_precision")
            pattern.source_precision = qualpf::SourcePrecision(value.toInt());
        else
            pattern.intensity_scale = qualpf::IntensityScale(value.toInt());
    }
    if (dd.contains("corrected_two_theta")) {
        if (!dd["corrected_two_theta"].isArray()) {
            return failed("Invalid corrected grid.");
        }
        pattern.corrected_two_theta.emplace();
        for (const auto v : dd["corrected_two_theta"].toArray()) {
            if (!v.isDouble()) {
                return failed("Invalid corrected grid.");
            }
            pattern.corrected_two_theta->emplace_back(v.toDouble());
        }
    }
    // v1 background flags are absent: retain unknown metadata, do not invent
    // a subtraction history or reconstruct residual intensities from phases.
    for (auto v : dd["x"].toArray())
        pattern.two_theta.push_back(qualpf::TwoThetaDegrees{v.toDouble()});
    for (auto v : dd["y"].toArray())
        pattern.observed.push_back(v.toDouble());
    const auto waves = dd["wave"].toArray(), ratios = dd["ratio"].toArray();
    for (qsizetype i = 0; i < waves.size(); ++i)
        pattern.radiation.lines.push_back(
            {qualpf::WavelengthAngstrom{waves[i].toDouble()}, ratios[i].toDouble()});
    if (dd.contains("back_subtracted"))
        pattern.processing.background_subtracted = dd["back_subtracted"].toBool();
    if (dd.contains("kalpha2_subtracted"))
        pattern.processing.kalpha2_subtracted = dd["kalpha2_subtracted"].toBool();
    if (dd.contains("original_kalpha2_subtracted"))
        pattern.original_kalpha2_subtracted = dd["original_kalpha2_subtracted"].toBool();
    if (dd.contains("intensity_scaled")) {
        if (!dd["intensity_scaled"].isBool()) {
            return failed("Invalid scaling state.");
        }
        pattern.processing.intensity_scaled = dd["intensity_scaled"].toBool();
    }
    if (dd.contains("original_observed")) {
        if (!dd["original_observed"].isArray()) {
            return failed("Invalid original pattern.");
        }
        pattern.original_observed.emplace();
        for (auto v : dd["original_observed"].toArray()) {
            if (!v.isDouble() || !std::isfinite(v.toDouble())) {
                return failed("Invalid original pattern.");
            }
            pattern.original_observed->push_back(v.toDouble());
        }
    }
    if (dd["has_back"].toBool()) {
        pattern.background = qualpf::SampledSignal{};
        for (auto v : dd["yb"].toArray())
            pattern.background->values.push_back(v.toDouble());
        if (dd.contains("background_first")) {
            const auto first = dd["background_first"];
            if (!first.isDouble() || first.toDouble() < 0 ||
                first.toDouble() != std::floor(first.toDouble()) ||
                first.toDouble() > double(pattern.two_theta.size())) {
                return failed("Invalid background alignment.");
            }
            pattern.background->window = qualpf::SampleWindow{std::size_t(first.toDouble())};
        }
    }
    if (!pattern.two_theta.empty()) {
        // Existing QXP versions define their saved x/y extent as the matching
        // domain when no separate range is encoded (Phase 8/9.3 contract).
        // New files can explicitly preserve an unknown range without changing
        // that historical format default. This does not infer processing history.
        if (!dd.contains("calculation_range_known") || dd["calculation_range_known"].toBool())
            pattern.calculation_range =
                qualpf::AngularRange{pattern.two_theta.front(), pattern.two_theta.back()};
        if (dd.contains("tmin") && dd.contains("tmax"))
            pattern.calculation_range = qualpf::AngularRange{qualpf::TwoThetaDegrees{dd["tmin"].toDouble()},
                                                             qualpf::TwoThetaDegrees{dd["tmax"].toDouble()}};
    }
    for (auto val : root["results"].toArray())
        next.candidates.push_back(cardFromJson(val.toObject(), wave));
    for (auto val : root["phases"].toArray())
        next.phases.push_back(cardFromJson(val.toObject(), wave));
    auto powderProvenance =
        qualpf::infrastructure::decodePowderCifProvenance(dd["powder_cif_provenance"], pattern.source_id);
    if (!powderProvenance) {
        return Result<application::ProductState>{powderProvenance.error()};
    }
    next.powderCifProvenance = std::move(powderProvenance).value();
    next.projectMetadata = std::get<application::MetadataObject>(metadataValue(projectMetadata).data);
    next.searchOptions = std::get<application::MetadataObject>(metadataValue(searchOptions).data);
    if (root.contains("workflow_settings")) {
        if (!root["workflow_settings"].isObject()) return failed("Invalid workflow settings.");
        auto settings = decodeWorkflowSettings(root["workflow_settings"].toObject());
        if (!settings) return Result<application::ProductState>{settings.error()};
        next.projectSettings = std::move(settings).value();
    }
    next.restraints = std::get<application::MetadataObject>(metadataValue(restraints).data);
    QJsonObject envelope = root;
    for (const char *key : {"version", "automatic_delta", "peak_tolerance", "restraints", "search_options",
                            "preprocessing", "workflow_settings", "created", "diffraction_data", "peaks", "results", "phases"})
        envelope.remove(key);
    next.projectEnvelope = std::get<application::MetadataObject>(metadataValue(envelope).data);
    next.provenance.legacyUnverified = application::hasDerivedProjectData(next);
    auto valid = application::validateProductState(next);
    if (!valid) {
        return Result<application::ProductState>{valid.error()};
    }

    if (!root.contains("automatic_delta") && !next.peaks.tth.empty()) {
        auto peaks = application::productSearchPeaks(next.peaks);
        if (!peaks) {
            return Result<application::ProductState>{peaks.error()};
        }
        auto tolerance = qualpf::preprocessing::automatic_tolerance(peaks.value());
        if (!tolerance) {
            return Result<application::ProductState>{tolerance.error()};
        }
        next.automaticDelta = tolerance.value();
    }
    return Result<application::ProductState>{std::move(next)};
}
Result<QJsonObject> encodeLegacyProject(application::ProductHandle snapshot,
                                  const application::MetadataObject &defaults, const QString &created) {
    if (!snapshot) return Result<QJsonObject>{Error{ErrorCode::invalid_input, "project", "Missing project snapshot", "invalid_project"}};
    auto prepared = application::prepareProjectSerialization(snapshot, quantificationInputs(snapshot->phases));
    if (!prepared) return Result<QJsonObject>{prepared.error()};
    return encodeLegacyProject(prepared.value(), defaults, created);
}
Result<QJsonObject> encodeLegacyProject(const application::ProjectSerializationInput &prepared,
                                  const application::MetadataObject &defaults, const QString &created) {
    const auto &snapshot = prepared.source();
    const auto failed = [](std::string message) {
        return Result<QJsonObject>{
            Error{ErrorCode::invalid_input, "project", std::move(message), "invalid_project"}};
    };
    if (!snapshot)
        return failed("Missing project snapshot");
    auto legacyValid = validateLegacyProjectSaveSource(snapshot);
    if (!legacyValid) return Result<QJsonObject>{legacyValid.error()};
    // Helper: std::vector<double> → QJsonArray
    auto toJsonArray = [](const std::vector<double> &v) {
        QJsonArray a;
        for (double x : v)
            a.append(x);
        return a;
    };

    QJsonObject root = jsonValue(application::MetadataValue(snapshot->projectEnvelope)).toObject();
    root["version"] = 2;
    root["automatic_delta"] = snapshot->automaticDelta;
    if (snapshot->peakTolerance)
        root["peak_tolerance"] = *snapshot->peakTolerance;
    root["restraints"] = jsonValue(application::MetadataValue(snapshot->restraints));
    root["search_options"] = jsonValue(application::MetadataValue(snapshot->searchOptions));
    root["preprocessing"] = qualpf::infrastructure::encodePreprocessing(snapshot->preprocessing);
    if (snapshot->projectSettings) root["workflow_settings"] = encodeWorkflowSettings(*snapshot->projectSettings);
    if (root["search_options"].toObject().isEmpty())
        root["search_options"] = jsonValue(application::MetadataValue(defaults)).toObject();
    root["created"] = created;

    // ── Diffraction data (x, y, background, wavelengths) ────────────
    const auto &pattern = snapshot->pattern;
    if (!pattern.two_theta.empty()) {
        QJsonObject dd = jsonValue(application::MetadataValue(snapshot->projectMetadata)).toObject();
        QJsonArray x, y, wave, ratio;
        for (auto v : pattern.two_theta)
            x.append(v.value);
        for (auto v : pattern.observed)
            y.append(v);
        for (const auto &line : pattern.radiation.lines) {
            wave.append(line.wavelength.value);
            ratio.append(line.relative_weight);
        }
        dd["x"] = x;
        dd["y"] = y;
        dd["wave"] = wave;
        dd["ratio"] = ratio;
        dd["filename"] = QString::fromStdString(pattern.source_id);
        dd["source_precision"] = int(pattern.source_precision);
        dd["intensity_scale"] = int(pattern.intensity_scale);
        if (pattern.corrected_two_theta) {
            QJsonArray corrected;
            for (auto x : *pattern.corrected_two_theta)
                corrected.append(x.value);
            dd["corrected_two_theta"] = corrected;
        } else
            dd.remove("corrected_two_theta");
        if (pattern.processing.background_subtracted)
            dd["back_subtracted"] = *pattern.processing.background_subtracted;
        else
            dd.remove("back_subtracted");
        if (pattern.processing.kalpha2_subtracted)
            dd["kalpha2_subtracted"] = *pattern.processing.kalpha2_subtracted;
        else
            dd.remove("kalpha2_subtracted");
        if (pattern.original_kalpha2_subtracted)
            dd["original_kalpha2_subtracted"] = *pattern.original_kalpha2_subtracted;
        else
            dd.remove("original_kalpha2_subtracted");
        if (pattern.processing.intensity_scaled)
            dd["intensity_scaled"] = *pattern.processing.intensity_scaled;
        else
            dd.remove("intensity_scaled");
        if (pattern.original_observed) {
            QJsonArray original;
            for (auto v : *pattern.original_observed)
                original.append(v);
            dd["original_observed"] = original;
        } else
            dd.remove("original_observed");
        dd.remove("background_first");
        dd["calculation_range_known"] = pattern.calculation_range.has_value();
        if (pattern.calculation_range) {
            dd["tmin"] = pattern.calculation_range->minimum.value;
            dd["tmax"] = pattern.calculation_range->maximum.value;
        } else {
            dd.remove("tmin");
            dd.remove("tmax");
        }
        if (pattern.background) {
            dd["has_back"] = true;
            if (pattern.background->window)
                dd["background_first"] = double(pattern.background->window->first);
            QJsonArray background;
            for (auto v : pattern.background->values)
                background.append(v);
            dd["yb"] = background;
        } else if (dd.contains("has_back")) {
            dd["has_back"] = false;
            dd.remove("yb");
        }
        // Missing historical existence metadata stays unknown. Any opaque
        // historical payload is retained, but never used as an aligned curve.
        root["diffraction_data"] = dd;
    }

    // ── Experimental peaks ──────────────────────────────────────────
    const application::ProductPeaks &ep = snapshot->peaks;
    const auto savedPeaks = encode_project_peaks(ep);
    QString peakError;
    if (!decode_project_peaks(savedPeaks, 2, peakError)) {
        return failed(peakError.toStdString());
    }
    root["peaks"] = savedPeaks;

    // ── Search results ───────────────────────────────────────────────
    const std::vector<application::ProductCard> cards = snapshot->candidates;
    if (!cards.empty()) {
        QJsonArray arr;
        for (const application::ProductCard &c : cards) {
            QJsonObject o;
            o["id"] = QString::fromStdString(c.id);
            o["name"] = QString::fromStdString(c.chemicalName);
            o["mineral"] = QString::fromStdString(c.mineralName);
            o["formula"] = QString::fromStdString(c.chemicalFormula);
            o["quality"] = QString::fromStdString(c.quality);
            o["rir"] = QString::fromStdString(c.rir);
            o["spg"] = QString::fromStdString(c.spaceGroup);
            o["fom_calculated"] = c.fomCalculated;
            o["fom"] = c.fom;
            o["fom_peakpos"] = c.fomPeakPos;
            o["fom_intensity"] = c.fomIntensity;
            o["scale"] = c.scale;
            o["d"] = toJsonArray(c.d);
            o["intensity"] = toJsonArray(c.intensity);
            arr.append(o);
        }
        root["results"] = arr;
    }

    // ── Accepted phases ──────────────────────────────────────────────
    const std::vector<application::ProductCard> &phases = snapshot->phases;
    const auto &quant = prepared.percentages();
    if (!phases.empty()) {
        QJsonArray arr;
        for (int i = 0; i < phases.size(); ++i) {
            QJsonObject o;
            o["id"] = QString::fromStdString(phases[i].id);
            o["name"] = QString::fromStdString(phases[i].chemicalName);
            o["mineral"] = QString::fromStdString(phases[i].mineralName);
            o["formula"] = QString::fromStdString(phases[i].chemicalFormula);
            o["quality"] = QString::fromStdString(phases[i].quality);
            o["rir"] = QString::fromStdString(phases[i].rir);
            o["spg"] = QString::fromStdString(phases[i].spaceGroup);
            o["fom_calculated"] = phases[i].fomCalculated;
            o["fom"] = phases[i].fom;
            o["fom_peakpos"] = phases[i].fomPeakPos;
            o["fom_intensity"] = phases[i].fomIntensity;
            o["scale"] = phases[i].scale;
            o["d"] = toJsonArray(phases[i].d);
            o["intensity"] = toJsonArray(phases[i].intensity);
            o["weight_percent"] = (i < quant.size()) ? quant[i] : 0.0;
            arr.append(o);
        }
        root["phases"] = arr;
    }

    QString arrayError;
    if (!qualpf::infrastructure::validate_project_arrays(root, arrayError)) {
        return failed(arrayError.toStdString());
    }
    return Result<QJsonObject>{std::move(root)};
}
Result<QJsonObject> encodeProject(application::ProductHandle source, const application::MetadataObject &, const QString &created) {
    return encodeProjectV3(std::move(source),created);
}
Result<QJsonObject> encodeProject(const application::ProjectSerializationInput &prepared, const application::MetadataObject &, const QString &created) {
    return encodeProjectV3(prepared.source(),created);
}
Result<application::ProductState> readProject(const QString &path, application::SearchBackend backend,
                                              const application::MetadataObject &defaults) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return Result<application::ProductState>{
            Error{ErrorCode::missing_data, "project.file", path.toStdString(), "project_read_failed"}};
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (document.isNull())
        return Result<application::ProductState>{Error{ErrorCode::invalid_input, "project.json",
                                                       error.errorString().toStdString(),
                                                       "project_parse_failed"}};
    return decodeProject(document.object(), backend, defaults);
}
Status writeProject(const QString &path, const QJsonObject &root) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return Status{
            Error{ErrorCode::invalid_input, "project.file", path.toStdString(), "project_open_failed"}};
    const auto bytes = QJsonDocument(root).toJson();
    if (file.write(bytes) != bytes.size() || !file.commit())
        return Status{
            Error{ErrorCode::invalid_input, "project.file", path.toStdString(), "project_save_failed"}};
    return Status{std::monostate{}};
}
} // namespace qualpf::infrastructure
