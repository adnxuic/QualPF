#include "pfsm_sampling.h"
#include "pfsm_simd.h"
#include "pfsm_work.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <numbers>
#include <utility>
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#endif

namespace qualpf::profile {
namespace {
Error invalid(const char *message) { return {ErrorCode::invalid_input, "pfsm.expansion", message}; }
class IlTemplate final : public IProfileTemplate {
  public:
    IlTemplate(std::vector<ProfileLine> lines, double width, bool bounded_lines = true)
        : lines_(std::move(lines)), width_(width), factor_(std::sqrt(4 * (std::cbrt(4.) - 1)) / width),
          bounded_lines_(bounded_lines) {}
    Result<ProfileTemplate> relativeTo(double origin, const ExecutionContext &context) const {
        std::vector<ProfileLine> lines;
        lines.reserve(lines_.size());
        bool bounded = true;
        for (auto line : lines_) {
            if ((lines.size() & 255) == 0 && context.cancellation.stop_requested())
                return Result<ProfileTemplate>{
                    Error{ErrorCode::cancelled, "pfsm.expansion", "IL cache preparation canceled"}};
            line.position -= origin;
            bounded = bounded && std::abs(line.position) <= 360;
            lines.push_back(std::move(line));
        }
        return Result<ProfileTemplate>{std::make_shared<const IlTemplate>(std::move(lines), width_, bounded)};
    }
    double fwhmDegrees() const noexcept override { return width_; }
    std::size_t storageBytes() const noexcept { return sizeof(*this) + lines_.capacity() * sizeof(ProfileLine); }
    Result<ProfileSamples> evaluate(std::span<const double> x, double shift, bool derivative,
                                    const ExecutionContext &context) const override {
        if (context.numerical_policy != NumericalPolicy::explicit_float64)
            return Result<ProfileSamples>{invalid("IL requires explicit binary64 policy")};
        if (!std::isfinite(shift))
            return Result<ProfileSamples>{invalid("Nonfinite shift")};
        ProfileSamples result;
        result.intensity.resize(x.size());
        if (derivative)
            result.shift_derivative.resize(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) {
            if ((i & 255) == 0 && context.cancellation.stop_requested())
                return Result<ProfileSamples>{
                    Error{ErrorCode::cancelled, "pfsm.expansion", "IL evaluation canceled"}};
            if (!std::isfinite(x[i]) || (i && !(x[i] > x[i - 1])))
                return Result<ProfileSamples>{invalid("Coordinates must be finite and strictly increasing")};
        }
#if defined(__SSE2__) || defined(_M_X64)
        // Independent coordinates can execute together without changing the
        // reflection sum order for either coordinate. Supported products are
        // x64, where SSE2 is baseline. AVX2 lives in an independently compiled
        // source and is called only after baseline CPU/OS capability detection.
        // The bounded fast domain proves that the square cannot overflow.
        // Cache preparation translates lines, so their positions need the same
        // bound as coordinates and shifts before entering this square-root path.
        if (bounded_lines_ && !derivative && !x.empty() && std::abs(x.front()) <= 360 && std::abs(x.back()) <= 360 &&
            std::abs(shift) <= 360 && factor_ < 1e140) {
            std::size_t first = 0;
            if (x.size() >= 4 && detail::avx2Available()) {
                const auto count = x.size() - x.size() % 4;
                const auto status = detail::ilAvx2(x.first(count), lines_, shift, factor_,
                    std::span<double>{result.intensity}.first(count), context.cancellation);
                if (status == detail::SimdResult::cancelled)
                    return Result<ProfileSamples>{Error{ErrorCode::cancelled, "pfsm.expansion", "IL evaluation canceled"}};
                if (status == detail::SimdResult::complete)
                    first = count;
            }
            // Keep the remainder in the original baseline-compiled evaluator.
            // In particular, its odd scalar sample retains the platform sqrt
            // provider; AVX2 compilation can inline a differently rounded sqrt.
            const auto one = _mm_set1_pd(1.);
            const auto factor = _mm_set1_pd(factor_), delta = _mm_set1_pd(shift);
            for (const auto &line : lines_) {
                const auto position = _mm_set1_pd(line.position), strength = _mm_set1_pd(line.intensity);
                std::size_t i = first;
                for (; i + 1 < x.size(); i += 2) {
                    if ((i & 255) == 0 && context.cancellation.stop_requested())
                        return Result<ProfileSamples>{
                            Error{ErrorCode::cancelled, "pfsm.expansion", "IL evaluation canceled"}};
                    const auto v = _mm_mul_pd(
                        _mm_sub_pd(_mm_sub_pd(_mm_loadu_pd(x.data() + i), position), delta), factor);
                    const auto inverse = _mm_div_pd(one, _mm_sqrt_pd(_mm_add_pd(one, _mm_mul_pd(v, v))));
                    const auto kernel = _mm_mul_pd(_mm_mul_pd(inverse, inverse), inverse);
                    _mm_storeu_pd(
                        result.intensity.data() + i,
                        _mm_add_pd(_mm_loadu_pd(result.intensity.data() + i), _mm_mul_pd(strength, kernel)));
                }
                if (i < x.size()) {
                    const double v = (x[i] - line.position - shift) * factor_;
                    const double inverse = 1 / std::sqrt(1 + v * v);
                    result.intensity[i] += line.intensity * ((inverse * inverse) * inverse);
                }
            }
            for (double value : result.intensity)
                if (!std::isfinite(value))
                    return Result<ProfileSamples>{
                        Error{ErrorCode::numerical_failure, "pfsm.expansion", "IL overflow"}};
            return Result<ProfileSamples>{std::move(result)};
        }
#endif
        for (std::size_t i = 0; i < x.size(); ++i) {
            if ((i & 255) == 0 && context.cancellation.stop_requested())
                return Result<ProfileSamples>{
                    Error{ErrorCode::cancelled, "pfsm.expansion", "IL evaluation canceled"}};
            double value = 0, gradient = 0;
            for (const auto &line : lines_) {
                const double v = (x[i] - line.position - shift) * factor_;
                // Ordinary physical distances use the direct IL expression.
                // Retain hypot for extreme inputs where squaring could overflow.
                // This changes no tail, sampling or summation-order contract.
                const double inverse = 1 / (std::abs(v) < 1e150 ? std::sqrt(1 + v * v) : std::hypot(1., v));
                const double kernel = inverse * inverse * inverse;
                value += line.intensity * kernel;
                if (derivative && std::isfinite(v))
                    gradient += line.intensity * (3 * factor_) * (v * inverse) * kernel * inverse;
            }
            if (!std::isfinite(value) || !std::isfinite(gradient))
                return Result<ProfileSamples>{
                    Error{ErrorCode::numerical_failure, "pfsm.expansion", "IL overflow"}};
            result.intensity[i] = value;
            if (derivative)
                result.shift_derivative[i] = gradient;
        }
        return Result<ProfileSamples>{std::move(result)};
    }

