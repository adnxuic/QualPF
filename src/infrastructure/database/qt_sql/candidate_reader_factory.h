#pragma once
#include "connection_trace.h"
#include "qualpf/application/search_coordinator.h"
namespace qualpf::infrastructure {
class SqlCandidateReaderFactory final : public application::CandidateReaderFactory {
  public:
    explicit SqlCandidateReaderFactory(ConnectionTrace trace = {}) : trace_(std::move(trace)) {}
    Result<std::shared_ptr<const application::ReferenceReadGuard>>
    protect(const application::ReferenceDatabase &) override;
    Result<std::unique_ptr<application::CandidateReader>>
    open(const application::ReferenceDatabase &, std::uint64_t, const ExecutionContext &) override;

  private:
    ConnectionTrace trace_;
};
} // namespace qualpf::infrastructure
