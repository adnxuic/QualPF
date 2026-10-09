#include "gui/model/cardtype.h"
#include "infrastructure/serialization/metadata_values.h"
#include "infrastructure/serialization/project_codec.h"
#include "infrastructure/serialization/quantification.h"
#include "qualpf/domain/radiation.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QTemporaryDir>
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace qualpf;
using namespace qualpf::application;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
std::vector<double> originalQuant(const std::vector<ProductCard> &phases) {
    QVector<double> rap(static_cast<int>(phases.size()), 0.0);
    bool valid = true;
    for (int i = 0; i < rap.size(); ++i) {
        if (!phases[i].fomCalculated) {
            valid = false;
            break;
        }
        bool ok;
        const double rir = QString::fromStdString(phases[i].rir).trimmed().toDouble(&ok);
        if (!ok || rir <= 0.0) {
            valid = false;
            break;
        }
        double maxI = 0.0;
        for (double value : phases[i].intensity)
            maxI = qMax(maxI, value * phases[i].scale);
        rap[i] = maxI / rir;
    }
    double sum = 0.0;
    for (double r : rap)
        sum += r;
    QVector<double> quant(rap.size(), 0.0);
    if (valid && sum > 0.0)
        for (int i = 0; i < rap.size(); ++i)
            quant[i] = 100.0 * rap[i] / sum;
    return valid && sum > 0.0 ? std::vector<double>(quant.begin(), quant.end()) : std::vector<double>{};
}
void checkQuant() {
    ProductCard first;
    first.fomCalculated = true;
    first.scale = 0.713576;
    first.intensity = {2, 100.125, 6};
    first.rir = " 2.345 ";
    ProductCard second = first;
    second.scale = 3.417;
    second.rir = "1.75";
    for (const auto &rir : {"1.75", "0", "-1", "invalid", "nan", "inf", "  2.5  "})
        for (double scale : {0.0, -2.0, 0.7912, std::numeric_limits<double>::infinity()})
            for (bool calculated : {false, true}) {
                second.rir = rir;
                second.scale = scale;
                second.fomCalculated = calculated;
                const std::vector<ProductCard> cards{first, second};
                const auto expected = originalQuant(cards),
                           actual = infrastructure::quantitativePercentages(cards);
                require(expected.size() == actual.size(), "Quantification validity changed");
                for (std::size_t i = 0; i < actual.size(); ++i)
                    require(std::bit_cast<std::uint64_t>(expected[i]) ==
                                std::bit_cast<std::uint64_t>(actual[i]),
                            "Quantification value changed");
            }
    require(infrastructure::quantitativePercentages({}).empty(), "Empty quantification changed");
}
struct FailedProject : ProjectSourceReader {
    Result<ProductState> read(const std::string &, SearchBackend, const MetadataObject &) const override {
        return Result<ProductState>{Error{ErrorCode::missing_data, "project.file", "controlled failure"}};
    }
};
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        checkQuant();
        ProductState state;
        state.projectEnvelope["future"] =
            MetadataObject{{"wide", std::int64_t{9007199254740993LL}},
                           {"null", nullptr},
                           {"array", MetadataArray{std::int64_t{3}, 3.5, "value"}}};
        state.projectMetadata["unknown"] =
            MetadataObject{{"integer", std::int64_t{-9007199254740993LL}}, {"null", nullptr}};
        state.pattern.id = {1};
        state.pattern.source_id = "owned-pattern";
        state.pattern.two_theta = {TwoThetaDegrees{20}, TwoThetaDegrees{40}};
        state.pattern.observed = {12, 24};
        state.pattern.radiation.lines = {{radiation::default_wavelength, 1}};
        state.pattern.calculation_range = AngularRange{TwoThetaDegrees{20}, TwoThetaDegrees{40}};
        ProductCard card;
        card.id = "7";
        card.d = {4, 2};
        card.intensity = {100, 50};
        card.scale = .7912;
        state.candidates = {card};
        const auto source = std::make_shared<const ProductState>(state);
        auto encoded = infrastructure::encodeProject(source, {}, "frozen-clock");
        require(bool(encoded), "Canonical project encoding failed");
        auto decoded = infrastructure::decodeProject(encoded.value(), SearchBackend::cpp, {});
        require(bool(decoded), "Project round-trip failed");
        require(decoded.value().projectEnvelope == state.projectEnvelope,
                "Unknown envelope lost integer/null/array types");
        require(decoded.value().projectMetadata.at("unknown") == state.projectMetadata.at("unknown"),
                "Unknown diffraction metadata changed");
        CardType original;
        original.setD({4, 2}, radiation::default_wavelength_angstrom);
        const auto angles = original.getTth();
        require(decoded.value().candidates[0].tth == std::vector<double>(angles.begin(), angles.end()),
                "Project card coordinates changed");
        require(decoded.value().candidates[0].scale == card.scale &&
                    !decoded.value().candidates[0].fomCalculated,
                "Unscored previous scale changed");
        auto invalid = encoded.value();
        auto dd = invalid["diffraction_data"].toObject();
        dd["y"] = QJsonArray{1};
        invalid["diffraction_data"] = dd;
        require(!infrastructure::decodeProject(invalid, SearchBackend::cpp, {}),
                "Malformed project published state");
        QTemporaryDir directory;
        require(directory.isValid(), "Project fixture directory");
        const auto filename = directory.path() + QString::fromUtf8("/项目.qxp");
        require(bool(infrastructure::writeProject(filename, encoded.value())),
                "Atomic project file write failed");
        auto read = infrastructure::readProject(filename, SearchBackend::cpp, {});
        require(bool(read) && read.value().projectEnvelope == state.projectEnvelope,
                "Project file IO changed metadata");
        ProductSearchSession session;
        const auto before = session.snapshot();
        require(!openProjectInput(session, FailedProject{}, "missing", {}) && session.snapshot() == before,
                "Failed project IO committed");
        require(
            bool(openProjectInput(session, infrastructure::ProjectFileReader{}, filename.toStdString(), {})),
            "Project reader port commit failed");
        auto v1 = encoded.value();
        v1["version"] = 1;
        auto old = infrastructure::decodeProject(v1, SearchBackend::cpp, {});
        require(bool(old), "Historical project rejected");
        std::cout << "PASS: project values, unknown metadata, file ports, card angles and independent "
                     "quantification parity\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
