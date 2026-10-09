#include <filesystem>
#pragma once
#include "gui/state/search_session.h"
#include "qualpf/application/workspace_ports.h"
namespace qualpf::gui {
using namespace application;
// CLI and noninteractive clients. GUI can prepare/read, choose radiation, then
// invoke the same openPattern transaction. Native mirroring is a caller concern.
Status openPatternFile(SessionView &, const application::PatternSourceReader &, const std::filesystem::path &);
} // namespace qualpf::gui
