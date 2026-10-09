#include "qualpf/application/export/atomic_export_writer.h"
#include <iostream>
#include <stdexcept>

using namespace qualpf;
using namespace qualpf::application::exporting;
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
int main() {
    try {
        exporting::PatternExportSnapshot snapshot;
        snapshot.x = {10, 20};
        snapshot.observed = {1, 2};
        const auto fmt = qualpf::exporting::PatternFormat::xy_observed;
        const auto payload = exporting::format_pattern(snapshot, fmt).value();
        for (int failure = 1; failure <= 4; ++failure) {
            FakeTarget fake;
            fake.failure = failure;
            auto result = write_pattern_export_to(fake, snapshot, fmt);
            const auto expected = failure == 1   ? "path_or_open_failure"
                                  : failure == 3 ? "commit_failure"
                                                 : "write_failure";
            check(!result && result.error().reason == expected && fake.target == "original",
                  "Structured atomic I/O failure");
            check(failure == 1 ? fake.writes == 0 && fake.commits == 0 : fake.cancels == 1,
                  "Failed write/commit cancels transaction");
        }
        FakeTarget success;
        check(bool(write_pattern_export_to(success, snapshot, fmt)) && success.target == payload &&
                  success.opens == 1 && success.writes == 1 && success.commits == 1 && success.cancels == 0,
              "Complete bytes committed once");
        auto invalid = snapshot;
        invalid.observed.clear();
        FakeTarget untouched;
        check(!write_pattern_export_to(untouched, invalid, fmt) && untouched.opens == 0,
              "Formatting failure performs no I/O");
        std::cout << "PASS open/short-write/negative-write/commit fault injection, original target preservation, no "
                     "I/O on format rejection; Qt=0 Fortran=0\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
