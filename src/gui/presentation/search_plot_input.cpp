#include "gui/presentation/search_plot_input.h"
#include "gui/state/preprocessing_product.h"
#include <QString>

namespace qualpf::gui::presentation {
using namespace application; using namespace qualpf::gui;
using namespace application::presentation;
std::string compatibility_plot_name(const std::string &source_id) {
    auto bytes = QString::fromStdString(source_id).toLocal8Bit().left(256);
    while (bytes.endsWith(' '))
        bytes.chop(1);
    bytes = bytes.mid(bytes.lastIndexOf('/') + 1);
    const auto dot = bytes.lastIndexOf('.');
    if (dot >= 0 && bytes.size() - dot - 1 <= 4)
        bytes.truncate(dot);
    return QString::fromUtf8(bytes).toStdString();
}
PlotInput make_plot_input(const SessionStateView &state, const PresentationWorkspace &workspace,
                          double card_display_wavelength) {
    PlotInput out;
    out.presentation = workspace.snapshot();
    out.mode = workspace.mode();
    auto add_pattern = [&](const Pattern &pattern) {
        PatternLayerInput input;
        input.pattern = pattern;
        input.count = pattern.two_theta.size();
        input.display_name = compatibility_plot_name(pattern.source_id);
        input.observed_visibility = out.presentation.observed;
        input.background_enabled = hasActiveBackground(pattern);
        // Presentation has no active curve for the subtracted zero placeholder;
        // the owning Pattern/QXP still retains the aligned transport unchanged.
        if (!input.background_enabled)
            input.pattern.background.reset();
        out.patterns.push_back(std::move(input));
    };
    if (!state.pattern.two_theta.empty())
        add_pattern(state.pattern);
    for (const auto &pattern : state.comparisonPatterns)
        add_pattern(pattern);
    if (!out.patterns.empty() && state.preprocessing.background_points)
        for (auto point : *state.preprocessing.background_points) {
            out.patterns[0].background_points_x.push_back(point.x);
            out.patterns[0].background_points_y.push_back(point.y);
        }
    out.peaks.x.assign(state.peaks.tth.begin(), state.peaks.tth.end());
    out.peaks.current_intensity.assign(state.peaks.intensity.begin(), state.peaks.intensity.end());
    out.peaks.wavelength = state.peaks.wave;
    for (const auto &card : state.phases) {
        CardOverlayInput input;
        input.id = card.getId().toStdString();
        input.chemical_name = card.getChemicalName().toStdString();
        input.mineral_name = card.getMineralName().toStdString();
        input.display_wavelength = card_display_wavelength;
        const auto tth = card.getTth();
        const auto d = card.getD();
        input.two_theta.assign(tth.begin(), tth.end());
        input.d_spacing.assign(d.begin(), d.end());
        out.cards.push_back(std::move(input));
    }
    return out;
}
} // namespace qualpf::gui::presentation
