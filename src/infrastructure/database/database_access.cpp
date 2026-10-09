#include "database_access.h"
#include <QDir>
#include <QFileInfo>
#include <limits>
#include <map>
#include <mutex>

namespace qualpf::infrastructure {
struct DatabaseAccess {
    std::mutex mutex;
    std::size_t readers = 0;
    bool writing = false;
};
namespace {
std::shared_ptr<DatabaseAccess> access(const QString &base) {
    const QFileInfo file(base);
    const auto directory = QFileInfo(file.absolutePath()).canonicalFilePath();
    auto key =
        QDir::cleanPath((directory.isEmpty() ? file.absolutePath() : directory) + "/" + file.fileName());
#ifdef Q_OS_WIN
    key = key.toCaseFolded();
#endif
    static std::mutex registryMutex;
    static std::map<QString, std::weak_ptr<DatabaseAccess>> registry;
    std::lock_guard lock(registryMutex);
    auto &entry = registry[key];
    auto result = entry.lock();
    if (!result)
        entry = result = std::make_shared<DatabaseAccess>();
    return result;
}
} // namespace
DatabaseLease::DatabaseLease(std::shared_ptr<DatabaseAccess> state, bool exclusive)
    : state_(std::move(state)), exclusive_(exclusive) {
    std::lock_guard lock(state_->mutex);
    if (state_->writing || (exclusive_ && state_->readers) ||
        (!exclusive_ && state_->readers == std::numeric_limits<std::size_t>::max()))
        return;
    if (exclusive_)
        state_->writing = true;
    else
        ++state_->readers;
    acquired_ = true;
}
DatabaseLease::~DatabaseLease() {
    if (!acquired_)
        return;
    std::lock_guard lock(state_->mutex);
    if (exclusive_)
        state_->writing = false;
    else
        --state_->readers;
}
std::unique_ptr<DatabaseLease> DatabaseLease::read(const QString &base) {
    auto lease = std::unique_ptr<DatabaseLease>(new DatabaseLease(access(base), false));
    return lease->acquired_ ? std::move(lease) : nullptr;
}
std::unique_ptr<DatabaseLease> DatabaseLease::write(const QString &base) {
    auto lease = std::unique_ptr<DatabaseLease>(new DatabaseLease(access(base), true));
    return lease->acquired_ ? std::move(lease) : nullptr;
}
} // namespace qualpf::infrastructure
