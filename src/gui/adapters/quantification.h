#pragma once
#include "qualpf/application/presentation/quantification.h"
#include "qualpf/application/product_session.h"
#include <QString>
namespace qualpf::gui {
inline std::vector<double> quantitativePercentages(const std::vector<application::ProductCard> &cards) {
    std::vector<application::QuantificationInput> inputs;
    inputs.reserve(cards.size());
    for (const auto &card : cards) {
        bool ok = false;
        const double rir = QString::fromStdString(card.rir).trimmed().toDouble(&ok);
        inputs.push_back(
            {card.fomCalculated, card.intensity, card.scale, ok ? std::optional<double>(rir) : std::nullopt});
    }
    return application::quantify(inputs);
}
inline application::QuantificationResult prepareQuantification(application::ProductHandle source) {
    if (!source || source->pfsm) return application::prepareQuantification(std::move(source));
    std::vector<application::QuantificationInput> inputs;
    for (const auto &card : source->phases) {
        bool ok = false;
        const double rir = QString::fromStdString(card.rir).trimmed().toDouble(&ok);
        inputs.push_back({card.fomCalculated, card.intensity, card.scale, ok ? std::optional<double>(rir) : std::nullopt});
    }
    return application::prepareQuantification(std::move(source), inputs);
}
inline std::vector<double> quantitativePercentages(application::ProductHandle source) {
    return qualpf::gui::prepareQuantification(std::move(source)).percentages();
}
} // namespace qualpf::gui
