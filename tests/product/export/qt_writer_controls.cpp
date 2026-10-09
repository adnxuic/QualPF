#include "infrastructure/io/atomic_export_target.h"
#include "qualpf/application/export/atomic_export_writer.h"
#include "gui/export/session_export_adapter.h"
#include "qualpf/domain/radiation.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace qualpf;
using qualpf::infrastructure::exporting::write_pattern_export_atomic;
using namespace qualpf::application::exporting; using namespace qualpf::gui::exporting;
class FakeTarget final : public AtomicExportTarget {
  public:
    int failure = 0, opens = 0, writes = 0, commits = 0, cancels = 0;
    std::string target = "original", temporary;
    bool open() override {
        ++opens;
        return failure != 1;
    }
    std::int64_t write(std::string_view bytes) override {
        ++writes;
        temporary = bytes;
        return failure == 2   ? static_cast<std::int64_t>(bytes.size()) - 1
               : failure == 4 ? -1
                              : static_cast<std::int64_t>(bytes.size());
    }
    bool commit() override {
        ++commits;
        if (failure == 3)
            return false;
        target = temporary;
        return true;
    }
    void cancel() noexcept override {
        ++cancels;
        temporary.clear();
    }
    std::string error_message() const override { return "injected I/O failure"; }
};
void check(bool value, const char *text) {
    if (!value)
        throw std::runtime_error(text);
}
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        gui::SessionStateView state;
        state.pattern.two_theta = {TwoThetaDegrees{10}, TwoThetaDegrees{20}};
        state.pattern.observed = {1, 2};
        state.pattern.radiation.lines = {{radiation::default_wavelength, 1}};
        state.projectMetadata["radtype"] = 1;
        auto snapshot = prepare_session_export(state, qualpf::exporting::PatternFormat::powder_cif);
        check(bool(snapshot) && snapshot.value().radiation_token == "neutron", "Actual session metadata adapter");
        state.projectMetadata["radtype"] = "uninterpreted string";
        auto coerced = prepare_session_export(state, qualpf::exporting::PatternFormat::powder_cif);
        check(bool(coerced) && coerced.value().radiation_token == "'Cu K\\a~1~'",
              "Qt toInt missing/string default preserved");
        const auto fmt = qualpf::exporting::PatternFormat::powder_cif;
        const auto payload = qualpf::exporting::format_pattern(snapshot.value(), fmt).value();
        for (int failure = 1; failure <= 4; ++failure) {
            FakeTarget fake;
            fake.failure = failure;
            auto result = write_pattern_export_to(fake, snapshot.value(), fmt);
            const auto expected = failure == 1   ? "path_or_open_failure"
                                  : failure == 3 ? "commit_failure"
                                                 : "write_failure";
            check(!result && result.error().reason == expected && fake.target == "original",
                  "Structured atomic I/O failure");
            check(failure == 1 ? fake.writes == 0 && fake.commits == 0 : fake.cancels == 1,
                  "Failed write/commit cancels transaction");
        }
        FakeTarget success;
        check(bool(write_pattern_export_to(success, snapshot.value(), fmt)) && success.target == payload,
              "Complete bytes committed once");
        auto invalid = snapshot.value();
        invalid.observed.clear();
        FakeTarget untouched;
        check(!write_pattern_export_to(untouched, invalid, fmt) && untouched.opens == 0,
              "Formatting failure performs no I/O");
        QTemporaryDir directory(QDir::currentPath() + "/phase24-writer-XXXXXX");
        check(directory.isValid(), "Writer test directory");
        const auto path = directory.path() + QString::fromUtf8("/瀹為獙 overwrite.cif");
        {
            QFile file(path);
            check(file.open(QIODevice::WriteOnly) && file.write("original") == 8, "Original target setup");
        }
        for (int boundary = 0; boundary != 3; ++boundary) {
            auto bad = snapshot.value();
            if (boundary == 0)
                bad.d_spacing_angstrom[0] = std::numeric_limits<double>::infinity();
            if (boundary == 1)
                bad.d_spacing_angstrom[0] = -1;
            if (boundary == 2)
                bad.wavelength_angstrom = 0;
            check(!write_pattern_export_atomic(path, bad, fmt), "B2 actual QSaveFile rejection");
            QFile retained(path);
            check(retained.open(QIODevice::ReadOnly) && retained.readAll() == "original",
                  "B2 actual original target retained");
        }
        check(!write_pattern_export_atomic(path, invalid, fmt), "Actual writer format rejection");
        {
            QFile file(path);
            check(file.open(QIODevice::ReadOnly) && file.readAll() == "original",
                  "Failed actual export preserves original file");
        }
        check(bool(write_pattern_export_atomic(path, snapshot.value(), fmt)),
              "Actual QSaveFile atomic overwrite with Unicode path");
        {
            QFile file(path);
            check(file.open(QIODevice::ReadOnly) && file.readAll() == QByteArray::fromStdString(payload),
                  "Actual writer exact bytes");
        }
        auto missing = write_pattern_export_atomic(directory.path() + "/missing/target.cif", snapshot.value(), fmt);
        check(!missing && missing.error().reason == "path_or_open_failure", "Actual path failure");
        auto cancel = write_pattern_export_atomic({}, snapshot.value(), fmt);
        check(!cancel && cancel.error().code == ErrorCode::cancelled, "Empty dialog target cancellation");
        auto nul = write_pattern_export_atomic(path + QChar('\0') + "suffix", snapshot.value(), fmt);
        check(!nul && nul.error().reason == "invalid_path", "Embedded NUL path rejected");
        std::cout << "PASS canonical session adapter, injected open/short-write/negative-write/commit failures, "
                     "cancellation, atomic rejection/overwrite, Unicode path\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
