#include "candidate_reader_factory.h"
#include "infrastructure/database/database_access.h"
#include "sql_reference_repository.h"
#include <QUuid>
#include "infrastructure/settings/database_identity.h"
namespace qualpf::infrastructure {
Result<std::unique_ptr<application::CandidateReader>>
SqlCandidateReaderFactory::open(const application::ReferenceDatabase &database, std::uint64_t task,
                                const ExecutionContext &context) {
    using Return = Result<std::unique_ptr<application::CandidateReader>>;
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "search", "Search canceled"}};
    auto path = QString::fromUtf8(database.basePath.data(), static_cast<qsizetype>(database.basePath.size()));
    auto lease = DatabaseLease::read(path);
    if (!lease)
        return Return{Error{ErrorCode::invalid_input, "database", "Database is being written", "busy"}};
    if(!database.identity.empty() && database.identity!=databaseFileIdentity(database.basePath))
        return Return{Error{ErrorCode::stale_snapshot,"project.database","Reference database changed before opening","project_database_mismatch"}};
    auto scope = QUuid::createUuid().toString(QUuid::Id128) + "_task_" + QString::number(task);
    auto reader =
        std::make_unique<qualpf::infrastructure::SqlReferenceRepository>(scope, trace_, std::move(lease));
    const auto opened = reader->openDatabases(path, context.cancellation);
    if (context.cancellation.stop_requested())
        return Return{Error{ErrorCode::cancelled, "search", "Search canceled"}};
    if (!opened)
        return Return{Error{ErrorCode::missing_data, "database", "Could not open reference databases"}};
    return Return{std::unique_ptr<application::CandidateReader>(std::move(reader))};
}
} // namespace qualpf::infrastructure

namespace qualpf::infrastructure {
namespace {
struct ReadGuard final : application::ReferenceReadGuard {
    std::unique_ptr<DatabaseLease> lease;
    explicit ReadGuard(std::unique_ptr<DatabaseLease> value) : lease(std::move(value)) {}
};
} // namespace
Result<std::shared_ptr<const application::ReferenceReadGuard>>
SqlCandidateReaderFactory::protect(const application::ReferenceDatabase &database) {
    auto lease = DatabaseLease::read(QString::fromStdString(database.basePath));
    if (!lease)
        return Result<std::shared_ptr<const application::ReferenceReadGuard>>{
            Error{ErrorCode::invalid_input, "database", "Database is being written", "busy"}};
    return Result<std::shared_ptr<const application::ReferenceReadGuard>>{
        std::make_shared<ReadGuard>(std::move(lease))};
}
} // namespace qualpf::infrastructure