  private:
    const std::vector<ProfileLine> lines_;
    const double width_, factor_;
    const bool bounded_lines_;
};

// Optional interpolation of the same complete IL sum, never a truncated peak.
// 32 Chebyshev nodes per interval of at most H/2 keep the nearest IL
// singularity outside the Bernstein ellipse rho=4. Its analytic truncation
// bound is below 1e-18 times the sum of peak strengths; binary64 rounding
// dominates in practice.
class CachedIlTemplate final : public IProfileTemplate {
  public:
    static constexpr std::size_t nodes = 32;
    using Coefficients = detail::IlCoefficients;
    CachedIlTemplate(ProfileTemplate direct, double lower, double upper,
                     std::vector<Coefficients> coefficients)
        : direct_(std::move(direct)), lower_(lower), upper_(upper),
          step_((upper - lower) / double(coefficients.size())), coefficients_(std::move(coefficients)) {}
    double fwhmDegrees() const noexcept override { return direct_->fwhmDegrees(); }
    bool usesApproximation() const noexcept override { return true; }
    std::size_t storageBytes() const noexcept { return sizeof(*this) + coefficients_.capacity() * sizeof(Coefficients); }
    Result<ProfileSamples> evaluateExact(std::span<const double> x, double shift, bool derivative,
                                         const ExecutionContext &context) const override {
        return direct_->evaluate(x, shift, derivative, context);
    }
    Result<ProfileSamples> evaluate(std::span<const double> x, double shift, bool derivative,
                                    const ExecutionContext &context) const override {
        // Derivatives are supplied by the original analytic kernel. They are not
        // used by the reduced finite-difference solver.
        // With |shift| <= the cache span, relative query intermediates remain
        // bounded by twice the span. Larger shifts can cancel large coordinates;
        // preserve the original direct evaluator in that domain.
        if (derivative || x.empty() || !std::isfinite(shift) || std::abs(shift) > upper_ - lower_ ||
            (x.front() - lower_) - shift < 0 || (x.back() - lower_) - shift > upper_ - lower_)
            return evaluateExact(x, shift, derivative, context);
        if (context.numerical_policy != NumericalPolicy::explicit_float64)
            return Result<ProfileSamples>{invalid("IL requires explicit binary64 policy")};
        ProfileSamples out;
        out.intensity.resize(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) {
            if ((i & 255) == 0 && context.cancellation.stop_requested())
                return Result<ProfileSamples>{
                    Error{ErrorCode::cancelled, "pfsm.expansion", "IL cache evaluation canceled"}};
            if (!std::isfinite(x[i]) || (i && x[i] <= x[i - 1]))
                return Result<ProfileSamples>{invalid("Coordinates must be finite and strictly increasing")};
        }
        if (detail::avx2Available()) {
            const auto status = detail::cachedIlAvx2(x, lower_, step_, shift, coefficients_, out.intensity,
                                                     context.cancellation);
            if (status == detail::SimdResult::cancelled)
                return Result<ProfileSamples>{Error{ErrorCode::cancelled, "pfsm.expansion", "IL cache evaluation canceled"}};
            if (status == detail::SimdResult::fallback)
                return evaluateExact(x, shift, derivative, context);
            return Result<ProfileSamples>{std::move(out)};
        }
        const auto cellAt = [&](double coordinate) {
            // Subtract the local origin before the small trial shift, so that
            // physical-coordinate rounding cannot erase a solver perturbation.
            const double location = (coordinate - lower_) - shift;
            const auto index =
                std::min(coefficients_.size() - 1, static_cast<std::size_t>(location / step_));
            const double t = 2 * (location - double(index) * step_) / step_ - 1;
            return std::pair{index, t};
        };
        std::size_t i = 0;
#if defined(__SSE2__) || defined(_M_X64)
        // Two independent Clenshaw recurrences, with the scalar operation order
        // unchanged in each lane. No horizontal reduction or FMA is introduced.
        for (; i + 1 < x.size(); i += 2) {
            if ((i & 255) == 0 && context.cancellation.stop_requested())
                return Result<ProfileSamples>{
                    Error{ErrorCode::cancelled, "pfsm.expansion", "IL cache evaluation canceled"}};
            const auto left = cellAt(x[i]), right = cellAt(x[i + 1]);
            const auto &cl = coefficients_[left.first], &cr = coefficients_[right.first];
            const auto t = _mm_set_pd(right.second, left.second);
            const auto twice = _mm_mul_pd(_mm_set1_pd(2.), t);
            auto a = _mm_setzero_pd(), b = _mm_setzero_pd();
            for (std::size_t k = nodes - 1; k > 0; --k) {
                const auto next = _mm_add_pd(_mm_sub_pd(_mm_mul_pd(twice, a), b), _mm_set_pd(cr[k], cl[k]));
                b = a;
                a = next;
            }
            _mm_storeu_pd(out.intensity.data() + i,
                          _mm_add_pd(_mm_sub_pd(_mm_mul_pd(t, a), b), _mm_set_pd(cr[0], cl[0])));
            if (!std::isfinite(out.intensity[i]) || !std::isfinite(out.intensity[i + 1]) ||
                out.intensity[i] < 0 || out.intensity[i + 1] < 0)
                return evaluateExact(x, shift, derivative, context);
        }
#endif
        for (; i < x.size(); ++i) {
            if ((i & 255) == 0 && context.cancellation.stop_requested())
                return Result<ProfileSamples>{
                    Error{ErrorCode::cancelled, "pfsm.expansion", "IL cache evaluation canceled"}};
            const auto [index, t] = cellAt(x[i]);
            const auto &c = coefficients_[index];
            double a = 0, b = 0;
            for (std::size_t k = nodes - 1; k > 0; --k) {
                const double next = (2 * t * a - b) + c[k];
                b = a;
                a = next;
            }
            out.intensity[i] = (t * a - b) + c[0];
            // Never clip an interpolation anomaly or publish an invalid template.
            if (!std::isfinite(out.intensity[i]) || out.intensity[i] < 0)
                return evaluateExact(x, shift, derivative, context);
        }
        return Result<ProfileSamples>{std::move(out)};
    }

