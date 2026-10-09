// Binary32 compatibility retains the frozen Phase25A.1 arithmetic and grammar.
// Its certified integer enclosure also owns the current binary64 asin seam.
#include "binary32.h"
#include <algorithm>
#include <bit>
#include <boost/multiprecision/cpp_int.hpp>
#include <limits>
#include <new>
#include <stdexcept>
namespace qualpf::io::powder_cif::binary32 {
using boost::multiprecision::cpp_int;
using I = cpp_int;
constexpr U signMask = 0x80000000u, inf = 0x7f800000u, nanBits = 0x7fc00000u;
struct Q {
  I n, d;
};
I ceilQ(const I &n, const I &d) { return (n + d - 1) / d; }
Q exact(U bits) {
  unsigned e = (bits >> 23) & 255, m = bits & 0x7fffff;
  if (e == 255)
    throw std::runtime_error("nonfinite");
  if (e)
    m |= 0x800000;
  int power = e ? int(e) - 150 : -149;
  I n = m, d = 1;
  if (power >= 0)
    n <<= power;
  else
    d <<= -power;
  if (bits & signMask)
    n = -n;
  return {n, d};
}
U rounded(I n, I d, bool negativeZero = false) {
  U sign = n < 0 || (!n && negativeZero) ? signMask : 0;
  if (n < 0)
    n = -n;
  if (!n)
    return sign;
  int e =
      int(boost::multiprecision::msb(n)) - int(boost::multiprecision::msb(d));
  if (e >= 0 ? n < (d << e) : (n << -e) < d)
    --e;
  int quantum = std::max(e - 23, -149);
  if (quantum >= 0)
    d <<= quantum;
  else
    n <<= -quantum;
  I quotient = n / d, remainder = n % d;
  if (2 * remainder > d || (2 * remainder == d && bool(quotient & 1)))
    ++quotient;
  U m = quotient.convert_to<U>();
  if (m >= 0x1000000u) {
    m >>= 1;
    ++e;
  }
  if (e > 127)
    return sign | inf;
  return sign | (e < -126 ? m : (U(e + 127) << 23) | (m & 0x7fffff));
}
bool special(U a) { return (a & inf) == inf; }
bool isNan(U a) { return special(a) && (a & 0x7fffff); }
U add(U a, U b) {
  if (isNan(a) || isNan(b))
    return nanBits;
  if (special(a) || special(b))
    return special(a) && special(b) && ((a ^ b) & signMask) ? nanBits
           : special(a)                                     ? a
                                                            : b;
  auto x = exact(a), y = exact(b);
  return rounded(x.n * y.d + y.n * x.d, x.d * y.d,
                 a == signMask && b == signMask);
}
U mul(U a, U b) {
  if (isNan(a) || isNan(b))
    return nanBits;
  U sign = (a ^ b) & signMask;
  if (special(a) || special(b))
    return !(a & ~signMask) || !(b & ~signMask) ? nanBits : sign | inf;
  auto x = exact(a), y = exact(b);
  return rounded(x.n * y.n, x.d * y.d, sign != 0);
}
U divide(U a, U b) {
  if (isNan(a) || isNan(b))
    return nanBits;
  U sign = (a ^ b) & signMask;
  if (special(a))
    return special(b) ? nanBits : sign | inf;
  if (special(b))
    return sign;
  if (!(b & ~signMask))
    return (a & ~signMask) ? sign | inf : nanBits;
  auto x = exact(a), y = exact(b);
  I n = x.n * y.d, d = x.d * y.n;
  if (d < 0) {
    n = -n;
    d = -d;
  }
  return rounded(n, d, sign != 0);
}
U power10(int exponent) {
  unsigned e = unsigned(exponent < 0 ? -exponent : exponent);
  U base = 0x41200000u, value = 0x3f800000u;
  while (e) {
    if (e & 1)
      value = mul(value, base);
    e >>= 1;
    if (e)
      base = mul(base, base);
  }
  return exponent < 0 ? divide(0x3f800000u, value) : value;
}
U decode(const std::string &token) {
  std::size_t pos = 0;
  const auto digit = [&] {
    return pos < token.size() && token[pos] >= '0' && token[pos] <= '9';
  };
  const bool negativeMantissa = !token.empty() && token.front() == '-';
  if (pos < token.size() && (token[pos] == '+' || token[pos] == '-'))
    ++pos;
  const auto mantissaStart = pos;
  std::size_t digits = 0;
  while (digit()) {
    ++digits;
    ++pos;
  }
  if (pos < token.size() && token[pos] == '.') {
    ++pos;
    while (digit()) {
      ++digits;
      ++pos;
    }
  }
  if (!digits)
    throw std::runtime_error("numeric_grammar");
  const auto mantissaEnd = pos;
  int exponent = 0;
  bool negativeExponent = false;
  if (pos < token.size() && (token[pos] == 'e' || token[pos] == 'E' ||
                             token[pos] == 'd' || token[pos] == 'D')) {
    ++pos;
    if (pos < token.size() && (token[pos] == '+' || token[pos] == '-')) {
      negativeExponent = token[pos] == '-';
      ++pos;
    }
    if (!digit())
      throw std::runtime_error("numeric_grammar");
    while (digit()) {
      // Saturate after the approved bound, allowing any leading zero count.
      exponent = std::min(39, exponent * 10 + int(token[pos] - '0'));
      ++pos;
    }
  }
  if (pos < token.size() && token[pos] == '(') {
    ++pos;
    if (!digit())
      throw std::runtime_error("numeric_grammar");
    while (digit())
      ++pos;
    if (pos == token.size() || token[pos] != ')')
      throw std::runtime_error("numeric_grammar");
    ++pos;
  }
  if (pos != token.size())
    throw std::runtime_error("numeric_grammar");
  // The complete token is syntax checked before decoder-domain rejection.
  if (digits > 38 || exponent > 38)
    throw std::runtime_error("decoder_bound");
  const int e = negativeExponent ? -exponent : exponent;
  const std::string mant =
      token.substr(mantissaStart, mantissaEnd - mantissaStart);
  U top = 0, bottom = 0x3f800000;
  bool dot = false;
  for (char c : mant) {
    if (c == '.') {
      dot = true;
      continue;
    }
    top = add(mul(0x41200000, top), rounded(c - '0', 1));
    if (dot)
      bottom = mul(0x41200000, bottom);
  }
  U factor = power10(e);
  if (negativeMantissa)
    factor |= signMask;
  return divide(mul(factor, top), bottom);
}
struct Interval {
  I lo, hi;
};
Interval atanInverse(unsigned denominator, unsigned p) {
  const I scale = I(1) << p;
  I power = denominator, lo = 0, hi = 0;
  unsigned n = 0;
  for (;; ++n) {
    I d = power * (2 * n + 1), a = scale / d, b = ceilQ(scale, d);
    if (n & 1) {
      lo -= b;
      hi -= a;
    } else {
      lo += a;
      hi += b;
    }
    power *= denominator * denominator;
    const I tail = ceilQ(scale, power * (2 * n + 3));
    if (tail <= 1) {
      if (n & 1)
        hi += tail;
      else
        lo -= tail;
      break;
    }
  }
  return {lo, hi};
}
Interval piInterval(unsigned p) {
  auto a = atanInverse(5, p), b = atanInverse(239, p);
  return {16 * a.lo - 4 * b.hi, 16 * a.hi - 4 * b.lo};
}
Interval smallAsin(I lo, I hi, unsigned p) {
  const I scale = I(1) << p;
  I squareLo = lo * lo / scale, squareHi = ceilQ(hi * hi, scale);
  I termLo = lo, termHi = hi, sumLo = lo, sumHi = hi;
  const unsigned nmax = p / 2 + 24;
  for (unsigned n = 1; n <= nmax; ++n) {
    I numerator = I(2 * n - 1) * (2 * n - 1),
      denominator = I(2 * n) * (2 * n + 1) * scale;
    termLo = termLo * squareLo * numerator / denominator;
    termHi = ceilQ(termHi * squareHi * numerator, denominator);
    sumLo += termLo;
    sumHi += termHi;
  }
  // Exact positive series ratio < x^2 <= 1/4. The remaining tail
  // is at most term_N/3 (the next term times geometric 4/3).
  sumHi += ceilQ(termHi, 3);
  return {sumLo, sumHi};
}
U certifiedAsin(U bits) {
  U sign = bits & signMask;
  auto q = exact(bits & ~signMask);
  if (q.n > q.d)
    throw std::runtime_error("asin_domain");
  if (!q.n)
    return sign;
  for (unsigned p : {320u, 640u, 1280u}) {
    I scale = I(1) << p;
    I x = q.n * scale / q.d;
    Interval enclosure;
    if (2 * x <= scale)
      enclosure = smallAsin(x, x, p);
    else {
      // asin x = pi/2 - 2 asin sqrt((1-x)/2), monotone endpoints.
      I square = (scale - x) * scale / 2;
      I root = boost::multiprecision::sqrt(square);
      I upper = root + (root * root != square ? 1 : 0);
      auto a = smallAsin(root, upper, p), pi = piInterval(p);
      enclosure = {pi.lo / 2 - 2 * a.hi, ceilQ(pi.hi, 2) - 2 * a.lo};
    }
    U lo = rounded(enclosure.lo, scale), hi = rounded(enclosure.hi, scale);
    if (lo == hi)
      return sign | lo;
  }
  throw std::runtime_error("certificate_not_converged");
}
namespace {
// Only positive normal asin enclosure endpoints enter this rounder. Tiny
// binary64 arguments are handled by the independently proved identity below.
std::uint64_t roundAsin64Endpoint(const I &n, unsigned p) {
  const unsigned highest = boost::multiprecision::msb(n);
  const unsigned shift = highest - 52;
  I significand = n >> shift;
  const I remainder = n - (significand << shift);
  const I midpoint = I(1) << (shift - 1);
  if (remainder > midpoint ||
      (remainder == midpoint && bool(significand & 1)))
    ++significand;
  int exponent = int(highest) - int(p);
  if (significand == (I(1) << 53)) {
    significand >>= 1;
    ++exponent;
  }
  return (std::uint64_t(exponent + 1023) << 52) |
         (significand.convert_to<std::uint64_t>() & 0x000fffffffffffffull);
}
} // namespace
std::uint64_t certifiedAsin64(std::uint64_t bits) {
  constexpr std::uint64_t signMask64 = 0x8000000000000000ull;
  const auto sign = bits & signMask64;
  const auto magnitude = bits & ~signMask64;
  if (magnitude > 0x3ff0000000000000ull)
    return 0x7ff8000000000000ull;
  // asin(x)-x < x*2^-54 <= half-ULP for 0<=x<2^-26, including
  // every subnormal. Bit transport preserves signed zero and ignores FTZ/DAZ.
  if (magnitude < 0x3e50000000000000ull)
    return bits;
  const unsigned exponent = unsigned(magnitude >> 52);
  const auto significand = (magnitude & 0x000fffffffffffffull) |
                           0x0010000000000000ull;
  for (unsigned p = 320; ; p *= 2) {
    // Bound all index/shift/int conversions before work or precision doubling.
    // Exhausted allocation/index resources never produce an approximation.
    if (p > std::numeric_limits<unsigned>::max() / 2)
      throw std::bad_alloc();
    const I scale = I(1) << p;
    const I x = I(significand) << (p - (1075u - exponent));
    Interval enclosure;
    if (2 * x <= scale)
      enclosure = smallAsin(x, x, p);
    else {
      const I square = (scale - x) * scale / 2;
      const I root = boost::multiprecision::sqrt(square);
      const I upper = root + (root * root != square ? 1 : 0);
      const auto a = smallAsin(root, upper, p), pi = piInterval(p);
      enclosure = {pi.lo / 2 - 2 * a.hi, ceilQ(pi.hi, 2) - 2 * a.lo};
    }
    const auto lo = roundAsin64Endpoint(enclosure.lo, p);
    const auto hi = roundAsin64Endpoint(enclosure.hi, p);
    if (lo == hi)
      return sign | lo;
  }
}
std::uint64_t widen(U bits) {
  auto sign = std::uint64_t(bits & signMask) << 32;
  U e = (bits >> 23) & 255, m = bits & 0x7fffff;
  if (e == 255)
    return sign | (m ? 0x7ff8000000000000ull : 0x7ff0000000000000ull);
  if (e)
    return sign | (std::uint64_t(e + 896) << 52) | (std::uint64_t(m) << 29);
  if (!m)
    return sign;
  unsigned high = 31;
  while (!(m & (U(1) << high)))
    --high;
  return sign | (std::uint64_t(high + 874) << 52) |
         ((std::uint64_t(m) << (52 - high)) & 0xfffffffffffffull);
}
U index(std::size_t i) { return rounded(I(i), I(1)); }
bool finite(U b) noexcept { return (b & inf) != inf; }
bool less(U a, U b) noexcept {
  if (!(a & ~signMask) && !(b & ~signMask))
    return false;
  auto key = [](U n) { return n & signMask ? ~n : n | signMask; };
  return key(a) < key(b);
}
double value(U b) { return std::bit_cast<double>(widen(b)); }
} // namespace qualpf::io::powder_cif::binary32
