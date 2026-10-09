#include "rounded_transcendentals.h"
#include <algorithm>
#include <bit>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <cstdint>
#include <limits>

namespace qualpf::crystallography::detail {
namespace {
using I = boost::multiprecision::cpp_int;
using R = boost::rational<I>;
using U = std::uint64_t;
constexpr U sign = U{1} << 63;
constexpr U infinity = 0x7ff0000000000000ULL;
constexpr U nan = 0x7ff8000000000000ULL;
constexpr U one = 0x3ff0000000000000ULL;
constexpr U fraction_mask = (U{1} << 52) - 1;
constexpr unsigned precision = 320;
constexpr unsigned reduction = 14;
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53);

struct Fixed { I lo, hi; };
struct Bounds { R lo, hi; };
I ceiling(const I& n, const I& d) { return (n + d - 1) / d; }
I floor_signed(const I& n, const I& d) {
  return n >= 0 ? I(n / d) : I(-ceiling(-n, d));
}
I ceil_signed(const I& n, const I& d) { return -floor_signed(-n, d); }

R exact(U bits) {
  const U magnitude = bits & ~sign;
  const unsigned exponent = unsigned(magnitude >> 52);
  I m = magnitude & fraction_mask;
  if (exponent) m += I(1) << 52;
  if (bits & sign) m = -m;
  const int power = exponent ? int(exponent) - 1075 : -1074;
  return power >= 0 ? R(I(m << power)) : R(m, I(1) << -power);
}

U pack(I q, int exponent) {
  U m = q.convert_to<U>();
  if (m >= (U{1} << 53)) { m >>= 1; ++exponent; }
  if (exponent > 1023) return infinity;
  if (exponent < -1022 || m < (U{1} << 52)) return m;
  return (U(exponent + 1023) << 52) | (m & fraction_mask);
}
int clamped_index_difference(const I& n, const I& d) {
  const auto a = boost::multiprecision::msb(n), b = boost::multiprecision::msb(d);
  // Subtract unsigned indices before narrowing. All observable binary64
  // rounding boundaries below lie inside [-4096,4096]; a larger difference
  // needs only its sign to select the proved overflow/underflow early exit.
  if (a >= b) {
    const auto difference = a - b;
    return difference > 4096 ? 4097 : static_cast<int>(difference);
  }
  const auto difference = b - a;
  return difference > 4096 ? -4097 : -static_cast<int>(difference);
}
int binary_exponent(const I& n, const I& d, int e) {
  if (e >= 0 ? n < (d << e) : (n << -e) < d) --e;
  return e;
}
U round_rational(const R& value) {
  I n = value.numerator(), d = value.denominator();
  const U s = n < 0 ? sign : 0;
  if (n < 0) n = -n;
  if (!n) return s;
  const int difference = clamped_index_difference(n, d);
  // floor(log2(n/d)) is difference or difference-1. Guard before any
  // exponent-dependent shift; values below half a subnormal round to zero.
  if (difference >= 1025) return s | infinity;
  if (difference <= -1076) return s;
  const int e = binary_exponent(n, d, difference);
  const int quantum = std::max(e - 52, -1074);
  if (quantum < 0) n <<= -quantum; else d <<= quantum;
  I q = n / d;
  const I remainder = n - q * d;
  if (2 * remainder > d || (2 * remainder == d && bool(q & 1))) ++q;
  return s | pack(q, e);
}
bool rounded(const Bounds& x, U& result) {
  const U a = round_rational(x.lo), b = round_rational(x.hi);
  result = a;
  return a == b;
}
Bounds divide(const Bounds& a, const Bounds& b) {
  // b is a certified positive logarithm. Four corners handle either sign of a.
  const R q[] = {a.lo / b.lo, a.lo / b.hi, a.hi / b.lo, a.hi / b.hi};
  return {*std::min_element(q, q + 4), *std::max_element(q, q + 4)};
}

Fixed atanh_fixed(const I& numerator, const I& denominator) {
  const I scale = I(1) << precision;
  I pl = numerator * scale / denominator;
  I ph = ceiling(numerator * scale, denominator);
  const I square_lo = pl * pl / scale;
  const I square_hi = ceiling(ph * ph, scale);
  I lo = pl, hi = ph;
  for (unsigned odd = 3;; odd += 2) {
    pl = pl * square_lo / scale;
    ph = ceiling(ph * square_hi, scale);
    // The next positive tail is <= next_power/(odd*(1-t^2)).
    // All queried t <=1/3, so 1/(1-t^2) <=9/8.
    if (ph <= 1) return {I(2 * lo), I(2 * (hi + ceiling(9 * ph, I(8) * odd)))};
    lo += pl / odd;
    hi += ceiling(ph, I(odd));
  }
}
struct LogConstants { Fixed two, ten; };
const LogConstants& constants() {
  static const LogConstants c = [] {
    const Fixed two = atanh_fixed(I(1), I(3));
    const Fixed five_fourths = atanh_fixed(I(1), I(9));
    return LogConstants{two, {I(five_fourths.lo + 3 * two.lo),
                             I(five_fourths.hi + 3 * two.hi)}};
  }();
  return c;
}
struct AtanhSeries {
  R t, square, power, sum;
  I odd = 1;
  explicit AtanhSeries(R value) : t(value), square(t * t), power(t), sum(t) {}
  void advance() { odd += 2; power *= square; sum += power / R(odd); }
  Bounds bounds() const {
    const R tail = power * square / (R(I(odd + 2)) * (R(I(1)) - square));
    return {R(I(2)) * sum, R(I(2)) * (sum + tail)};
  }
};
Bounds log_combination(const Bounds& m, const Bounds& two, int k) {
  return k >= 0 ? Bounds{m.lo + R(I(k)) * two.lo, m.hi + R(I(k)) * two.hi}
                : Bounds{m.lo + R(I(k)) * two.hi, m.hi + R(I(k)) * two.lo};
}
U log_fallback(const R& mantissa, int k) {
  AtanhSeries m((mantissa - R(I(1))) / (mantissa + R(I(1))));
  AtanhSeries two(R(I(1), I(3))), five_fourths(R(I(1), I(9)));
  for (;;) {
    const Bounds l2 = two.bounds(), l54 = five_fourths.bounds();
    const Bounds l10{l54.lo + R(I(3)) * l2.lo, l54.hi + R(I(3)) * l2.hi};
    U result;
    if (rounded(divide(log_combination(m.bounds(), l2, k), l10), result)) return result;
    m.advance(); two.advance(); five_fourths.advance();
  }
}

Bounds exp_fixed(const R& argument) {
  const bool negative = argument < R(I(0));
  const R a = negative ? -argument : argument;
  const I scale = I(1) << precision;
  const I denominator = a.denominator() << reduction;
  const I yl = a.numerator() * scale / denominator;
  const I yh = ceiling(a.numerator() * scale, denominator);
  I tl = scale, th = scale, lo = scale, hi = scale;
  for (unsigned n = 1;; ++n) {
    tl = tl * yl / (scale * n);
    th = ceiling(th * yh, I(scale * n));
    lo += tl; hi += th;
    const I next = ceiling(th * yh, I(scale * (n + 1)));
    // |argument|<1024, y<1/16. All later ratios <=1/16,
    // hence a remaining next term <=1 unit has tail <2 units.
    if (next <= 1) { hi += 2; break; }
  }
  for (unsigned n = 0; n < reduction; ++n) {
    lo = lo * lo / scale;
    hi = ceiling(hi * hi, scale);
  }
  return negative ? Bounds{R(scale, hi), R(scale, lo)}
                  : Bounds{R(lo, scale), R(hi, scale)};
}
U exp_fallback(const R& argument) {
  if (!argument.numerator()) return one;
  const bool negative = argument < R(I(0));
  const R y = negative ? -argument : argument;
  R term(I(1)), sum(I(1));
  for (I n = 1;; ++n) {
    term *= y / R(n); sum += term;
    // The unreduced exact-rational series avoids denominator growth from
    // fourteen rational squarings. Its geometric tail applies after n+2>y.
    if (R(I(n + 2)) <= y) continue;
    const R next = term * y / R(I(n + 1));
    const R tail = next / (R(I(1)) - y / R(I(n + 2)));
    R lo = sum, hi = sum + tail;
    const Bounds bounds = negative ? Bounds{R(I(1)) / hi, R(I(1)) / lo} : Bounds{lo, hi};
    U result;
    if (rounded(bounds, result)) return result;
  }
}
U exp_exact(const R& argument) {
  U result;
  return rounded(exp_fixed(argument), result) ? result : exp_fallback(argument);
}
U pow_fallback(const R& x) {
  AtanhSeries two(R(I(1), I(3))), five_fourths(R(I(1), I(9)));
  for (;;) {
    const Bounds a = two.bounds(), b = five_fourths.bounds();
    R lo = x * (b.lo + R(I(3)) * a.lo), hi = x * (b.hi + R(I(3)) * a.hi);
    if (hi < lo) std::swap(lo, hi);
    const U l = exp_exact(lo), h = exp_exact(hi);
    if (l == h) return l;
    two.advance(); five_fourths.advance();
  }
}
} // namespace