    // Fast-only differentiated Clenshaw recurrence. The formal evaluator above is unchanged.
    Result<ProfileSamples> evaluatePaired(std::span<const double> x,double shift,const ExecutionContext &context) const {
        if(x.empty() || !std::isfinite(shift) || std::abs(shift)>upper_-lower_ ||
           (x.front()-lower_)-shift<0 || (x.back()-lower_)-shift>upper_-lower_)
            return evaluateExact(x,shift,true,context);
        if(context.numerical_policy!=NumericalPolicy::explicit_float64)
            return Result<ProfileSamples>{invalid("IL requires explicit binary64 policy")};
        ProfileSamples out;out.intensity.resize(x.size());out.shift_derivative.resize(x.size());
        for(std::size_t i=0;i<x.size();++i) {
            if((i&255)==0 && context.cancellation.stop_requested())
                return Result<ProfileSamples>{Error{ErrorCode::cancelled,"pfsm.fast.derivative","Cache derivative canceled"}};
            if(!std::isfinite(x[i]) || (i && x[i]<=x[i-1]))
                return Result<ProfileSamples>{invalid("Coordinates must be finite and strictly increasing")};
            const double location=(x[i]-lower_)-shift;
            const auto cell=std::min(coefficients_.size()-1,static_cast<std::size_t>(location/step_));
            const double t=2*(location-double(cell)*step_)/step_-1;
            const auto &c=coefficients_[cell];
            double a=0,b=0,da=0,db=0;
            for(std::size_t k=nodes-1;k>0;--k) {
                const double next=(2*t*a-b)+c[k];
                const double derivative=2*a+2*t*da-db;
                b=a;a=next;db=da;da=derivative;
            }
            out.intensity[i]=(t*a-b)+c[0];
            out.shift_derivative[i]=(-2/step_)*(a+t*da-db);
            if(!std::isfinite(out.intensity[i]) || out.intensity[i]<0 || !std::isfinite(out.shift_derivative[i]))
                return evaluateExact(x,shift,true,context);
        }
        return Result<ProfileSamples>{std::move(out)};
    }

