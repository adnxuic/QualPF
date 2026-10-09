#include "binary32.h"
#include "asin64_reference.h"
#include <cfenv>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#define QUALPF_ASIN64_TEST_SSE 1
#endif
using namespace qualpf::io::powder_cif::binary32;
void emit(U b) {
  std::cout << ' ' << std::hex << std::setw(8) << std::setfill('0') << b;
}
int verifyAsin64() {
  const int originalRound = std::fegetround();
#ifdef QUALPF_ASIN64_TEST_SSE
  const auto originalCsr = _mm_getcsr();
#endif
  bool passed = true;
  unsigned environments = 0;
  for (int mode : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO}) {
    if (std::fesetround(mode) != 0) {
      passed = false;
      break;
    }
    for (unsigned denormals : {0u, 1u}) {
#ifdef QUALPF_ASIN64_TEST_SSE
      const auto csr = _mm_getcsr();
      constexpr unsigned denormalMask = (1u << 15) | (1u << 6);
      _mm_setcsr((csr & ~denormalMask) | (denormals ? denormalMask : 0));
#else
      if (denormals)
        continue;
#endif
      ++environments;
      for (const auto &row : asin64Reference) {
        const auto actual = certifiedAsin64(row[0]);
        if (actual != row[1]) {
          std::cerr << "asin64 mismatch " << std::hex << row[0] << ' '
                    << actual << ' ' << row[1] << '\n';
          passed = false;
        }
      }
#ifdef QUALPF_ASIN64_TEST_SSE
      _mm_setcsr(csr);
#endif
    }
  }
  std::fesetround(originalRound);
#ifdef QUALPF_ASIN64_TEST_SSE
  _mm_setcsr(originalCsr);
#endif
  std::cout << (passed ? "PASS" : "FAIL") << " asin64 "
            << asin64Reference.size() << " independent cases in "
            << environments << " rounding/denormal environments\n";
  return passed ? 0 : 1;
}
int main(int argc, char **argv) {
  if (argc > 1 && std::string(argv[1]) == "verify-asin64")
    return verifyAsin64();
  if (argc > 1) {
    std::string mode = argv[1];
    std::fesetround(mode == "down"   ? FE_DOWNWARD
                    : mode == "up"   ? FE_UPWARD
                    : mode == "zero" ? FE_TOWARDZERO
                                     : FE_TONEAREST);
  }
  std::string line;
  while (std::getline(std::cin, line)) {
    std::istringstream in(line);
    std::string id, cmd;
    in >> id >> cmd;
    std::cout << id;
    U a = 0, b = 0;
    int n = 0;
    try {
      if (cmd == "C") {
        emit(0x40490fdb);
        emit(0x42652ee0);
        emit(0x3c8efa36);
      } else if (cmd == "D") {
        std::string token;
        in >> n >> token;
        emit(decode(token));
        std::cout << " 0";
      } else if (cmd == "A") {
        in >> std::hex >> a;
        emit(certifiedAsin(a));
      } else if (cmd == "A64") {
        std::uint64_t value = 0;
        in >> std::hex >> value;
        std::cout << ' ' << std::hex << std::setw(16) << std::setfill('0')
                  << certifiedAsin64(value);
      } else if (cmd == "T") {
        in >> std::hex >> a >> b;
        U arg = divide(mul(0x3f000000, b), a);
        emit(arg);
        U angle = certifiedAsin(arg);
        emit(angle);
        emit(mul(mul(0x40000000, angle), 0x42652ee0));
      } else if (cmd == "G") {
        in >> std::hex >> a >> b >> std::dec >> n;
        emit(add(a, mul(index(n), b)));
      } else if (cmd == "S") {
        in >> std::dec >> n;
        std::vector<U> v(n);
        U maximum = 0;
        bool first = true;
        for (auto &x : v) {
          in >> std::hex >> x;
          if (first || less(maximum, x)) {
            maximum = x;
            first = false;
          }
        }
        for (auto x : v)
          emit(divide(mul(0x447a0000, x), maximum));
      } else if (cmd == "W") {
        in >> std::hex >> a;
        std::cout << ' ' << std::hex << std::setw(16) << std::setfill('0')
                  << widen(a);
      } else
        throw std::runtime_error("command");
    } catch (const std::exception &e) {
      std::cout << " ERROR " << e.what();
    }
    std::cout << '\n';
  }
  return 0;
}
