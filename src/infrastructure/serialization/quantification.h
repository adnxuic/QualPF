#pragma once
#include "qualpf/application/presentation/quantification.h"
#include <QString>
namespace qualpf::infrastructure {
inline std::vector<application::QuantificationInput> quantificationInputs(const std::vector<application::ProductCard> &cards) {
    std::vector<application::QuantificationInput> inputs;
    inputs.reserve(cards.size());
    for (const auto &card : cards) {
        bool ok = false;
        const double rir = QString::fromStdString(card.rir).trimmed().toDouble(&ok);
        inputs.push_back(
            {card.fomCalculated, card.intensity, card.scale, ok ? std::optional<double>(rir) : std::nullopt});
    }
    return inputs;
}
// Explicit default compatibility facade, retained for independent parity tests.
inline std::vector<double> quantitativePercentages(const std::vector<application::ProductCard> &cards) {
    return application::quantify(quantificationInputs(cards));
}
} // namespace qualpf::infrastructure
