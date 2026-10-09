#include "qualpf/application/product_session.h"
#include "qualpf/domain/validation.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
namespace qualpf::application {
struct ProductSearchSession::Subscription::Observers {
    std::map<std::uint64_t, std::function<void(ProductHandle)>> callbacks;
    std::uint64_t next = 1, notificationFailures = 0;
};
ProductSearchSession::Subscription::Subscription(std::shared_ptr<Observers> observers, std::uint64_t id)
    : observers_(observers), id_(id) {}
ProductSearchSession::Subscription::Subscription(Subscription &&other) noexcept
    : observers_(std::move(other.observers_)), id_(std::exchange(other.id_, 0)) {}
ProductSearchSession::Subscription &
ProductSearchSession::Subscription::operator=(Subscription &&other) noexcept {
    if (this != &other) {
        reset();
        observers_ = std::move(other.observers_);
        id_ = std::exchange(other.id_, 0);
    }
    return *this;
}
ProductSearchSession::Subscription::~Subscription() { reset(); }
void ProductSearchSession::Subscription::reset() noexcept {
    if (auto observers = observers_.lock())
        observers->callbacks.erase(id_);
    id_ = 0;
    observers_.reset();
}
ProductSearchSession::Subscription
ProductSearchSession::subscribe(std::function<void(ProductHandle)> observer) {
    if (std::this_thread::get_id() != ownerThread_)
        throw std::logic_error("Subscribe requires the owner thread");
    if (observers_->next == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("Subscription IDs exhausted");
    const auto id = observers_->next++;
    observers_->callbacks.emplace(id, std::move(observer));
    return Subscription(observers_, id);
}
ProductSearchSession::ProductSearchSession(SearchBackend backend)
    : observers_(std::make_shared<Subscription::Observers>()) {
    ProductState initial;
    initial.backend = backend;
    state_ = std::make_shared<const ProductState>(std::move(initial));
}
ProductSearchSession::ProductSearchSession(ProductState initial)
    : observers_(std::make_shared<Subscription::Observers>()) {
    auto valid = validateProductState(initial);
    if (!valid)
        throw std::invalid_argument(valid.error().message);
    state_ = std::make_shared<const ProductState>(std::move(initial));
}
Status ProductSearchSession::commit(ProductHandle source, ProductState next) {
    if (std::this_thread::get_id() != ownerThread_)
        return Status{Error{ErrorCode::invalid_input, "session.thread", "Commit requires the owner thread"}};
    if (source != state_)
        return Status{Error{ErrorCode::stale_snapshot, "session", "Search input changed"}};
    if (state_->revision == std::numeric_limits<std::uint64_t>::max())
        return Status{Error{ErrorCode::exhausted_revision, "session", "Revision exhausted"}};
    auto valid = validateProductState(next);
    if (!valid)
        return valid;
    next.revision = state_->revision + 1;
    auto prepared = std::make_shared<const ProductState>(std::move(next));
    const auto callbacks = observers_->callbacks; // Allocate before atomic replacement.
    state_ = std::move(prepared);
    for (const auto &[id, callback] : callbacks) {
        if (observers_->callbacks.contains(id)) {
            try {
                callback(state_);
            } catch (...) {
                ++observers_->notificationFailures;
            }
        }
    }
    return Status{std::monostate{}};
}

void ProductSearchSession::replacePeaks(ProductPeaks peaks) {
    auto next = *state_;
    next.peaks = std::move(peaks);
    for (auto &card : next.candidates)
        card.fomCalculated = false;
    for (auto &card : next.phases)
        card.fomCalculated = false;
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
}
void ProductSearchSession::replacePattern(Pattern pattern) {
    auto result = openPattern(state_, std::move(pattern));
    if (!result)
        throw std::runtime_error(result.error().message);
}
Status ProductSearchSession::openPattern(ProductHandle source, Pattern pattern,
                                         std::vector<Pattern> comparisons, bool synchrotron) {
    auto next = prepareProductPatternOpen(source, std::move(pattern), std::move(comparisons), synchrotron);
    if (!next)
        return Status{next.error()};
    return commit(std::move(source), std::move(next).value());
}
Result<ProductState> prepareProductPatternOpen(ProductHandle source, Pattern pattern,
                                               std::vector<Pattern> comparisons, bool synchrotron) {
    if (!source || source->editing)
        return Result<ProductState>{Error{ErrorCode::invalid_input, "session.edit",
                                          "Finish the preprocessing edit before opening a pattern"}};
    const auto issues = validate(pattern);
    if (!issues.empty())
        return Result<ProductState>{issues.front()};
    if (pattern.two_theta.size() < 2 || pattern.radiation.lines.empty())
        return Result<ProductState>{
            Error{ErrorCode::missing_data, "pattern", "Expected at least two samples"}};
    ProductState next;
    next.backend = source->backend;
    next.searchOptions = source->searchOptions;
    next.restraints = source->restraints;
    next.pattern = std::move(pattern);
    next.comparisonPatterns = std::move(comparisons);
    next.peaks.wave = next.pattern.radiation.lines.front().wavelength.value;
    next.preprocessing.background = preprocessing::BackgroundOptions{};
    next.preprocessing.smoothing = preprocessing::SmoothingOptions{};
    next.preprocessing.peaks = preprocessing::PeakOptions{};
    next.preprocessing.selection = PeakSelection{};
    next.preprocessing.smoothing_applied = false;
    next.projectMetadata["radtype"] = next.pattern.radiation.kind == RadiationKind::neutron
                                          ? 1
                                          : (next.pattern.radiation.kind == RadiationKind::electron ? 2 : 0);
    next.projectMetadata["synchrotron"] = synchrotron;
    return Result<ProductState>{std::move(next)};
}
void ProductSearchSession::replaceCandidates(std::vector<ProductCard> cards) {
    auto next = *state_;
    next.candidates = std::move(cards);
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
}
void ProductSearchSession::replacePhases(std::vector<ProductCard> cards) {
    auto next = *state_;
    next.phases = std::move(cards);
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
}
void ProductSearchSession::reset() {
    cppEditOpened_.reset();
    edits_.clear();
    legacyDraft_.reset();
    legacyDraftSource_.reset();
    editOpened_.reset();
    ProductState next;
    next.backend = state_->backend;
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
}

void ProductSearchSession::beginPreprocessingEdit() {
    if (state_->editing || cppEditOpened_ || !edits_.empty())
        throw std::logic_error("Cannot nest preprocessing editors");
    auto next = *state_;
    next.editing = true;
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
    cppEditOpened_ = state_;
}

Status ProductSearchSession::commitPreprocessingEdit(ProductHandle source, ProductState next) {
    if (source != state_ || source != cppEditOpened_)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing", "Preview source changed"}};
    next.editing = false;
    cppEditOpened_.reset();
    auto result = commit(source, std::move(next));
    if (!result)
        cppEditOpened_ = std::move(source);
    return result;
}

void ProductSearchSession::cancelPreprocessingEdit(ProductHandle source) {
    if (!cppEditOpened_ || source != cppEditOpened_)
        return;
    cppEditOpened_.reset();
    if (!state_->editing)
        return;
    auto next = *state_;
    next.editing = false;
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
}

void ProductSearchSession::beginLegacyEdit(const std::string &operation) {
    if (cppEditOpened_)
        throw std::logic_error("Cannot enter a native editor during a C++ edit");
    if (edits_.contains(operation))
        return;
    if (!edits_.empty())
        throw std::logic_error("Cannot nest native preprocessing editors");
    legacyDraft_.reset();
    legacyDraftSource_.reset();
    edits_.emplace(operation, state_);
    auto next = *state_;
    next.editing = true;
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
    editOpened_ = state_;
}

void ProductSearchSession::endLegacyEdit(const std::string &operation, bool accept) {
    const auto found = edits_.find(operation);
    if (found == edits_.end())
        return;
    if (accept && legacyDraft_ && legacyDraftSource_ != state_)
        throw std::logic_error("Native preprocessing preview became stale");
    // A stale editor must never roll back a newer transaction during cleanup.
    auto next = accept && legacyDraft_ ? *legacyDraft_ : (state_ == editOpened_ ? *found->second : *state_);
    if (accept && (operation == "smooth" || operation == "background")) {
        const auto &old = found->second->pattern;
        const auto &updated = next.pattern;
        const bool sameBackground =
            (!old.background && !updated.background) ||
            (old.background && updated.background && old.background->values == updated.background->values);
        if (old.observed != updated.observed || old.two_theta != updated.two_theta || !sameBackground) {
            // Native smoothing/background does not regenerate pkind. These peaks
            // belong to the old signal and cannot be a valid search snapshot.
            next.peaks = {};
            next.preprocessing.peak_candidates.reset();
            next.preprocessing.candidate_failure.reset();
            if (operation == "smooth")
                next.preprocessing.smoothing_applied = true;
            next.peakTolerance.reset();
            next.automaticDelta = .5;
        }
    }
    legacyDraft_.reset();
    legacyDraftSource_.reset();
    editOpened_.reset();
    edits_.erase(found);
    next.editing = !edits_.empty();
    auto result = commit(state_, std::move(next));
    if (!result)
        throw std::runtime_error(result.error().message);
}

Status ProductSearchSession::stageLegacyInput(ProductHandle source, ProductState next) {
    if (!source)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing.preview", "Missing source snapshot"}};
    if (!edits_.empty() && source != editOpened_)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing.preview", "Opening source changed"}};
    if (!source->editing)
        return commit(std::move(source), std::move(next));
    legacyDraft_.reset();
    legacyDraftSource_.reset();
    if (source != state_ || edits_.empty() || source != editOpened_)
        return Status{Error{ErrorCode::stale_snapshot, "preprocessing.preview", "Preview source changed"}};
    auto valid = validateProductState(next);
    if (!valid)
        return valid;
    legacyDraft_ = std::move(next);
    legacyDraftSource_ = std::move(source);
    return Status{std::monostate{}};
}

Result<PeakSet> productSearchPeaks(const ProductPeaks &source) {
    if (!source.valid)
        return Result<PeakSet>{Error{ErrorCode::missing_data, "peaks", "Qt peak snapshot is not valid"}};
    const auto count = source.tth.size();
    if (source.integratedIntensity && source.integratedIntensity->size() != count)
        return Result<PeakSet>{
            Error{ErrorCode::invalid_input, "peaks.area", "Peak area column length differs"}};
    for (auto size : {source.d.size(), source.deltaD.size(), source.intensity.size(),
                      source.intensityOrig.size(), source.fwhm.size()})
        if (size != count)
            return Result<PeakSet>{
                Error{ErrorCode::invalid_input, "peaks", "Qt peak columns have different lengths"}};
    PeakSet out;
    out.dataset = {1};
    out.effective_wavelength = WavelengthAngstrom{source.wave};
    out.source_precision = SourcePrecision::float64;
    for (std::size_t i = 0; i < count; ++i) {
        Peak p;
        p.id = {i + 1};
        p.two_theta = TwoThetaDegrees{source.tth[i]};
        p.intensity = source.intensity[i];
        p.d_spacing = DSpacingAngstrom{source.d[i]};
        p.delta_d = DSpacingAngstrom{source.deltaD[i]};
        p.original_intensity = source.intensityOrig[i];
        p.fwhm = AngularWidthDegrees{source.fwhm[i]};
        if (source.integratedIntensity)
            p.integrated_intensity = (*source.integratedIntensity)[i];
        out.peaks.push_back(p);
    }
    auto issues = validate(out);
    if (!issues.empty())
        return Result<PeakSet>{issues.front()};
    return Result<PeakSet>{std::move(out)};
}
Status validateProductState(const ProductState &state) {
    for (const auto &p : state.comparisonPatterns) {
        auto issues = validate(p);
        if (!issues.empty())
            return Status{issues.front()};
    }
    if (!std::isfinite(state.peaks.wave))
        return Status{Error{ErrorCode::invalid_input, "peaks.wave", "Nonfinite peak wavelength"}};
    auto configuration = validatePreprocessing(state.preprocessing);
    if (!configuration)
        return configuration;
    if (!state.pattern.two_theta.empty() || !state.pattern.observed.empty()) {
        auto issues = validate(state.pattern);
        if (!issues.empty())
            return Status{issues.front()};
    }
    if (state.peaks.valid || !state.peaks.tth.empty() || !state.peaks.d.empty() ||
        !state.peaks.deltaD.empty() || !state.peaks.intensity.empty() || !state.peaks.intensityOrig.empty() ||
        !state.peaks.fwhm.empty() || state.peaks.integratedIntensity) {
        auto peaks = productSearchPeaks(state.peaks);
        if (!peaks)
            return Status{peaks.error()};
    }
    if (!std::isfinite(state.automaticDelta) || state.automaticDelta < 0 ||
        (state.peakTolerance && (!std::isfinite(*state.peakTolerance) || *state.peakTolerance < 0)))
        return Status{Error{ErrorCode::invalid_input, "session.delta", "Invalid search tolerance"}};
    const auto bad = Error{ErrorCode::invalid_input, "session.options", "Invalid persisted search option"};
    for (const char *key :
         {"SearchOptions/minFom", "SearchOptions/weight2thetaD", "SearchOptions/weightIntensity",
          "SearchOptions/weightPhases", "SearchOptions/delta2theta", "SearchOptions/maxEntries"}) {
        auto it = state.searchOptions.find(key);
        if (it == state.searchOptions.end())
            continue;
        const auto &data = it->second.data;
        if (!std::holds_alternative<double>(data) && !std::holds_alternative<std::int64_t>(data))
            return Status{bad};
        const double value = std::holds_alternative<double>(data) ? std::get<double>(data)
                                                                  : double(std::get<std::int64_t>(data));
        if (!std::isfinite(value) || value < 0)
            return Status{bad};
        if (std::string_view(key) == "SearchOptions/maxEntries" &&
            (value < 1 || value > std::numeric_limits<int>::max() || value != std::floor(value)))
            return Status{bad};
        if (std::string_view(key) == "SearchOptions/delta2theta" && value <= 0)
            return Status{bad};
    }
    for (const char *key : {"SearchOptions/delta2thetaAuto", "SearchOptions/residualSearching",
                            "SearchOptions/checkStrongest", "SearchOptions/checkDeleted"}) {
        auto it = state.searchOptions.find(key);
        if (it != state.searchOptions.end() && !std::holds_alternative<bool>(it->second.data))
            return Status{bad};
    }
    return Status{std::monostate{}};
}
} // namespace qualpf::application
