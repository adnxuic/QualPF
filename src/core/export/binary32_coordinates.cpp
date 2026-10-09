#include "qualpf/export/binary32_coordinates.h"
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>

namespace qualpf::exporting {
namespace {
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53);
constexpr std::uint32_t sign_mask = 0x80000000u, fraction_mask = 0x007fffffu;
// Frozen REAL parameters: pi=4*atan(1), rtod=180/pi, dtor=1/rtod.
// The parameter is a binary32 bit pattern, not a new binary64 pi/180 formula.
constexpr std::uint32_t native_dtor = 0x3c8efa36u;

struct Dyadic {
    std::uint64_t significand;
    int power;
};
Dyadic decode(std::uint32_t bits) noexcept {
    const int exponent = static_cast<int>((bits >> 23) & 255u);
    return {exponent ? (bits & fraction_mask) | 0x00800000u : bits & fraction_mask, exponent ? exponent - 150 : -149};
}
std::uint64_t rounded_shift(std::uint64_t value, unsigned shift) noexcept {
    if (shift == 0)
        return value;
    if (shift >= 64)
        return 0; // All callers have at most 53 significant bits.
    const auto quotient = value >> shift;
    const auto remainder = value & ((std::uint64_t{1} << shift) - 1);
    const auto half = std::uint64_t{1} << (shift - 1);
    return quotient + (remainder > half || (remainder == half && (quotient & 1)));
}
std::uint32_t pack(std::uint64_t significand, int exponent) noexcept {
    if (significand == 0)
        return 0;
    if (exponent < -126)
        exponent = -126;
    if (significand >= 0x01000000u) {
        significand >>= 1;
        ++exponent;
    }
    if (exponent > 127)
        return 0x7f800000u;
    if (exponent == -126 && significand < 0x00800000u)
        return static_cast<std::uint32_t>(significand);
    return static_cast<std::uint32_t>((exponent + 127) << 23) |
           (static_cast<std::uint32_t>(significand) & fraction_mask);
}
std::uint32_t round_dyadic(std::uint64_t significand, int power) noexcept {
    if (!significand)
        return 0;
    const int exponent = static_cast<int>(std::bit_width(significand)) - 1 + power;
    const int quantum = exponent < -126 ? -149 : exponent - 23;
    const int shift = power - quantum;
    const auto rounded = shift >= 0 ? significand << shift : rounded_shift(significand, -shift);
    return pack(rounded, exponent);
}
std::uint32_t narrow(double value) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto sign = static_cast<std::uint32_t>(bits >> 32) & sign_mask;
    const int exponent = static_cast<int>((bits >> 52) & 2047u);
    const auto fraction = bits & 0x000fffffffffffffull;
    if (exponent == 2047)
        return sign | (fraction ? 0x7fc00000u : 0x7f800000u);
    return sign |
           round_dyadic(exponent ? fraction | 0x0010000000000000ull : fraction, exponent ? exponent - 1075 : -1074);
}
double widen(std::uint32_t bits) noexcept {
    const auto sign = std::uint64_t{bits & sign_mask} << 32;
    const auto exponent = (bits >> 23) & 255u;
    const auto fraction = bits & fraction_mask;
    if (exponent == 255)
        return std::bit_cast<double>(sign | (fraction ? 0x7ff8000000000000ull : 0x7ff0000000000000ull));
    if (exponent)
        return std::bit_cast<double>(sign | (std::uint64_t{exponent + 896} << 52) | (std::uint64_t{fraction} << 29));
    if (!fraction)
        return std::bit_cast<double>(sign);
    const auto top = std::bit_width(fraction) - 1;
    return std::bit_cast<double>(sign | (static_cast<std::uint64_t>(top + 874) << 52) |
                                 ((std::uint64_t{fraction} << (52 - top)) & 0x000fffffffffffffull));
}
std::uint32_t multiply(std::uint32_t left, std::uint32_t right) noexcept {
    const auto a = decode(left), b = decode(right);
    return ((left ^ right) & sign_mask) | round_dyadic(a.significand * b.significand, a.power + b.power);
}
std::uint32_t divide(std::uint32_t numerator, std::uint32_t denominator) noexcept {
    const auto sign = (numerator ^ denominator) & sign_mask;
    const auto a = decode(numerator), b = decode(denominator);
    if (!b.significand)
        return a.significand ? sign | 0x7f800000u : 0x7fc00000u;
    if (!a.significand)
        return sign;
    const int difference =
        static_cast<int>(std::bit_width(a.significand)) - static_cast<int>(std::bit_width(b.significand));
    int ratio_exponent = difference;
    if (difference >= 0 ? a.significand < (b.significand << difference)
                        : (a.significand << -difference) < b.significand)
        --ratio_exponent;
    const int exponent = a.power - b.power + ratio_exponent;
    if (exponent < -150)
        return sign;
    if (exponent > 127)
        return sign | 0x7f800000u;
    const int quantum = exponent < -126 ? -149 : exponent - 23;
    const int shift = a.power - b.power - quantum;
    // Normalized operands guarantee both shifted integers fit in 49 bits.
    const auto top = shift >= 0 ? a.significand << shift : a.significand;
    const auto bottom = shift >= 0 ? b.significand : b.significand << -shift;
    const auto quotient = top / bottom, remainder = top % bottom;
    const auto rounded = quotient + (2 * remainder > bottom || (2 * remainder == bottom && (quotient & 1)));
    return sign | pack(rounded, exponent);
}

