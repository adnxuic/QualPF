#include "tests/support/retired_presentation_probe.h"

// Temporary C++ observation shell; it has no Native provider or publication.
namespace qualpf::legacy {
LegacyPresentationMirror &LegacyPresentationMirror::process() {
    static LegacyPresentationMirror mirror;
    return mirror;
}
LegacyPresentationMirror::~LegacyPresentationMirror() = default;
} // namespace qualpf::legacy
