#pragma once
#include "qualpf/application/reference_database.h"
namespace qualpf::infrastructure {
class DatabaseCatalogSettings final : public application::DatabaseCatalogStore {
  public:
    std::vector<application::ReferenceDatabaseEntry> load() const override;
    void save(const std::vector<application::ReferenceDatabaseEntry> &) override;
    std::string defaultDirectory() const override;
    void setDefaultDirectory(const std::string &) override;
    bool directoryExists(const std::string &) const override;
    void discover(std::vector<application::ReferenceDatabaseEntry> &, const std::string &) override;
    std::string identity(const std::string &) const override;
    std::pair<int, std::string> describe(const std::string &) const override;
    std::unique_ptr<application::DatabaseRemoval> prepareRemoval(const std::string &) override;
};
} // namespace qualpf::infrastructure