// Nonnegative fixed-point integers with P fractional bits and 64 integer bits.
// The interval operations round outward using only standard uint32/uint64 math.
template <unsigned FractionWords, unsigned IntegerWords = 2> struct Fixed {
    static constexpr unsigned size = FractionWords + IntegerWords, precision = FractionWords * 32;
    std::array<std::uint32_t, size> words{};
    bool less(const Fixed &other) const noexcept {
        for (unsigned i = size; i-- > 0;)
            if (words[i] != other.words[i])
                return words[i] < other.words[i];
        return false;
    }
    int top() const noexcept {
        for (unsigned i = size; i-- > 0;)
            if (words[i])
                return static_cast<int>(i * 32 + std::bit_width(words[i]) - 1);
        return -1;
    }
    Fixed shifted(unsigned distance) const noexcept {
        Fixed result;
        for (unsigned i = 0; i < size * 32; ++i)
            if (bit(i) && i + distance < size * 32)
                result.words[(i + distance) / 32] |= std::uint32_t{1} << ((i + distance) % 32);
        return result;
    }
    void add(const Fixed &other) noexcept {
        std::uint64_t carry = 0;
        for (unsigned i = 0; i < size; ++i) {
            carry += std::uint64_t{words[i]} + other.words[i];
            words[i] = static_cast<std::uint32_t>(carry);
            carry >>= 32;
        }
    }
    void subtract(const Fixed &other) noexcept {
        std::uint64_t borrow = 0;
        for (unsigned i = 0; i < size; ++i) {
            const auto part = std::uint64_t{other.words[i]} + borrow;
            borrow = std::uint64_t{words[i]} < part;
            words[i] = static_cast<std::uint32_t>(std::uint64_t{words[i]} - part);
        }
    }
    void increment() noexcept {
        for (auto &word : words)
            if (++word)
                break;
    }
    bool at_most_one() const noexcept {
        if (words[0] > 1)
            return false;
        for (unsigned i = 1; i < size; ++i)
            if (words[i])
                return false;
        return true;
    }
    static Fixed from_binary32(std::uint32_t bits) noexcept {
        Fixed result;
        const auto a = decode(bits);
        const unsigned shift = static_cast<unsigned>(a.power + precision);
        const auto shifted = a.significand << (shift % 32);
        result.words[shift / 32] = static_cast<std::uint32_t>(shifted);
        result.words[shift / 32 + 1] = static_cast<std::uint32_t>(shifted >> 32);
        return result;
    }
    static Fixed product(const Fixed &left, const Fixed &right, bool upward) noexcept {
        std::array<std::uint32_t, 2 * size> full{};
        for (unsigned i = 0; i < size; ++i) {
            std::uint64_t carry = 0;
            for (unsigned j = 0; j < size; ++j) {
                const auto value = std::uint64_t{left.words[i]} * right.words[j] + full[i + j] + carry;
                full[i + j] = static_cast<std::uint32_t>(value);
                carry = value >> 32;
            }
            full[i + size] = static_cast<std::uint32_t>(carry);
        }
        Fixed result;
        for (unsigned i = 0; i < size; ++i)
            result.words[i] = full[i + FractionWords];
        if (upward) {
            bool discarded = false;
            for (unsigned i = 0; i < FractionWords; ++i)
                discarded |= full[i] != 0;
            if (discarded)
                result.increment();
        }
        return result;
    }
    Fixed quotient(std::uint32_t divisor, bool upward) const noexcept {
        Fixed result;
        std::uint64_t remainder = 0;
        for (unsigned i = size; i-- > 0;) {
            const auto value = (remainder << 32) | words[i];
            result.words[i] = static_cast<std::uint32_t>(value / divisor);
            remainder = value % divisor;
        }
        if (upward && remainder)
            result.increment();
        return result;
    }
    bool bit(unsigned at) const noexcept { return (words[at / 32] >> (at % 32)) & 1u; }
    std::uint32_t rounded_binary32() const noexcept {
        int high = static_cast<int>(size) - 1;
        while (high >= 0 && !words[high])
            --high;
        if (high < 0)
            return 0;
        const int top = high * 32 + static_cast<int>(std::bit_width(words[high])) - 1;
        const int exponent = top - static_cast<int>(precision);
        const unsigned shift = exponent < -126 ? precision - 149 : static_cast<unsigned>(top - 23);
        std::uint64_t significant = 0;
        for (int i = top; i >= static_cast<int>(shift); --i)
            significant = (significant << 1) | bit(i);
        const bool guard = bit(shift - 1);
        bool sticky = false;
        for (unsigned i = 0; i < (shift - 1) / 32; ++i)
            sticky |= words[i] != 0;
        const unsigned remainder = (shift - 1) % 32;
        sticky |= (words[(shift - 1) / 32] & ((std::uint32_t{1} << remainder) - 1)) != 0;
        significant += guard && (sticky || (significant & 1));
        return pack(significant, exponent);
    }
};

