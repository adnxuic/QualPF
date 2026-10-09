#include "qualpf/application/workspace_ports.h"
namespace qualpf::application {
Status openPatternInput(ProductSearchSession &session, const PatternSourceReader &reader,
                        const std::string &identity) {
    auto source = session.snapshot();
    auto loaded = reader.read(identity);
    if (!loaded)
        return Status{loaded.error()};
    return session.openPattern(std::move(source), std::move(loaded).value());
}
Status openProjectInput(ProductSearchSession &session, const ProjectSourceReader &reader, const std::string &path, const MetadataObject &defaults) {
    auto source=session.snapshot();
    if (source->editing) return Status{Error{ErrorCode::invalid_input,"session.edit","Finish the preprocessing edit before loading a project."}};
    auto loaded=reader.read(path,source->backend,defaults);
    if (!loaded) return Status{loaded.error()};
    return session.commit(std::move(source),std::move(loaded).value());
}
Result<SearchInput> matchingInput(ProductHandle source, const SearchSettingsSnapshot &settings) {
    if (!source)
        return Result<SearchInput>{Error{ErrorCode::missing_data, "search.source", "Missing search input"}};
    SearchInput input;
    input.source = std::move(source);
    input.parameters = settings.parameters;
    input.operation = SearchOperation::initial;
    input.query.route = settings.parameters.filter_strongest ? CandidateQuery::Route::stored_strongest
                                                             : CandidateQuery::Route::matching;
    input.query.d = input.source->peaks.d;
    input.query.deltaD = input.source->peaks.deltaD;
    input.query.wavelength = input.source->peaks.wave;
    input.query.excludeDeleted = !settings.parameters.include_deleted_cards;
    input.query.decodeReflections = true;
    return Result<SearchInput>{std::move(input)};
}
} // namespace qualpf::application
