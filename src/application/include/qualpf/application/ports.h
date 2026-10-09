#pragma once

#include "qualpf/domain/search.h"

#include <string>
#include <vector>

namespace qualpf::application {

struct CardKey { std::string database_id; std::string card_id; };
// SQL/query syntax does not enter Core. Rich query constraints are still U14 and
// will be specified before migrating the existing query builder.
class ICardRepository {
public:
    virtual ~ICardRepository() = default;
    virtual Result<ReferenceCard> read_card(const CardKey&) = 0;
};
struct ImportedDataset {
    Pattern pattern;
    std::optional<PeakSet> peaks;
    std::vector<std::string> unavailable_fields;
    std::string source_revision;
};
// Transport is one-way and by value. Missing fields cannot be reconstructed by
// zero-filling, executing hidden preprocessing, or invoking the GUI.
class IDatasetSnapshotSource {
public:
    virtual ~IDatasetSnapshotSource() = default;
    virtual Result<ImportedDataset> read_snapshot(DatasetId requested) = 0;
};

} // namespace qualpf::application
