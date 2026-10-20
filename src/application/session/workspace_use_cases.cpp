#include "qualpf/application/workspace_ports.h"
namespace qualpf::application {
Status openPatternInput(ProductSearchSession &session, const PatternSourceReader &reader,
                        const std::string &identity) {
    auto source = session.snapshot();
    auto loaded = reader.readImported(identity);
    if (!loaded)
        return Status{loaded.error()};
    auto imported = std::move(loaded).value();
    return session.openPattern(std::move(source), std::move(imported.input.pattern), {}, false,
                               std::move(imported.original_signal_provenance));
}
Result<SearchInput> matchingInput(ProductHandle source, const SearchSettingsSnapshot &settings) {
    if (!source)
        return Result<SearchInput>{Error{ErrorCode::missing_data, "search.source", "Missing search input"}};
    SearchInput input;
    input.source = std::move(source);
    input.method = input.source->algorithms->spec.method();
    auto method = preflightSearchMethod(input.method);
    if (!method) return Result<SearchInput>{method.error()};
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
