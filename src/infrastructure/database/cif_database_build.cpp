#include "cif_database_build.h"
#include "qualpf/application/database/cif_card_record.h"
#include "infrastructure/database/qt_sql/cif_database_writer.h"
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <exception>

namespace qualpf::infrastructure {
using namespace application;
namespace {
CifBuildIssue issue(std::string stage, std::string code, const QString &source,
                    std::string field, std::string reason) {
  return {std::move(stage),
          std::move(code),
          source.toUtf8().toStdString(),
          {},
          std::move(field),
          0,
          0,
          std::move(reason),
          {},
          {}};
}
CifBuildIssue storage_issue(std::string stage,
                            const database::WriteError &error) {
  return {std::move(stage),
          error.code,
          error.source,
          error.block,
          error.field,
          0,
          0,
          error.reason,
          {},
          {}};
}
} // namespace

CifBuildReport build_cif_directory(const QString &database_base,
                                   const QString &directory,
                                   CifBuildOptions options,
                                   std::stop_token stop,
                                   CifBuildProgress progress) {
  CifBuildReport report;
  const auto cancelled = [&] {
    report.cancelled = stop.stop_requested();
    return report.cancelled;
  };
  if (cancelled())
    return report;
  const QFileInfo root(directory);
  if (!root.exists() || !root.isDir() || !root.isReadable()) {
    report.fatal =
        issue("discovery", "invalid_cif_directory", directory, "directory",
              "CIF directory does not exist or is not a readable directory");
    return report;
  }
  database::CifDatabaseWriter writer;
  if (auto error = writer.create(database_base)) {
    report.fatal = storage_issue("database_create", *error);
    report.fatal->source = database_base.toUtf8().toStdString();
    return report;
  }
  report.database_ready = true;
  const auto notify = [&](const QString &file, bool starting) {
    if (!progress)
      return true;
    try {
      progress(file, starting, report);
      return true;
    } catch (const std::exception &error) {
      report.fatal = issue("progress", "progress_callback_failure", file,
                           "progress", error.what());
    } catch (...) {
      report.fatal =
          issue("progress", "progress_callback_failure", file, "progress",
                "Progress callback threw a non-standard exception");
    }
    return false;
  };
  const auto fail = [&](CifBuildIssue error) {
    ++report.failed;
    report.issues.push_back(std::move(error));
  };
  const auto process = [&](const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      fail(issue("read", "cif_read_failure", path, "file",
                 file.errorString().toStdString()));
      return;
    }
    constexpr auto maximum = static_cast<qint64>(io::cif::Limits{}.bytes);
    const auto bytes = file.read(maximum + 1);
    if (file.error() != QFileDevice::NoError) {
      fail(issue("read", "cif_read_failure", path, "file",
                 file.errorString().toStdString()));
      return;
    }
    file.close();
    if (cancelled())
      return;
    auto calculated = CifCardService::from_bytes(
        std::string_view(bytes.constData(),
                         static_cast<std::size_t>(bytes.size())),
        path.toUtf8().toStdString(), {}, options.selection);
    if (cancelled())
      return;
    if (const auto *error = std::get_if<CifCardIssue>(&calculated)) {
      fail({std::string(cif_card_stage_name(error->stage)), error->code,
            error->source, error->block, error->field, error->location.line,
            error->location.column, error->message, error->core_field,
            error->core_reason});
      return;
    }
    // This deliberately retains QFileInfo::baseName().toInt() identity:
    // nonnumeric/out-of-range names map to zero, and collisions are explicit
    // per-card errors from the writer rather than partial native duplicates.
    auto adapted = cif_card_record(std::get<CifCard>(calculated),
                                   QFileInfo(path).baseName().toInt());
    if (const auto *error = std::get_if<database::WriteError>(&adapted)) {
      fail(storage_issue("record", *error));
      return;
    }
    const auto &record = std::get<database::CifRecord>(adapted);
    if (options.inorganic_only && record.subfile != "inorganic") {
      ++report.skipped;
      return;
    }
    if (cancelled())
      return;
    if (auto error = writer.write(record)) {
      auto build_error = storage_issue("database_write", *error);
      if (error->code == "rollback_failure")
        report.fatal = build_error;
      fail(std::move(build_error));
      return;
    }
    ++report.committed;
  };
  QDirIterator iterator(directory, {QStringLiteral("*.cif")}, QDir::Files,
                        options.recursive ? QDirIterator::Subdirectories
                                          : QDirIterator::NoIteratorFlags);
  while (!cancelled() && iterator.hasNext()) {
    const auto path = iterator.next();
    ++report.discovered;
    if (!notify(path, true) || cancelled())
      break;
    process(path);
    if (report.cancelled || report.fatal)
      break;
    if (!notify(path, false))
      break;
  }
  report.completed = !report.cancelled && !report.fatal;
  return report; // Writer closes with no pending transaction tail.
}
} // namespace qualpf::infrastructure
