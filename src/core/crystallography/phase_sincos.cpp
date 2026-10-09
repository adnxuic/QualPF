#include "phase_sincos.h"
#include <algorithm>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/rational.hpp>
#include <stdexcept>
#include <utility>

namespace qualpf::crystallography::detail {
namespace {

using I = boost::multiprecision::cpp_int;
using U = std::uint64_t;
constexpr U sign = U{1} << 63;
constexpr U mask = sign - 1;

I ceiling(const I &n, const I &d) { return (n+d-1)/d; }

U round_endpoint(I n, unsigned p) {
  const U s = n < 0 ? sign : 0;
  if (n < 0) n = -n;
  if (!n) return s;
  int e = int(boost::multiprecision::msb(n)) - int(p);
  const int quantum = std::max(e-52, -1074);
  const int shift = quantum + int(p);
  I q;
  if (shift > 0) {
    q = n >> shift;
    const I r = n - (q << shift);
    const I half = I(1) << (shift-1);
    if (r > half || (r == half && bool(q & 1))) ++q;
  } else q = n << -shift;
  U m = q.convert_to<U>();
  if (m >= (U{1} << 53)) { m >>= 1; ++e; }
  return s | (e < -1022 ? m : (U(e+1023) << 52) | (m & ((U{1} << 52)-1)));
}

std::pair<I,I> enclosure(const I &x, unsigned p, bool cosine) {
  const I scale = I(1) << p;
  const I square = x*x;
  const I square_lo = square >> p;
  const I square_hi = ceiling(square,scale);
  I tl = cosine ? scale : x, th = tl;
  I lo = tl, hi = th;
  for (unsigned n=1;;++n) {
    const I denominator = scale * (cosine ? I(2*n-1)*(2*n) : I(2*n)*(2*n+1));
    tl = tl*square_lo/denominator;
    th = ceiling(th*square_hi,denominator);
    if (n & 1) { lo -= th; hi -= tl; }
    else { lo += tl; hi += th; }
    // From n>=2 the next term decreases, x^2<10 and next divisor>=30.
    // Its alternating remainder has magnitude <= the next term <= 1 unit.
    if (n>=2 && th<=1) return {lo-1,hi+1};
  }
}

// Exact rational fallback: no native precision/iteration counter or semantic cap.
using R = boost::rational<I>;
U round_rational(const R &value) {
  I n = value.numerator();
  const I &d = value.denominator();
  const U s = n < 0 ? sign : 0;
  if (n < 0) n = -n;
  if (!n) return s;
  // All queried endpoints have magnitude <4; subnormal quantum is fixed.
  int e = 1;
  while (e > -1022 && (e >= 0 ? n < (d << e) : (n << -e) < d)) --e;
  const int quantum = std::max(e-52, -1074);
  const I scaled = n << -quantum;
  I q = scaled / d;
  const I remainder = scaled - q*d;
  if (2*remainder > d || (2*remainder == d && bool(q & 1))) ++q;
  U m = q.convert_to<U>();
  if (m >= (U{1} << 53)) { m >>= 1; ++e; }
  if (e == -1022 && m < (U{1} << 52)) return s | m;
  return s | (U(e+1023) << 52) | (m & ((U{1} << 52)-1));
}
U rational_trig(const R &x, bool cosine, bool negative) {
  const R square = x*x;
  R term = cosine ? R(I(1)) : x;
  R sum = term;
  for (I n=1;;++n) {
    const I divisor = cosine ? I((2*n-1)*(2*n)) : I((2*n)*(2*n+1));
    term *= square / R(divisor);
    if (bool(n & 1)) sum -= term; else sum += term;
    if (n >= 2) {
      const I next_divisor = cosine ? I((2*n+1)*(2*n+2)) : I((2*n+2)*(2*n+3));
      const R tail = term * square / R(next_divisor);
      R lo = sum-tail, hi = sum+tail;
      if (negative) { const R old=lo; lo=-hi; hi=-old; }
      const U low = round_rational(lo), high = round_rational(hi);
      if (low == high) return low;
    }
  }
}

struct Result { U sine, cosine; unsigned precision; };
Result evaluate(U bits, bool force_rational) {
  const U magnitude = bits & mask;
  if (magnitude > 0x400921fb54442d18ULL)
    throw std::invalid_argument("outside existing structure-factor phase interval");
  if (magnitude <= 0x3e40000000000000ULL)
    return {bits,0x3ff0000000000000ULL,0};
  const int power = int(magnitude >> 52) - 1075;
  const U m = (magnitude & ((U{1} << 52)-1)) | (U{1} << 52);
  if (!force_rational) {
    constexpr unsigned p=320;
    const I x = I(m) << (int(p)+power);
    auto [sl,sh] = enclosure(x,p,false);
    auto [cl,ch] = enclosure(x,p,true);
    if (bits & sign) { const I old=sl; sl=-sh; sh=-old; }
    const U s=round_endpoint(sl,p), c=round_endpoint(cl,p);
    if (s==round_endpoint(sh,p) && c==round_endpoint(ch,p))
      return {s,c,p};
  }
  const R x(I(m), I(1) << -power);
  return {rational_trig(x,false,bool(bits & sign)), rational_trig(x,true,false), 0};
}

} // namespace

PhaseSincosBits phase_sincos_bits(std::uint64_t phase_bits, bool force_rational) {
  const auto result = evaluate(phase_bits, force_rational);
  return {result.sine, result.cosine};
}
} // namespace qualpf::crystallography::detail
