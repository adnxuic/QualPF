#pragma once
#include "qualpf/application/product_values.h"
#include <span>
namespace qualpf::application {
struct QuantificationInput {
    bool calculated;
    std::span<const double> intensity;
    double scale;
    std::optional<double> rir;
};
std::vector<double> quantify(std::span<const QuantificationInput>);
} // namespace qualpf::application