  private:
    const ProfileTemplate direct_;
    const double lower_, upper_, step_;
    const std::vector<Coefficients> coefficients_;
};

struct ChebyshevTransform {
    static constexpr auto nodes = CachedIlTemplate::nodes;
    std::array<double, nodes> locations{};
    std::array<std::array<double, nodes>, nodes> cosine{};
    ChebyshevTransform() {
        detail::countWork(&detail::WorkCounters::chebyshev_tables);
        for (std::size_t index = 0; index < nodes; ++index) {
            const double angle = std::numbers::pi * (double(nodes - index) - .5) / double(nodes);
            locations[index] = std::cos(angle);
            for (std::size_t order = 0; order < nodes; ++order)
                cosine[order][index] = std::cos(double(order) * angle);
        }
    }
};
const ChebyshevTransform &nearestChebyshevTransform() {
    static const ChebyshevTransform transform;
    return transform;
}
Result<ProfileTemplate> cached(std::shared_ptr<const IlTemplate> direct, std::pair<double, double> window,
                               const ExecutionContext &context) {
    const auto [lower, upper] = window;
    if (!std::isfinite(lower) || !std::isfinite(upper) || lower >= upper)
        return Result<ProfileTemplate>{invalid("Invalid IL cache window")};
    const double count = std::ceil((upper - lower) / (.5 * direct->fwhmDegrees()));
    // Bounded per-candidate storage, independent of the number of database cards.
    if (!std::isfinite(count) || count > 16384 || count < 1)
        return Result<ProfileTemplate>{std::move(direct)};
    const auto segments = static_cast<std::size_t>(count);
    constexpr auto n = CachedIlTemplate::nodes;
    const double step = (upper - lower) / double(segments);
    // Build translated lines only after validating the bounded cache window.
    // The original template remains the exact/fallback evaluator.
    const auto relative = direct->relativeTo(lower, context);
    if (!relative)
        return Result<ProfileTemplate>{relative.error()};
    std::optional<ChebyshevTransform> localTransform;
    if (std::fegetround() != FE_TONEAREST)
        localTransform.emplace();
    const auto &transform = localTransform ? *localTransform : nearestChebyshevTransform();
    std::vector<double> x;
    x.reserve(segments * n);
    for (std::size_t cell = 0; cell < segments; ++cell)
        for (double t : transform.locations)
            x.push_back((double(cell) + .5 + .5 * t) * step);
    if (std::adjacent_find(x.begin(), x.end(), [](double a, double b) { return a >= b; }) != x.end())
        return Result<ProfileTemplate>{std::move(direct)};
    auto samples = relative.value()->evaluate(x, 0, false, context);
    if (!samples) {
        // Internal interpolation nodes can overflow even when the requested
        // observation samples are finite. The optional cache must not reject
        // that direct problem; cancellation and input errors still propagate.
        if (samples.error().code == ErrorCode::numerical_failure)
            return Result<ProfileTemplate>{std::move(direct)};
        return Result<ProfileTemplate>{samples.error()};
    }
    std::vector<CachedIlTemplate::Coefficients> coefficients(segments);
    for (std::size_t cell = 0; cell < segments; ++cell) {
        if (context.cancellation.stop_requested())
            return Result<ProfileTemplate>{
                Error{ErrorCode::cancelled, "pfsm.expansion", "IL cache preparation canceled"}};
        for (std::size_t k = 0; k < n; ++k) {
            double sum = 0, correction = 0;
            for (std::size_t j = 0; j < n; ++j) {
                const double term = (samples.value().intensity[cell * n + j] / double(n)) * transform.cosine[k][j];
                const double corrected = term - correction;
                const double next = sum + corrected;
                correction = (next - sum) - corrected;
                sum = next;
            }
            coefficients[cell][k] = k ? 2 * sum : sum;
            if (!std::isfinite(coefficients[cell][k]))
                return Result<ProfileTemplate>{std::move(direct)};
        }
    }
    return Result<ProfileTemplate>{
        std::make_shared<const CachedIlTemplate>(std::move(direct), lower, upper, std::move(coefficients))};
}
} // namespace
Result<std::vector<ProfileLine>> expandProfileLines(const ProfileExpansionInput &in,
                                                                const ExecutionContext &context) {
    if (context.cancellation.stop_requested())
        return Result<std::vector<ProfileLine>>{
            Error{ErrorCode::cancelled, "pfsm.expansion", "IL preparation canceled"}};
    if (context.numerical_policy != NumericalPolicy::explicit_float64 || !std::isfinite(in.fwhm_degrees) ||
        in.fwhm_degrees <= 0 || in.fwhm_degrees > 180 ||
        !std::isfinite(std::sqrt(4 * (std::cbrt(4.) - 1)) / in.fwhm_degrees))
        return Result<std::vector<ProfileLine>>{invalid("A positive physical FWHM and binary64 policy are required")};
    const bool spacing = in.coordinates == ProfileExpansionInput::Coordinates::d_spacing_angstrom;
    if (!spacing && in.coordinates != ProfileExpansionInput::Coordinates::two_theta_degrees)
        return Result<std::vector<ProfileLine>>{invalid("Unknown reference coordinates")};
    double radiation_sum = 0;
    if (spacing) {
        for (const auto &r : in.radiation.lines) {
            if (!std::isfinite(r.wavelength.value) || r.wavelength.value <= 0 ||
                !std::isfinite(r.relative_weight) || r.relative_weight < 0)
                return Result<std::vector<ProfileLine>>{invalid("Invalid radiation component")};
            radiation_sum += r.relative_weight;
        }
        if (!(radiation_sum > 0) || !std::isfinite(radiation_sum))
            return Result<std::vector<ProfileLine>>{
                invalid("Explicit radiation components are required for d spacings")};
    }
    std::vector<ProfileLine> lines;
    for (const auto &line : in.lines) {
        if (context.cancellation.stop_requested())
            return Result<std::vector<ProfileLine>>{
                Error{ErrorCode::cancelled, "pfsm.expansion", "IL preparation canceled"}};
        if (!std::isfinite(line.position) || !std::isfinite(line.intensity) || line.intensity < 0 ||
            (spacing ? line.position <= 0 : (line.position < 0 || line.position > 180)))
            return Result<std::vector<ProfileLine>>{invalid("Invalid paired reference line")};
        if (!spacing)
            lines.push_back(line);
        else
            for (const auto &r : in.radiation.lines) {
                const double sine = .5 * r.wavelength.value / line.position;
                if (sine > 1 || r.relative_weight == 0)
                    continue;
                lines.push_back({line.identity, 2 * std::asin(sine) * (180 / std::numbers::pi),
                                 line.intensity * (r.relative_weight / radiation_sum)});
            }
    }
    return Result<std::vector<ProfileLine>>{std::move(lines)};
}
Result<ProfileTemplate> IntermediateLorentzianExpander::prepare(const ProfileExpansionInput &in,
                                                                const ExecutionContext &context) const {
    detail::countWork(&detail::WorkCounters::template_preparations);
    auto expanded = expandProfileLines(in, context);
    if (!expanded) return Result<ProfileTemplate>{expanded.error()};
    auto lines = std::move(expanded).value();
    // Expanded physical lines are in [0, 180], proving the default SIMD bound.
    auto direct = std::make_shared<const IlTemplate>(std::move(lines), in.fwhm_degrees);
    if (in.interpolation_window)
        return cached(std::move(direct), *in.interpolation_window, context);
    return Result<ProfileTemplate>{std::move(direct)};
}
Result<ProfileSamples> evaluateProfilePairedSamples(const ProfileTemplate &profile,std::span<const double> x,
    double shift,const ExecutionContext &context) {
    detail::countWork(&detail::WorkCounters::paired_samples);
    if(!profile) return Result<ProfileSamples>{invalid("Missing fast profile")};
    if(const auto *cached=dynamic_cast<const CachedIlTemplate *>(profile.get()))
        return cached->evaluatePaired(x,shift,context);
    return profile->evaluateExact(x,shift,true,context);
}
namespace {
bool sameBits(double left, double right) {
    return std::bit_cast<std::uint64_t>(left) == std::bit_cast<std::uint64_t>(right);
}
unsigned denormalMode() {
#if defined(__SSE2__) || defined(_M_X64)
    return _mm_getcsr() & 0x8040;
#else
    return 0;
#endif
}
bool sameExpansion(const ProfileExpansionInput &left, const ProfileExpansionInput &right) {
    if (left.coordinates != right.coordinates || !sameBits(left.fwhm_degrees, right.fwhm_degrees) ||
        left.lines.size() != right.lines.size() || left.radiation.kind != right.radiation.kind ||
        left.radiation.lines.size() != right.radiation.lines.size() ||
        left.interpolation_window.has_value() != right.interpolation_window.has_value())
        return false;
    if (left.interpolation_window &&
        (!sameBits(left.interpolation_window->first, right.interpolation_window->first) ||
         !sameBits(left.interpolation_window->second, right.interpolation_window->second)))
        return false;
    for (std::size_t index = 0; index < left.lines.size(); ++index)
        if (left.lines[index].identity != right.lines[index].identity ||
            !sameBits(left.lines[index].position, right.lines[index].position) ||
            !sameBits(left.lines[index].intensity, right.lines[index].intensity))
            return false;
    for (std::size_t index = 0; index < left.radiation.lines.size(); ++index)
        if (!sameBits(left.radiation.lines[index].wavelength.value, right.radiation.lines[index].wavelength.value) ||
            !sameBits(left.radiation.lines[index].relative_weight, right.radiation.lines[index].relative_weight))
            return false;
    return true;
}
}
struct PreparedProfileTemplate::Data {
    ProfileExpansionInput input;
    std::shared_ptr<const IlTemplate> direct;
    int rounding = std::fegetround();
    unsigned denormals = denormalMode();
};
bool PreparedProfileTemplate::matches(const ProfileExpansionInput &input, const ExecutionContext &context) const noexcept {
    return context.numerical_policy == NumericalPolicy::explicit_float64 &&
        data_->rounding == std::fegetround() && data_->denormals == denormalMode() &&
        sameExpansion(data_->input, input);
}
const ProfileExpansionInput &PreparedProfileTemplate::input() const noexcept { return data_->input; }
Result<PreparedProfileTemplateHandle> PreparedProfileTemplate::prepare(
    const ProfileExpansionInput &input, const ExecutionContext &context, PreparedProfileTemplateHandle previous) {
    if (context.cancellation.stop_requested())
        return Result<PreparedProfileTemplateHandle>{
            Error{ErrorCode::cancelled, "pfsm.expansion", "IL preparation canceled"}};
    if (previous && !previous->matches(input, context))
        previous.reset();
    if (previous && previous->profile())
        return Result<PreparedProfileTemplateHandle>{std::move(previous)};
    detail::countWork(&detail::WorkCounters::template_preparations);
    std::shared_ptr<const Data> data;
    if (previous) {
        data = previous->data_;
    } else {
        auto lines = expandProfileLines(input, context);
        if (!lines)
            return Result<PreparedProfileTemplateHandle>{lines.error()};
        auto prepared = std::make_shared<Data>();
        prepared->input = input;
        prepared->direct = std::make_shared<const IlTemplate>(std::move(lines).value(), input.fwhm_degrees);
        data = std::move(prepared);
    }
    ProfileTemplate profile = data->direct;
    if (input.interpolation_window) {
        auto complete = cached(data->direct, *input.interpolation_window, context);
        if (!complete)
            return Result<PreparedProfileTemplateHandle>{complete.error()};
        profile = std::move(complete).value();
    }
    return Result<PreparedProfileTemplateHandle>{
        PreparedProfileTemplateHandle(new PreparedProfileTemplate(std::move(data), std::move(profile)))};
}
std::size_t PreparedProfileTemplate::storageBytes() const noexcept {
    std::size_t bytes = sizeof(*this) + sizeof(Data) + 384 +
        data_->input.lines.capacity() * sizeof(ProfileLine) +
        data_->input.radiation.lines.capacity() * sizeof(RadiationLine) + data_->direct->storageBytes();
    if (const auto *interpolated = dynamic_cast<const CachedIlTemplate *>(profile_.get()))
        bytes += interpolated->storageBytes() + 128;
    return bytes;
}
PreparedProfileTemplateHandle PreparedProfileTemplate::expandedOnly() const {
    return PreparedProfileTemplateHandle(new PreparedProfileTemplate(data_, {}));
}
std::size_t PreparedProfileTemplate::storageBound(const ProfileExpansionInput &input, bool coefficients) noexcept {
    return storageBoundForLineCount(input, input.lines.size(), coefficients);
}
std::size_t PreparedProfileTemplate::storageBoundForLineCount(const ProfileExpansionInput &input,
                                                             std::size_t lineCount, bool coefficients) noexcept {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    const auto components = input.coordinates == ProfileExpansionInput::Coordinates::d_spacing_angstrom
        ? input.radiation.lines.size() : std::size_t{1};
    if (components >= maximum / (2 * sizeof(ProfileLine)) ||
        lineCount > maximum / (sizeof(ProfileLine) * (1 + 2 * components)))
        return maximum;
    const auto lineBytes = lineCount * sizeof(ProfileLine) * (1 + 2 * components);
    if (input.radiation.lines.size() > (maximum - lineBytes) / sizeof(RadiationLine))
        return maximum;
    std::size_t bytes = lineBytes + input.radiation.lines.size() * sizeof(RadiationLine);
    const std::size_t overhead = sizeof(PreparedProfileTemplate) + sizeof(Data) + sizeof(IlTemplate) + 384;
    if (bytes > maximum - overhead)
        return maximum;
    bytes += overhead;
    if (coefficients && input.interpolation_window) {
        if (!std::isfinite(input.fwhm_degrees) || input.fwhm_degrees <= 0)
            return maximum;
        const double count = std::ceil((input.interpolation_window->second - input.interpolation_window->first) /
                                       (.5 * input.fwhm_degrees));
        if (std::isfinite(count) && count >= 1 && count <= 16384) {
            const auto extra = sizeof(CachedIlTemplate) + 128 +
                static_cast<std::size_t>(count) * sizeof(CachedIlTemplate::Coefficients);
            if (bytes > maximum - extra)
                return maximum;
            bytes += extra;
        }
    }
    return bytes;
}
} // namespace qualpf::profile