double rounded_log10(double x, bool force_rational) {
  const U bits = std::bit_cast<U>(x), magnitude = bits & ~sign;
  if (!magnitude) return std::bit_cast<double>(sign | infinity);
  if ((bits & sign) || magnitude > infinity) return std::bit_cast<double>(nan);
  if (magnitude == infinity) return x;
  const R input = exact(bits);
  if (input.denominator() == 1) {
    I integer = input.numerator(); unsigned power = 0;
    while (integer > 1 && integer % 10 == 0) { integer /= 10; ++power; }
    if (integer == 1) return double(power); // every representable exact10^k
  }
  const unsigned exponent = unsigned(magnitude >> 52);
  I m = magnitude & fraction_mask;
  int k;
  if (exponent) { m += I(1) << 52; k = int(exponent) - 1023; }
  else {
    const int top = int(boost::multiprecision::msb(m));
    m <<= 52 - top; k = top - 1074;
  }
  const I scale = I(1) << precision;
  const Fixed l = atanh_fixed(I(m - (I(1) << 52)), I(m + (I(1) << 52)));
  const auto& c = constants();
  const Bounds lm{R(l.lo, scale), R(l.hi, scale)}, l2{R(c.two.lo, scale), R(c.two.hi, scale)};
  U result;
  if (!force_rational && rounded(divide(log_combination(lm, l2, k),
                     {R(c.ten.lo, scale), R(c.ten.hi, scale)}), result))
    return std::bit_cast<double>(result);
  return std::bit_cast<double>(log_fallback(R(m, I(1) << 52), k));
}