template <unsigned Words> std::optional<std::uint32_t> certified_sine(std::uint32_t angle) noexcept {
    using F = Fixed<Words>;
    const auto x = F::from_binary32(angle);
    const auto square_low = F::product(x, x, false), square_high = F::product(x, x, true);
    auto term_low = x, term_high = x, sum_low = x, sum_high = x;
    // 0 < angle <= the transported half-angle at 180 degrees (< 1.571).
    // Terms decrease from n=0. Outward recurrence encloses every exact term.
    for (unsigned n = 1; n <= F::precision; ++n) {
        const auto divisor = (2 * n) * (2 * n + 1);
        term_low = F::product(term_low, square_low, false).quotient(divisor, false);
        term_high = F::product(term_high, square_high, true).quotient(divisor, true);
        if (n & 1) {
            sum_low.subtract(term_high);
            sum_high.subtract(term_low);
        } else {
            sum_low.add(term_low);
            sum_high.add(term_high);
        }
        if (term_high.at_most_one()) {
            // Alternating-series remainder lies between zero and the next
            // term, whose magnitude is bounded by the last term's upper bound.
            if (n & 1)
                sum_high.add(term_high);
            else
                sum_low.subtract(term_high);
            const auto lower = sum_low.rounded_binary32(), upper = sum_high.rounded_binary32();
            return lower == upper ? std::optional{lower} : std::nullopt;
        }
    }
    return std::nullopt;
}

// Only the newly reviewed angle domain uses argument reduction. The original
// B1 path above remains the certificate for 0 < transported two-theta <= 180.
template <unsigned Words> std::optional<std::uint32_t> certified_general_sine(std::uint32_t angle) noexcept {
    using F = Fixed<Words, 5>; // 160 integer bits hold every finite binary32 half-angle.
    constexpr const char *pi_fraction = Words == 8 ? "243f6a8885a308d313198a2e03707344a4093822299f31d0082efa98ec4e6c89"
                                                   : "243f6a8885a308d313198a2e03707344a4093822299f31d0082efa98ec4e6c89"
                                                     "452821e638d01377be5466cf34e90c6cc0ac29b7c97c50dd3f84d5b5b5470917";
    F pi;
    pi.words[Words] = 3;
    for (unsigned i = 0; i < Words * 8; ++i) {
        const char c = pi_fraction[i];
        const auto digit = static_cast<std::uint32_t>(c <= '9' ? c - '0' : c - 'a' + 10);
        const unsigned shift = (Words * 8 - i - 1) * 4;
        pi.words[shift / 32] |= digit << (shift % 32);
    }
    const auto period = pi.shifted(1); // floor(pi * 2^P) * 2, below exact 2*pi.
    auto x = F::from_binary32(angle & ~sign_mask);
    const int original_top = x.top();
    bool reduced = false;
    for (int shift = x.top() - period.top(); shift >= 0; --shift) {
        const auto divisor = period.shifted(static_cast<unsigned>(shift));
        if (!x.less(divisor)) {
            x.subtract(divisor);
            reduced = true;
        }
    }
    F reduction_error;
    if (reduced) {
        // q < |half_angle| < 2^(e+1), period error < 2^(1-P).
        // |sin(r)-sin(original)| <= q*period_error < 2^(e+2-P).
        const unsigned at = static_cast<unsigned>(original_top - static_cast<int>(F::precision) + 2);
        reduction_error.words[at / 32] = std::uint32_t{1} << (at % 32);
    }
    const auto square_low = F::product(x, x, false), square_high = F::product(x, x, true);
    auto term_low = x, term_high = x, positive_low = x, positive_high = x;
    F negative_low, negative_high;
    for (unsigned n = 1; n <= F::precision; ++n) {
        const auto divisor = (2 * n) * (2 * n + 1);
        term_low = F::product(term_low, square_low, false).quotient(divisor, false);
        term_high = F::product(term_high, square_high, true).quotient(divisor, true);
        if (n & 1) {
            negative_low.add(term_low);
            negative_high.add(term_high);
        } else {
            positive_low.add(term_low);
            positive_high.add(term_high);
        }
        // For r < 2*pi, terms decrease for n >= 3. Keeping positive/negative
        // totals separately avoids unsigned cancellation before convergence.
        if (n >= 3 && term_high.at_most_one()) {
            negative_high.add(term_high);
            negative_high.add(reduction_error);
            positive_high.add(term_high);
            positive_high.add(reduction_error);
            auto rounded_difference = [](F positive, F negative) {
                if (positive.less(negative)) {
                    negative.subtract(positive);
                    return sign_mask | negative.rounded_binary32();
                }
                positive.subtract(negative);
                return positive.rounded_binary32();
            };
            const auto low = rounded_difference(positive_low, negative_high);
            const auto high = rounded_difference(positive_high, negative_low);
            return low == high ? std::optional{low ^ (angle & sign_mask)} : std::nullopt;
        }
    }
    return std::nullopt;
}
Error invalid(const char *field, const char *reason) {
    return {ErrorCode::invalid_input, field, "Cannot resolve export binary32 coordinate", reason};
}
} // namespace

