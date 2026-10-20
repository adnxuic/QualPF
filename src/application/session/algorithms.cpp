#include "qualpf/application/algorithms.h"
#include "qualpf/application/preprocessing_configuration.h"
#include "qualpf/application/search_engine.h"
#include <algorithm>
#include <stdexcept>

namespace qualpf::application {
bool sameScoreProducers(const AlgorithmSpec &a, const AlgorithmSpec &b) {
    return a.method() == b.method() && (a.method() == SearchMethod::pfsm ?
        a.selections().profile_expander == b.selections().profile_expander &&
        a.selections().profile_fitter == b.selections().profile_fitter :
        a.selections().matcher == b.selections().matcher);
}
bool sameResidualDependencies(const AlgorithmSpec &a, const AlgorithmSpec &b) {
    const auto &x = a.selections(), &y = b.selections();
    return sameScoreProducers(a, b) && x.background == y.background &&
        x.initial_peaks == y.initial_peaks && x.fwhm == y.fwhm &&
        x.experimental_peaks == y.experimental_peaks && a.defaults() == b.defaults();
}
namespace {
bool sameRange(const std::optional<AngularRange> &a, const std::optional<AngularRange> &b) {
    return bool(a) == bool(b) && (!a || (a->minimum == b->minimum && a->maximum == b->maximum));
}
} // namespace
bool sameBackgroundOptions(const preprocessing::BackgroundOptions &a,
                           const preprocessing::BackgroundOptions &b) {
    return a.method == b.method && a.automatic == b.automatic && a.coefficients == b.coefficients &&
           a.iterations == b.iterations && a.half_window == b.half_window &&
           sameRange(a.bruckner_range, b.bruckner_range);
}
bool samePeakOptions(const preprocessing::PeakOptions &a, const preprocessing::PeakOptions &b) {
    return a.threshold_percent == b.threshold_percent && a.slope == b.slope &&
           a.search_range == b.search_range && a.half_window == b.half_window &&
           a.polynomial_degree == b.polynomial_degree && a.maximum_peaks == b.maximum_peaks &&
           a.minimum_peaks == b.minimum_peaks && a.automatic_window == b.automatic_window &&
           sameRange(a.detection_range, b.detection_range);
}
bool operator==(const AlgorithmDefaults &a, const AlgorithmDefaults &b) {
    return sameBackgroundOptions(a.background, b.background) && samePeakOptions(a.peaks, b.peaks);
}
Status preflightSearchMethod(SearchMethod method) {
    if (method == SearchMethod::classic_sm || method == SearchMethod::pfsm)
        return Status{std::monostate{}};
    return Status{Error{ErrorCode::unsupported, "search.method",
                        "Unknown search method", "unsupported_search_method"}};
}
Status validateAlgorithmSpec(const AlgorithmSpec &spec) {
    auto method = preflightSearchMethod(spec.method());
    if (!method)
        return method;
    const auto &s = spec.selections();
    for (const auto *id :
         {&s.background, &s.initial_peaks, &s.fwhm, &s.experimental_peaks,
          &s.matcher, &s.profile_expander, &s.profile_fitter})
        if (id->id.empty() || id->version !=
            (id->id == "qualpf.peaks.initial.cpp" ? 5u :
             (id->id == "qualpf.peaks.fwhm.cpp" || id->id == "qualpf.peaks.experimental.cpp") ? 3u :
             id->id == "qualpf.classic.matcher.cpp" ? 2u : 1u))
            return Status{Error{ErrorCode::unsupported, "algorithms.spec",
                                "Unknown algorithm contract version or empty ID",
                                "unsupported_algorithm_contract"}};
    PreprocessingConfiguration parameters;
    parameters.background = spec.defaults().background;
    parameters.peaks = spec.defaults().peaks;
    return validatePreprocessing(parameters);
}
namespace {
template <class Default, class Interface>
bool resolve(const AlgorithmIdentity &selected, std::shared_ptr<const Interface> &implementation) {
    if (!implementation) {
        if (selected != Default{}.identity())
            return false;
        implementation = std::make_shared<const Default>();
    }
    return implementation->identity() == selected;
}
} // namespace
Result<AlgorithmHandle> resolveAlgorithms(const AlgorithmSpec &spec, AlgorithmModules modules) {
    auto valid = validateAlgorithmSpec(spec);
    if (!valid)
        return Result<AlgorithmHandle>{valid.error()};
    const auto &s = spec.selections();
    if (!resolve<preprocessing::CppBackgroundEstimator>(s.background, modules.background) ||
        !resolve<preprocessing::ClassicInitialPeakFinder>(s.initial_peaks, modules.initial_peaks) ||
        !resolve<preprocessing::ClassicFwhmEstimator>(s.fwhm, modules.fwhm) ||
        !resolve<preprocessing::ClassicPeakExtractor>(s.experimental_peaks, modules.experimental_peaks) ||
        !resolve<search::ClassicMatcher>(s.matcher, modules.matcher) ||
        !resolve<profile::IntermediateLorentzianExpander>(s.profile_expander, modules.profile_expander) ||
        !resolve<profile::NnlsTrfProfileFitter>(s.profile_fitter, modules.profile_fitter))
        return Result<AlgorithmHandle>{
            Error{ErrorCode::unsupported, "algorithms.selection",
                  "Unknown algorithm ID or injected implementation does not match the specification",
                  "unresolved_algorithm"}};
    if (modules.matcher->produces() != search::classic_match_contract())
        return Result<AlgorithmHandle>{Error{ErrorCode::unsupported, "algorithms.match_contract",
                                             "Incompatible matcher and FOM", "incompatible_match_contract"}};
    return Result<AlgorithmHandle>{AlgorithmHandle(new ResolvedAlgorithms(spec, std::move(modules)))};
}
AlgorithmHandle defaultAlgorithms() {
    auto result = resolveAlgorithms(AlgorithmSpec{});
    if (!result)
        throw std::logic_error(result.error().message);
    return std::move(result).value();
}
Result<CandidateQuery> planCandidateQuery(const CandidateQuery &requested, bool scored,
    const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<CandidateQuery>{Error{ErrorCode::cancelled, "search", "Search canceled"}};
    auto query = requested;
    if (query.minimumSpecies < 0 || query.maximumSpecies < 0 ||
        (query.minimumSpecies && query.maximumSpecies && query.minimumSpecies > query.maximumSpecies))
        return Result<CandidateQuery>{Error{ErrorCode::invalid_input, "restraints.species", "Invalid species-count bounds"}};
    query.decodeReflections = scored;
    return Result<CandidateQuery>{std::move(query)};
}
} // namespace qualpf::application
