#include "atomic_export_target.h"
#include <QSaveFile>
namespace qualpf::infrastructure::exporting {
using namespace application::exporting;
namespace {
class QtAtomicExportTarget final : public AtomicExportTarget {
  public:
    explicit QtAtomicExportTarget(const QString &target) : file_(target) {}
    bool open() override { return file_.open(QIODevice::WriteOnly); }
    std::int64_t write(std::string_view data) override {
        return file_.write(data.data(), static_cast<qint64>(data.size()));
    }
    bool commit() override { return file_.commit(); }
    void cancel() noexcept override {
        if (file_.isOpen())
            file_.cancelWriting();
    }
    std::string error_message() const override { return file_.errorString().toStdString(); }

  private:
    QSaveFile file_;
};
} // namespace
Status write_pattern_export_atomic(const QString &target,
                                   const qualpf::exporting::PatternExportSnapshot &snapshot,
                                   qualpf::exporting::PatternFormat format) {
    return application::exporting::writePatternExport(QtAtomicExportFactory{}, target.toStdString(), snapshot, format);
}
Result<std::unique_ptr<AtomicExportTarget>> QtAtomicExportFactory::create(const std::string &path) const {
    const auto target=QString::fromStdString(path);
    if (target.isEmpty())
        return Result<std::unique_ptr<AtomicExportTarget>>{Error{ErrorCode::cancelled, "export.target", "No export target", "empty_target"}};
    if (target.contains(QChar('\0')))
        return Result<std::unique_ptr<AtomicExportTarget>>{
            Error{ErrorCode::invalid_input, "export.target", "Embedded NUL in export path", "invalid_path"}};
    return Result<std::unique_ptr<AtomicExportTarget>>{std::make_unique<QtAtomicExportTarget>(target)};
}
} // namespace qualpf::infrastructure::exporting
