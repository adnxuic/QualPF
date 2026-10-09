#include "infrastructure/io/atomic_export_target.h"
#include "fixture.h"
#include "gui/export/pattern_export_service.h"
#include "gui/export/session_export_adapter.h"
#include <QCoreApplication>
#include <iostream>

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    qualpf::infrastructure::exporting::QtAtomicExportFactory factory;
    try {
        if (argc != 4)
            throw std::runtime_error("input, atomic target, DTO paths required");
        std::ifstream in(argv[1], std::ios::binary);
        qualpf::exporting::PatternFormat format;
        const auto source = std::make_shared<const qualpf::gui::SessionStateView>(phase24b::load(in, format));
        const qualpf::gui::exporting::PatternExportService service(source,factory);
        const auto result = service.save(QString::fromLocal8Bit(argv[2]), format);
        if (!result) {
            std::cout << "REJECT " << result.error().field << ' ' << result.error().reason << '\n';
            return 2;
        }
        // Read-only DTO observation uses the exact production builder. File bytes
        // above were produced through the service and real atomic writer.
        const auto snapshot = qualpf::gui::exporting::prepare_session_export(*source, format);
        if (!snapshot)
            throw std::runtime_error("Observation disagrees with service");
        std::ofstream dto(argv[3], std::ios::binary);
        const auto &s = snapshot.value();
        phase24b::dump(dto, s.x);
        phase24b::dump(dto, s.observed);
        phase24b::dump(dto, s.d_spacing_angstrom);
        phase24b::dump(dto, s.background.value_or(std::vector<double>{}));
        phase24b::dump(dto, s.net.value_or(std::vector<double>{}));
        if (!dto)
            throw std::runtime_error("DTO observation write failed");
        std::cout << "PASS application service " << s.x.size() << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
