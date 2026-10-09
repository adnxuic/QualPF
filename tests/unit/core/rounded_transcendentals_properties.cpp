#include "../../../src/core/crystallography/rounded_transcendentals.h"
#include <bit>
#include <cfenv>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace {
using U = std::uint64_t;
bool forced_case(const std::string& op, U a) {
  return (op == "LOG" && (a == 0x4000000000000000ULL || a == 0x401f9c4588a04d12ULL)) ||
         (op == "EXP" && (a == 0x3ff0000000000000ULL || a == 0xbff0000000000000ULL)) ||
         (op == "POW" && (a == 0x3fb999999999999aULL || a == 0xbfb999999999999aULL ||
                         a == 0x4037000000000000ULL)) ||
         (op == "MAG" && a == 0x4008000000000000ULL);
}
bool check(const char* path, bool force) {
  std::ifstream input(path);
  std::string op;
  U a, b, expected;
  unsigned count = 0;
  while (input >> op >> std::hex >> a >> b >> expected) {
    if (force && !forced_case(op, a)) continue;
    using namespace qualpf::crystallography::detail;
    const double x = std::bit_cast<double>(a), y = std::bit_cast<double>(b);
    double actual;
    if (op == "LOG") actual = rounded_log10(x, force);
    else if (op == "EXP") actual = rounded_exp(x, force);
    else if (op == "POW") actual = rounded_pow10(x, force);
    else if (op == "MAG") actual = rounded_magnitude(x, y);
    else return false;
    if (std::bit_cast<U>(actual) != expected) {
      std::cerr << "Exact reference mismatch " << op << ' ' << std::hex << a << ' ' << b
                << " expected " << expected << " actual " << std::bit_cast<U>(actual) << '\n';
      return false;
    }
    ++count;
  }
  return input.eof() && count == (force ? 8u : 8807u);
}
} // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const int original = std::fegetround();
  const int modes[] = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
  for (int mode : modes) {
    if (std::fesetround(mode) != 0 || !check(argv[1], false)) {
      std::fesetround(original);
      return 1;
    }
  }
  const bool passed = std::fesetround(FE_TONEAREST) == 0 && check(argv[1], true);
  std::fesetround(original);
  if (!passed) return 1;
  std::cout << "PASS 8807 independently certified vectors, 4 rounding modes, 8 rational fallbacks\n";
}
