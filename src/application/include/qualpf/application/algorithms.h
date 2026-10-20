#pragma once
#include "qualpf/application/candidate_query.h"
#include "qualpf/preprocessing/algorithms.h"
#include "qualpf/search/algorithms.h"
#include "qualpf/profile/pfsm.h"
#include <memory>

namespace qualpf::application {
// Method is separate from initial/browse/rescore/accept operations.
enum class SearchMethod { classic_sm, pfsm };
Status preflightSearchMethod(SearchMethod);

struct AlgorithmSelections {
    AlgorithmIdentity background{"qualpf.background.cpp", 1};
    AlgorithmIdentity initial_peaks{"qualpf.peaks.initial.cpp", 5};
    AlgorithmIdentity fwhm{"qualpf.peaks.fwhm.cpp", 3};
    AlgorithmIdentity experimental_peaks{"qualpf.peaks.experimental.cpp", 3};
    AlgorithmIdentity matcher{"qualpf.classic.matcher.cpp", 2};
    AlgorithmIdentity profile_expander{"qualpf.profile.il.fixed", 1};
    AlgorithmIdentity profile_fitter{"qualpf.profile.nnls-trf", 1};
    friend bool operator==(const AlgorithmSelections &, const AlgorithmSelections &) = default;
};
struct AlgorithmDefaults {
    preprocessing::BackgroundOptions background;
    preprocessing::PeakOptions peaks;
    friend bool operator==(const AlgorithmDefaults &, const AlgorithmDefaults &);
};
bool sameBackgroundOptions(const preprocessing::BackgroundOptions &, const preprocessing::BackgroundOptions &);
bool samePeakOptions(const preprocessing::PeakOptions &, const preprocessing::PeakOptions &);
// No setters or mutable parameter bag. Build a new specification to change it.
// Per-operation options/SearchParameters remain typed, owned request values.
class AlgorithmSpec {
  public:
    explicit AlgorithmSpec(AlgorithmSelections selections = {}, SearchMethod method = SearchMethod::classic_sm,
                           AlgorithmDefaults defaults = {})
        : selections_(std::move(selections)), method_(method), defaults_(std::move(defaults)) {}
    const AlgorithmSelections &selections() const { return selections_; }
    SearchMethod method() const { return method_; }
    const AlgorithmDefaults &defaults() const { return defaults_; }
    friend bool operator==(const AlgorithmSpec &, const AlgorithmSpec &) = default;
  private:
    AlgorithmSelections selections_;
    SearchMethod method_;
    AlgorithmDefaults defaults_;
};
struct AlgorithmModules {
    std::shared_ptr<const preprocessing::IBackgroundEstimator> background;
    std::shared_ptr<const preprocessing::IInitialPeakFinder> initial_peaks;
    std::shared_ptr<const preprocessing::IFwhmEstimator> fwhm;
    std::shared_ptr<const preprocessing::IExperimentalPeakExtractor> experimental_peaks;
    std::shared_ptr<const search::IClassicMatcher> matcher;
    std::shared_ptr<const profile::IProfileExpander> profile_expander;
    std::shared_ptr<const profile::IProfileFitter> profile_fitter;
};
class ResolvedAlgorithms;
using AlgorithmHandle = std::shared_ptr<const ResolvedAlgorithms>;
Status validateAlgorithmSpec(const AlgorithmSpec &);
Result<AlgorithmHandle> resolveAlgorithms(const AlgorithmSpec &, AlgorithmModules overrides = {});
AlgorithmHandle defaultAlgorithms();
bool sameScoreProducers(const AlgorithmSpec &, const AlgorithmSpec &);
bool sameResidualDependencies(const AlgorithmSpec &, const AlgorithmSpec &);
class ResolvedAlgorithms {
  public:
    const AlgorithmSpec spec;
    const AlgorithmModules modules;
  private:
    ResolvedAlgorithms(AlgorithmSpec s, AlgorithmModules m) : spec(std::move(s)), modules(std::move(m)) {}
    friend Result<AlgorithmHandle> resolveAlgorithms(const AlgorithmSpec &, AlgorithmModules);
};
} // namespace qualpf::application
