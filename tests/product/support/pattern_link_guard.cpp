// Test-only link guards for retired Pattern and Kalpha2 boundaries.
// A supported product workflow must never enter these native interfaces.
#include <stdexcept>
namespace {
int reads = 0;
int strips = 0;
}
extern "C" void __wrap_open_diffraction_patt(const char *in, int ni, const char *out, int no, int add,
                                             int *error) {
    ++reads;
    throw std::runtime_error("Product Pattern entered the retired native reader");
}
extern "C" int qualpf_test_native_pattern_reads() { return reads; }
extern "C" void __wrap_legacy_kalpha2_values(int, const float *, const float *, float *) {
    ++strips;
    throw std::runtime_error("Product Kalpha2 entered the retired native value consumer");
}
extern "C" int qualpf_test_native_strips() { return strips; }
extern "C" void __wrap_kalpha2_stripping() { ++strips; throw std::runtime_error("Product Kalpha2 entered the retired native full consumer"); }
