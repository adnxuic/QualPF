#pragma once
#include <cstddef>
#include <optional>
class MainWindow;
namespace qualpf::gui {
bool editManualPeak(MainWindow &, std::optional<std::size_t> row = {}, std::optional<double> position = {}, bool table = false);
void recoverPreprocessing(MainWindow &);
} // namespace qualpf::gui