double rounded_exp(double x, bool force_rational) {
  const U bits = std::bit_cast<U>(x), magnitude = bits & ~sign;
  if (magnitude > infinity) return std::bit_cast<double>(nan);
  if (magnitude == infinity) return bits & sign ? 0.0 : x;
  if (!(bits & sign) && magnitude >= std::bit_cast<U>(710.0)) return std::bit_cast<double>(infinity);
  if ((bits & sign) && magnitude >= std::bit_cast<U>(746.0)) return 0.0;
  if (!magnitude) return 1.0;
  return std::bit_cast<double>(force_rational ? exp_fallback(exact(bits)) : exp_exact(exact(bits)));
}

double rounded_pow10(double x, bool force_rational) {
  const U bits = std::bit_cast<U>(x), magnitude = bits & ~sign;
  if (magnitude > infinity) return std::bit_cast<double>(nan);
  if (magnitude == infinity) return bits & sign ? 0.0 : x;
  if (!(bits & sign) && magnitude >= std::bit_cast<U>(309.0)) return std::bit_cast<double>(infinity);
  if ((bits & sign) && magnitude >= std::bit_cast<U>(324.0)) return 0.0;
  const R input = exact(bits);
  if (input.denominator() == 1) {
    const int n = input.numerator().convert_to<int>();
    I power = 1;
    for (int i = 0; i < (n < 0 ? -n : n); ++i) power *= 10;
    return std::bit_cast<double>(round_rational(n < 0 ? R(I(1), power) : R(power)));
  }
  if (force_rational) return std::bit_cast<double>(pow_fallback(input));
  const I scale = I(1) << precision;
  const Fixed& c = constants().ten;
  R lo = input * R(c.lo, scale), hi = input * R(c.hi, scale);
  if (hi < lo) std::swap(lo, hi);
  const U l = round_rational(exp_fixed(lo).lo), h = round_rational(exp_fixed(hi).hi);
  return std::bit_cast<double>(l == h ? l : pow_fallback(input));
}

double rounded_magnitude(double real, double imaginary) {
  const U a = std::bit_cast<U>(real) & ~sign, b = std::bit_cast<U>(imaginary) & ~sign;
  if (a == infinity || b == infinity) return std::bit_cast<double>(infinity);
  if (a > infinity || b > infinity) return std::bit_cast<double>(nan);
  const R r = exact(a), i = exact(b), square = r * r + i * i;
  if (!square.numerator()) return 0.0;
  I n = square.numerator(), d = square.denominator();
  const int difference = clamped_index_difference(n, d);
  // For finite binary64 components the squared exponent is bounded already;
  // these safe saturation guards also keep the conversion and shifts bounded.
  if (difference >= 2050) return std::bit_cast<double>(infinity);
  if (difference <= -2151) return 0.0;
  const int e2 = binary_exponent(n, d, difference);
  const int e = e2 >= 0 ? e2 / 2 : -((-e2 + 1) / 2);
  const int quantum = std::max(e - 52, -1074);
  if (quantum < 0) n <<= -2 * quantum; else d <<= 2 * quantum;
  const I floor_square = n / d;
  I q = boost::multiprecision::sqrt(floor_square);
  const I midpoint = 2 * q + 1;
  const I comparison = 4 * n - d * midpoint * midpoint;
  if (comparison > 0 || (comparison == 0 && bool(q & 1))) ++q;
  return std::bit_cast<double>(pack(q, e));
}
} // namespace qualpf::crystallography::detail
