#pragma once

namespace qualpf::crystallography::detail {
// Private binary64 RNE primitives for the existing direct numerical callers.
double rounded_log10(double x, bool force_rational = false);
double rounded_exp(double x, bool force_rational = false);
double rounded_pow10(double x, bool force_rational = false);
// Transcript presentation only; production complex amplitude/Fc2 is unchanged.
double rounded_magnitude(double real, double imaginary);
} // namespace qualpf::crystallography::detail
