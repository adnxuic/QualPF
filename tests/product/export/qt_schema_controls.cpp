#include "gui/export/session_export_adapter.h"
#include "qualpf/domain/radiation.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    using namespace qualpf;
    try {
        gui::SessionStateView state;
        state.pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{20}};
        state.pattern.observed = {1, 2};
        state.pattern.background = SampledSignal{{.5, 1.25}, SampleWindow{0}};
        state.pattern.radiation.lines = {{radiation::default_wavelength, 1}};
        unsigned checks = 0;
        for (bool points : {false, true})
            for (bool method : {false, true}) {
                state.preprocessing = {};
                state.preprocessing.background = preprocessing::BackgroundOptions{};
                state.preprocessing.background->method =
                    method ? preprocessing::BackgroundMethod::chebyshev : preprocessing::BackgroundMethod::none;
                if (points)
                    state.preprocessing.background_points =
                        std::vector<preprocessing::BackgroundPoint>{{10, .5}, {20, 1.25}};
                auto result =
                    gui::exporting::prepare_session_export(state, qualpf::exporting::PatternFormat::powder_cif);
                if (!result || result.value().powder_cif_schema !=
                                   (points && method ? exporting::PowderCifSchema::observed_net_background
                                                     : exporting::PowderCifSchema::observed))
                    throw std::runtime_error("Canonical configuration schema predicate mismatch");
                ++checks;
            }
        state.preprocessing.background.reset();
        auto default_method =
            gui::exporting::prepare_session_export(state, qualpf::exporting::PatternFormat::powder_cif);
        if (!default_method ||
            default_method.value().powder_cif_schema != exporting::PowderCifSchema::observed_net_background)
            throw std::runtime_error("Absent method must preserve native new-dataset default");
        state.preprocessing.background_points->clear();
        auto empty = gui::exporting::prepare_session_export(state, qualpf::exporting::PatternFormat::powder_cif);
        if (!empty || empty.value().powder_cif_schema != exporting::PowderCifSchema::observed)
            throw std::runtime_error("Allocated empty points must not enable schema");
        std::cout << "PASS " << checks << " actual canonical schema truth-table controls plus default/empty metadata\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
