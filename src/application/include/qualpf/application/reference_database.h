#pragma once
#include "qualpf/application/search_coordinator.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace qualpf::application {
struct ReferenceDatabaseEntry {
    bool inUse = false;
    std::string name;
    int entries = 0;
    std::string contentType, path;
};
struct CardDetails {
    std::string id, name, mineralName, chemicalFormula, spaceGroup, quality, rir;
    int nrec = 0, nd = 0;
    std::vector<double> dvalues, intensities;
    std::string authors, journal, journalVolume, pageStart, pageEnd, color, type;
    int journalYear = 0, z = 0;
    double crystalDensity = 0, volume = 0, density = 0;
    double a = 0, b = 0, c = 0, alpha = 0, beta = 0, gamma = 0, muCuKa = 0;
    std::vector<int> h, k, l, mul;
    bool valid = false;
};
using NamedCounts = std::vector<std::pair<std::string, int>>;
class DatabaseRemoval {
  public:
    virtual ~DatabaseRemoval() = default;
    virtual std::vector<std::string> removeFiles() = 0;
};
struct DatabaseRemovalResult {
    std::vector<ReferenceDatabaseEntry> entries;
    int activeIndex = -1;
    std::vector<std::string> failedFiles;
};
class ReferenceRepository : public CandidateReader {
  public:
    virtual bool open(const std::string &) = 0;
    virtual void close() = 0;
    virtual std::pair<int, std::string> information() = 0;
    virtual CardDetails details(const std::string &) const = 0;
    virtual NamedCounts spaceGroups() const = 0;
    virtual NamedCounts colors() const = 0;
};
class DatabaseCatalogStore {
  public:
    virtual ~DatabaseCatalogStore() = default;
    virtual std::vector<ReferenceDatabaseEntry> load() const = 0;
    virtual void save(const std::vector<ReferenceDatabaseEntry> &) = 0;
    virtual std::string defaultDirectory() const = 0;
    virtual void setDefaultDirectory(const std::string &) = 0;
    virtual bool directoryExists(const std::string &) const = 0;
    virtual void discover(std::vector<ReferenceDatabaseEntry> &, const std::string &) = 0;
    virtual std::string identity(const std::string &path) const { return path; }
    virtual std::pair<int, std::string> describe(const std::string &) const = 0;
    virtual std::unique_ptr<DatabaseRemoval> prepareRemoval(const std::string &) = 0;
};

// Owner-thread use case. Providers see owned paths/records, never the live Session.
class DatabaseCatalog {
  public:
    DatabaseCatalog(SearchCoordinator &, DatabaseCatalogStore &, ReferenceRepository &,
                    std::uint64_t version = 0);
    void load();
    Status select(std::vector<ReferenceDatabaseEntry>);
    Status openActive();
    void close() { repository_.close(); }
    const std::vector<ReferenceDatabaseEntry> &entries() const { return entries_; }
    const ReferenceDatabaseEntry *active() const;
    ReferenceDatabase reference() const { return {openPath_, version_, store_.identity(openPath_)}; }
    std::string defaultDirectory() const { return store_.defaultDirectory(); }
    void setDefaultDirectory(const std::string &path) { store_.setDefaultDirectory(path); }
    bool directoryExists(const std::string &path) const { return store_.directoryExists(path); }
    void discover(const std::string &path) { store_.discover(entries_, path); }
    void persist(const std::vector<ReferenceDatabaseEntry> &entries) { store_.save(entries); }
    Result<DatabaseRemovalResult> removeDraft(std::vector<ReferenceDatabaseEntry>, int row,
                                              int activeIndex, bool deleteFiles,
                                              std::function<void(const DatabaseRemovalResult &)> beforePersist = {});
    std::pair<int, std::string> describe(const std::string &path) const { return store_.describe(path); }
    std::pair<int, std::string> information() { return repository_.information(); }
    CardDetails details(const std::string &id) const { return repository_.details(id); }
    NamedCounts spaceGroups() const { return repository_.spaceGroups(); }
    NamedCounts colors() const { return repository_.colors(); }
    CandidateReader &reader() { return repository_; }
  private:
    SearchCoordinator &searches_;
    DatabaseCatalogStore &store_;
    ReferenceRepository &repository_;
    std::vector<ReferenceDatabaseEntry> entries_;
    std::string openPath_;
    std::uint64_t version_;
};

// Shared synchronous execution; Qt clients only convert requests/projections.
enum class SynchronousSearchPurpose { browse, cli_match };
Result<std::vector<ProductCard>> synchronousQuery(SearchCoordinator &, CandidateReader &,
    ReferenceDatabase, CandidateQuery, SearchParameters, double threshold, bool scored, SearchProgress = {},
    SynchronousSearchPurpose = SynchronousSearchPurpose::browse);
} // namespace qualpf::application
