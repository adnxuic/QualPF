#include "gui/state/pattern_input.h"

namespace qualpf::gui {
Status openPatternFile(SessionView &session, const application::PatternSourceReader &reader, const std::filesystem::path &path) {
    const auto identity = path.u8string();
    return application::openPatternInput(session.canonicalSession(), reader,
        std::string(reinterpret_cast<const char *>(identity.data()), identity.size()));
}
}
