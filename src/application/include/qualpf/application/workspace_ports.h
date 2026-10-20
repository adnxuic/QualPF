#pragma once
#include "qualpf/application/search_engine.h"
#include "qualpf/io/pattern_reader.h"
#include "qualpf/application/preferences.h"
#include "qualpf/application/project_document.h"
namespace qualpf::application {
struct SearchSettingsSnapshot {
    SearchParameters parameters;
    MetadataObject persistedOptions;
};
class SearchSettingsProvider {
  public:
    virtual ~SearchSettingsProvider() = default;
    virtual PreferencePayload option(ProductHandle, const std::string &, const PreferencePayload &fallback) const = 0;
    virtual Result<SearchSettingsSnapshot> load(ProductHandle) const = 0;
};
struct ImportedPattern {
    io::PatternRead input;
    std::optional<OriginalSignalProvenance> original_signal_provenance;
};
class PatternSourceReader {
  public:
    virtual ~PatternSourceReader() = default;
    virtual Result<Pattern> read(const std::string &utf8Identity) const = 0;
    virtual Result<io::PatternRead> readDetailed(const std::string &identity) const {
        auto loaded=read(identity);
        if (!loaded) return Result<io::PatternRead>{loaded.error()};
        return Result<io::PatternRead>{io::PatternRead{std::move(loaded).value(), false, 0}};
    }
    virtual Result<ImportedPattern> readImported(const std::string &identity) const {
        auto loaded = readDetailed(identity);
        if (!loaded) return Result<ImportedPattern>{loaded.error()};
        return Result<ImportedPattern>{ImportedPattern{std::move(loaded).value(), {}}};
    }
};
class ProjectSourceReader {
public:
    virtual ~ProjectSourceReader() = default;
    virtual Result<ProjectDocument> read(const std::string &, SearchBackend, const ProjectReadOptions & = {}) const = 0;
};
class Workspace;
Status openProjectInput(Workspace &, const ProjectSourceReader &, const std::string &, const ProjectReadOptions & = {});
// Read into an owned value, then apply the canonical open transaction. Neither
// the provider nor the parser receives the live Session.
Status openPatternInput(ProductSearchSession &, const PatternSourceReader &, const std::string &);
Result<SearchInput> matchingInput(ProductHandle, const SearchSettingsSnapshot &);
} // namespace qualpf::application
