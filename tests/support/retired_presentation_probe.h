#pragma once

#include "qualpf/application/state/presentation_workspace.h"
#include "qualpf/domain/result.h"
#include <stdexcept>

namespace qualpf::legacy {

// Temporary C++ observation shell after Native provider retirement.
// No entry can allocate or publish a Native presentation mirror.
class LegacyPresentationMirror {
  public:
    static LegacyPresentationMirror &process();
    LegacyPresentationMirror(const LegacyPresentationMirror &) = delete;
    LegacyPresentationMirror &operator=(const LegacyPresentationMirror &) = delete;
    bool exists() const noexcept { return exists_; }
    ~LegacyPresentationMirror();
  private:
    LegacyPresentationMirror() = default;
    bool exists_ = false;
};

class PresentationMirrorFailure : public std::runtime_error {
  public:
    explicit PresentationMirrorFailure(Error error)
        : std::runtime_error(error.message), error_(std::move(error)) {}
    const Error &error() const noexcept { return error_; }
  private:
    Error error_;
};

} // namespace qualpf::legacy
