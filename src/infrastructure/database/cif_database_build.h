#pragma once

#include "cif_import.h"
#include <QString>
#include <cstddef>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>

namespace qualpf::infrastructure {

struct CifBuildIssue {
  std::string stage;
  std::string code;
  std::string source;
  std::string block;
  std::string field;
  std::size_t line = 0;
  std::size_t column = 0;
  std::string reason;
  std::string core_field;
  std::string core_reason;
};
struct CifBuildReport {
  std::size_t discovered = 0;
  std::size_t committed = 0;
  std::size_t skipped = 0;
  std::size_t failed = 0;
  bool database_ready = false;
  bool completed = false; // Scan exhausted without cancellation/fatal error.
  bool cancelled = false;
  std::vector<CifBuildIssue> issues;
  std::optional<CifBuildIssue> fatal;
  [[nodiscard]] bool succeeded() const noexcept {
    return completed && failed == 0;
  }
};
struct CifBuildOptions {
  bool recursive = false;
  bool inorganic_only = false;
  io::cif::BlockSelection
      selection; // require_unique by default; never name preference
};
// Called synchronously before reading a discovered file and after its outcome.
// The report is a borrowed snapshot; consumers retaining it must copy it.
// Exceptions from UI/client callbacks become a fatal, reported build error.
using CifBuildProgress = std::function<void(const QString &file, bool starting,
                                            const CifBuildReport &)>;

// Streaming discovery, one complete card per attached-database transaction.
// Failed cards are reported and skipped; later files may still commit. Cancel
// preserves the already committed prefix and never commits the in-flight card
// after an observed stop request. No implicit overwrite or native fallback.
// Cancellation is checked between stages, not inside frozen numerical kernels.
[[nodiscard]] CifBuildReport
build_cif_directory(const QString &database_base, const QString &directory,
                    CifBuildOptions options = {}, std::stop_token stop = {},
                    CifBuildProgress progress = {});

} // namespace qualpf::infrastructure
