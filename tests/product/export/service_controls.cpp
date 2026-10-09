#include "infrastructure/io/atomic_export_target.h"
#define main approved_qt_writer_controls
#include "qt_writer_controls.cpp"
#undef main
#include "gui/export/pattern_export_service.h"

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    qualpf::infrastructure::exporting::QtAtomicExportFactory factory;
    try {
        using qualpf::exporting::PatternFormat;
        gui::SessionStateView state;
        state.pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{20}};
        state.pattern.observed = {1, 2};
        state.pattern.radiation.lines = {{radiation::default_wavelength, 1}};
        const auto source = std::make_shared<const gui::SessionStateView>(state);
        PatternExportService service(source,factory);
        FakeTarget success;
        check(bool(service.saveTo(success, PatternFormat::powder_cif)) && success.opens == 1 &&
                  success.writes == 1 && success.commits == 1 && success.cancels == 0,
              "Application commits complete output once");
        for (int failure = 1; failure <= 4; ++failure) {
            FakeTarget file;
            file.failure = failure;
            const auto result = service.saveTo(file, PatternFormat::powder_cif);
            const auto reason = failure == 1 ? "path_or_open_failure" : failure == 3 ? "commit_failure" : "write_failure";
            check(!result && result.error().reason == reason && file.target == "original", "Application atomic fault");
            check(failure == 1 ? file.writes == 0 && file.commits == 0 : file.cancels == 1,
                  "Application cancels failed write/commit");
        }
        QTemporaryDir dir(QDir::currentPath() + "/phase24b-service-XXXXXX");
        check(dir.isValid(), "Application test directory");
        const auto path = dir.path() + QString::fromUtf8("/原目标.cif");
        for (int boundary = 0; boundary != 3; ++boundary) {
            auto bad = state;
            if (boundary == 0)
                bad.pattern.two_theta[0] = TwoThetaDegrees{0};
            if (boundary == 1)
                bad.pattern.two_theta[0] = TwoThetaDegrees{-1};
            if (boundary == 2)
                bad.pattern.radiation.lines[0].wavelength = WavelengthAngstrom{1e-300};
            const auto rejected = std::make_shared<const gui::SessionStateView>(bad);
            PatternExportService guard(rejected,factory);
            FakeTarget file;
            const auto result = guard.saveTo(file, PatternFormat::powder_cif);
            const auto reason = boundary == 0 ? "nonfinite_d_spacing" : boundary == 1 ? "nonpositive_d_spacing" : "invalid_wavelength";
            check(!result && result.error().code == ErrorCode::invalid_input && result.error().reason == reason &&
                      file.opens == 0 && file.writes == 0 && file.commits == 0 && file.target == "original",
                  "B2 source rejects through Application before target open");
            {
                QFile original(path);
                check(original.open(QIODevice::WriteOnly) && original.write("original") == 8, "Old target");
            }
            check(!guard.save(path, PatternFormat::powder_cif), "B2 actual Application rejection");
            QFile original(path);
            check(original.open(QIODevice::ReadOnly) && original.readAll() == "original", "B2 old target retained");
            check(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size() == 1,
                  "No partial temporary target after B2 rejection");
        }
        check(bool(service.save(path, PatternFormat::powder_cif)), "Application Unicode atomic overwrite");
        QFile actual(path);
        check(actual.open(QIODevice::ReadOnly) && actual.readAll() == QByteArray::fromStdString(success.target),
              "Application actual and port bytes identical");
        const auto cancelled = service.save({}, PatternFormat::powder_cif);
        check(!cancelled && cancelled.error().code == ErrorCode::cancelled, "Application cancellation");
        const auto nul = service.save(path + QChar('\0') + "suffix", PatternFormat::powder_cif);
        check(!nul && nul.error().reason == "invalid_path", "Application invalid target");
        const auto missing = service.save(dir.path() + "/missing/target.cif", PatternFormat::powder_cif);
        check(!missing && missing.error().reason == "path_or_open_failure", "Application missing destination");
        // Export never publishes state; the source's numerical arrays stay exact.
        check(source->pattern.observed == state.pattern.observed && source->pattern.two_theta == state.pattern.two_theta &&
                  source->revision == state.revision && source->editing == state.editing,
              "Application read-only source");
        auto editing = state;
        editing.editing = true;
        const PatternExportService privateEdit(std::make_shared<const gui::SessionStateView>(editing),factory);
        FakeTarget guarded;
        const auto privateCif = privateEdit.saveTo(guarded, PatternFormat::powder_cif);
        check(!privateCif && privateCif.error().reason == "private_editor_active" && guarded.opens == 0,
              "CIF private editor guard");
        FakeTarget xy;
        check(bool(privateEdit.saveTo(xy, PatternFormat::xy_observed)), "XY reads committed canonical data during edit");
        std::cout << "PASS Application source B2 errors, no I/O on rejection, atomic faults, Unicode overwrite, "
                     "cancel/missing destination, source integrity, private edit contract\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
