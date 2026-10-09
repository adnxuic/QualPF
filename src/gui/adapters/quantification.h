#pragma once
#include "qualpf/application/presentation/quantification.h"
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
} // namespace qualpf::gui
