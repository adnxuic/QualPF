#pragma once
#include <QString>
#include <memory>

namespace qualpf::infrastructure {
struct DatabaseAccess;
// Nonblocking project-managed admission, independent of SQL handle ownership.
// Separate read leases may nest on one thread; no recursive shared_mutex lock.
class DatabaseLease {
  public:
    static std::unique_ptr<DatabaseLease> read(const QString &base);
    static std::unique_ptr<DatabaseLease> write(const QString &base);
    ~DatabaseLease();
    DatabaseLease(const DatabaseLease &) = delete;
    DatabaseLease &operator=(const DatabaseLease &) = delete;

  private:
    DatabaseLease(std::shared_ptr<DatabaseAccess>, bool exclusive);
    std::shared_ptr<DatabaseAccess> state_;
    bool exclusive_ = false, acquired_ = false;
};
} // namespace qualpf::infrastructure
