#pragma once
#include "qualpf/application/update_service.h"

namespace qualpf::infrastructure {
class QtUpdateServiceProvider final : public application::UpdateServiceProvider {
public:
    std::unique_ptr<application::UpdateSession> makeSession() override;
    application::UpdateDocument decode(const std::string &, const std::string &,
                                       const std::string &) const override;
};
} // namespace qualpf::infrastructure