double export_binary32_transport(double value) noexcept { return widen(narrow(value)); }

Result<Binary32ExportCoordinate> evaluate_export_binary32_d(TwoThetaDegrees angle, WavelengthAngstrom wavelength) {
    Binary32ExportCoordinate result;
    result.two_theta_bits = narrow(angle.value);
    result.wavelength_bits = narrow(wavelength.value);
    const double transported_angle = widen(result.two_theta_bits), transported_wave = widen(result.wavelength_bits);
    if (!std::isfinite(transported_angle))
        return Result<Binary32ExportCoordinate>{invalid("export.d_spacing", "invalid_coordinate")};
    if (!std::isfinite(transported_wave))
        return Result<Binary32ExportCoordinate>{invalid("export.wavelength", "invalid_wavelength")};
    result.radians_bits = multiply(native_dtor, result.two_theta_bits);
    result.half_angle_bits = multiply(result.radians_bits, 0x3f000000u);
    const auto magnitude = result.half_angle_bits & ~sign_mask;
    const bool original_b1_path = magnitude <= 0x3fc90fdbu;
    auto sine = !magnitude         ? std::optional{result.half_angle_bits}
                : original_b1_path ? certified_sine<8>(magnitude)
                                   : certified_general_sine<8>(result.half_angle_bits);
    if (magnitude && original_b1_path && sine)
        *sine ^= result.half_angle_bits & sign_mask;
    result.sine_precision_bits = 256;
    if (!sine) {
        sine = original_b1_path ? certified_sine<16>(magnitude) : certified_general_sine<16>(result.half_angle_bits);
        if (original_b1_path && sine)
            *sine ^= result.half_angle_bits & sign_mask;
        result.sine_precision_bits = 512;
    }
    if (!sine)
        return Result<Binary32ExportCoordinate>{invalid("export.d_spacing", "rounding_not_certified")};
    result.sine_bits = *sine;
    result.denominator_bits = multiply(0x40000000u, result.sine_bits);
    result.d_bits = divide(result.wavelength_bits, result.denominator_bits);
    result.d_angstrom = widen(result.d_bits);
    return Result<Binary32ExportCoordinate>{result};
}

Result<Binary32ExportCoordinate> resolve_export_binary32_d(TwoThetaDegrees angle, WavelengthAngstrom wavelength) {
    const double transported_wave = export_binary32_transport(wavelength.value);
    if (!std::isfinite(transported_wave) || transported_wave <= 0)
        return Result<Binary32ExportCoordinate>{invalid("export.wavelength", "invalid_wavelength")};
    auto result = evaluate_export_binary32_d(angle, wavelength);
    if (!result)
        return result;
    if (!std::isfinite(result.value().d_angstrom))
        return Result<Binary32ExportCoordinate>{invalid("export.d_spacing", "nonfinite_d_spacing")};
    if (result.value().d_angstrom <= 0)
        return Result<Binary32ExportCoordinate>{invalid("export.d_spacing", "nonpositive_d_spacing")};
    return result;
}
} // namespace qualpf::exporting
