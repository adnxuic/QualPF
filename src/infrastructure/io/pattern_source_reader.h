#pragma once
#include "qualpf/application/workspace_ports.h"
namespace qualpf::infrastructure {
class PatternSourceReader final : public application::PatternSourceReader {
  public:
    Result<io::PatternRead> readDetailed(const std::string &) const override;
    Result<Pattern> read(const std::string &utf8Identity) const override;
};
} // namespace qualpf::infrastructure
