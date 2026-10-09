#include "cif_site_symmetry.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <set>
#include <stdexcept>

namespace qualpf::io::cif {
namespace {
using I = std::int64_t;
I add(I a, I b) {
  constexpr auto hi = std::numeric_limits<I>::max();
  if ((b > 0 && a > hi - b) || (b < 0 && a < -hi - b))
    throw std::range_error("exact_coordinate_range");
  return a + b;
}
I mul(I a, I b) {
  constexpr auto hi = std::numeric_limits<I>::max();
  if (a && b && std::abs(a) > hi / std::abs(b))
    throw std::range_error("exact_coordinate_range");
  return a * b;
}
struct Q {
  I n = 0, d = 1;
  Q() = default;
  Q(I numerator, I denominator = 1) : n(numerator), d(denominator) {
    if (!d || n == std::numeric_limits<I>::min() ||
        d == std::numeric_limits<I>::min())
      throw std::range_error("exact_coordinate_range");
    if (d < 0) {
      n = -n;
      d = -d;
    }
    const I g = std::gcd(n, d);
    n /= g;
    d /= g;
  }
  Q operator-() const { return Q(-n, d); }
  Q operator+(Q b) const {
    const I g = std::gcd(d, b.d);
    return Q(add(mul(n, b.d / g), mul(b.n, d / g)), mul(d, b.d / g));
  }
  Q operator-(Q b) const { return *this + -b; }
  Q operator*(Q b) const {
    const I g = std::gcd(n, b.d), h = std::gcd(b.n, d);
    return Q(mul(n / g, b.n / h), mul(d / h, b.d / g));
  }
  Q operator/(Q b) const { return *this * Q(b.d, b.n); }
  bool operator==(const Q &) const = default;
  bool operator<(Q b) const { return (*this - b).n < 0; }
  I floor() const { return n / d - (n < 0 && n % d != 0); }
  Q wrap() const { return *this - Q(floor()); }
  std::string str() const {
    return std::to_string(n) + (d == 1 ? "" : "/" + std::to_string(d));
  }
  double value() const {
    return static_cast<double>(n) / static_cast<double>(d);
  }
};
Q abs(Q v) { return v.n < 0 ? -v : v; }
Q pow10(int e) {
  if (e < -18 || e > 18)
    throw std::range_error("exact_coordinate_precision");
  I v = 1;
  for (int i = 0; i < std::abs(e); ++i)
    v = mul(v, 10);
  return e >= 0 ? Q(v) : Q(1, v);
}
struct Decimal {
  Q nominal, quantum;
  std::optional<Q> su;
  int places = 0, exponent = 0;
};
I digits(std::string_view s) {
  if (s.empty())
    throw std::range_error("exact_coordinate_syntax");
  I result = 0;
  for (char c : s) {
    if (c < '0' || c > '9')
      throw std::range_error("exact_coordinate_syntax");
    result = add(mul(result, 10), c - '0');
  }
  return result;
}
Decimal decimal(std::string_view s) {
  Decimal result;
  const auto e = s.find_first_of("eE");
  if (e != s.npos) {
    auto exponent = s.substr(e + 1);
    bool minus = exponent.starts_with('-');
    if (minus || exponent.starts_with('+'))
      exponent.remove_prefix(1);
    I value = digits(exponent);
    if (value > 18)
      throw std::range_error("exact_coordinate_precision");
    result.exponent = static_cast<int>(minus ? -value : value);
    s = s.substr(0, e);
  }
  std::optional<I> su;
  const auto left = s.find('(');
  if (left != s.npos) {
    if (!s.ends_with(')'))
      throw std::range_error("exact_coordinate_syntax");
    su = digits(s.substr(left + 1, s.size() - left - 2));
    s = s.substr(0, left);
  }
  const bool minus = s.starts_with('-');
  if (minus || s.starts_with('+'))
    s.remove_prefix(1);
  const auto dot = s.find('.');
  if (dot != s.npos)
    result.places = static_cast<int>(s.size() - dot - 1);
  std::string mantissa(s);
  if (dot != s.npos)
    mantissa.erase(dot, 1);
  result.quantum = pow10(result.exponent - result.places);
  result.nominal = Q(digits(mantissa)) * result.quantum * Q(minus ? -1 : 1);
  if (su)
    result.su = Q(*su) * result.quantum;
  return result;
}
using Point = std::array<Q, 3>;
Q binary64(double v) {
  int exponent = 0;
  const auto fraction = std::frexp(v, &exponent);
  I mantissa = static_cast<I>(std::ldexp(fraction, 53));
  exponent -= 53;
  if (!mantissa)
    return Q();
  while (exponent < 0 && mantissa % 2 == 0) {
    mantissa /= 2;
    ++exponent;
  }
  if (exponent < -62 || exponent > 62)
    throw std::range_error("exact_coordinate_range");
  return exponent < 0 ? Q(mantissa, I(1) << -exponent)
                      : Q(mul(mantissa, I(1) << exponent));
}
Point image(const crystallography::SymmetryOperation &op, const Point &p) {
  Point result;
  for (std::size_t i = 0; i < 3; ++i) {
    result[i] = Q(op.translation_twelfths()[i], 12);
    for (std::size_t j = 0; j < 3; ++j)
      result[i] = result[i] + Q(op.rotation()[3 * i + j]) * p[j].wrap();
    result[i] = result[i].wrap();
  }
  return result;
}
std::size_t orbit(const crystallography::SpaceGroup &g, const Point &p,
                  std::vector<std::size_t> *stabilizer = nullptr) {
  std::set<Point> points;
  Point wrapped{p[0].wrap(), p[1].wrap(), p[2].wrap()};
  std::size_t index = 0;
  for (const auto &op : g.operations()) {
    auto q = image(op, p);
    if (stabilizer && q == wrapped)
      stabilizer->push_back(index);
    points.insert(q);
    ++index;
  }
  return points.size();
}
struct ConstraintFamily {
  int setting, multiplicity;
  char letter;
  int rows;
  // Integer coefficients and right-hand-side numerator / 24, modulo lattice.
  std::array<std::array<int, 4>, 3> equations;
};
#include "cif_site_constraints.inc"
bool fits(const ConstraintFamily &f, const Point &p) {
  for (int i = 0; i < f.rows; ++i) {
    const auto &row = f.equations[i];
    Q v = -Q(row[3], 24);
    for (std::size_t j = 0; j < 3; ++j)
      v = v + Q(row[j]) * p[j];
    if (v.d != 1)
      return false;
  }
  return true;
}
void output_point(std::array<std::string, 3> &out, const Point &p) {
  for (std::size_t i = 0; i < 3; ++i)
    out[i] = p[i].str();
}
} // namespace

std::optional<int>
single_site_formula_multiplicity(const Block &block, int atomic_number,
                                 const std::optional<Value> &occupancy) {
  const auto *formula = block.scalar("_chemical_formula_sum");
  const auto *z = block.scalar("_cell_formula_units_z");
  if (!formula || formula->missing() || !z || z->missing())
    return {};
  try {
    const Q units = decimal(z->text).nominal;
    if (units.n <= 0 || units.d != 1)
      return {};
    std::map<int, Q> amounts;
    const auto &text = formula->text;
    std::size_t i = 0;
    while (i < text.size()) {
      if (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' ||
          text[i] == '\n') {
        ++i;
        continue;
      }
      if (text[i] < 'A' || text[i] > 'Z')
        return {};
      std::string symbol(1, text[i++]);
      if (i < text.size() && text[i] >= 'a' && text[i] <= 'z')
        symbol += text[i++];
      auto species = crystallography::ScatteringRegistry{}.find(symbol);
      if (!species)
        return {};
      const auto start = i;
      while (i < text.size() &&
             ((text[i] >= '0' && text[i] <= '9') || text[i] == '.'))
        ++i;
      Q amount = i == start
                     ? Q(1)
                     : decimal(std::string_view(text).substr(start, i - start))
                           .nominal;
      if (amount.n <= 0)
        return {};
      const auto element = species.value().atomic_number();
      amounts[element] = amounts[element] + amount * units;
    }
    if (!amounts.contains(atomic_number))
      return {};
    const auto occ = occupancy && !occupancy->missing()
                         ? decimal(occupancy->text).nominal
                         : Q(1);
    if (occ.n <= 0)
      return {};
    const auto count = amounts.at(atomic_number) / occ;
    if (count.d == 1 && count.n >= 1 && count.n <= 192)
      return static_cast<int>(count.n);
  } catch (const std::range_error &) {
  }
  return {};
}

SiteSymmetryResult
reconcile_site(const crystallography::SpaceGroup &group,
               const SiteProvenance &source, std::array<double, 3> nominal,
               std::array<int, 3> origin_twenty_fourths, bool cartesian,
               std::optional<int> declared_multiplicity,
               std::optional<int> formula_multiplicity, bool springer_profile) {
  SiteSymmetryResult result{{}, nominal, {}};
  auto &e = result.evidence;
  e.policy_version = site_symmetry_policy;
  e.state = "INCONSISTENT_OR_AMBIGUOUS";
  e.constraint_source =
      declared_multiplicity ? "declared_multiplicity" : "no_count_constraint";
  e.constraint_multiplicity = declared_multiplicity;
  auto fail = [&](std::string code, std::string reason) {
    result.error = std::move(code);
    e.reason = std::move(reason);
    return result;
  };
  try {
    Point p;
    std::array<Decimal, 3> values;
    for (std::size_t i = 0; i < 3; ++i) {
      values[i] = decimal(source.coordinates[i].text);
      const auto &v = values[i];
      e.coordinates[i] = {v.nominal.str(),
                          v.su ? v.su->str() : "",
                          v.places,
                          v.exponent,
                          v.quantum.str(),
                          (v.nominal - v.quantum / Q(2)).str(),
                          (v.nominal + v.quantum / Q(2)).str()};
      p[i] = cartesian ? binary64(nominal[i])
                       : v.nominal - Q(origin_twenty_fourths[i], 24);
    }
    for (const auto &[tag, value] : source.site_symmetry_declarations) {
      if (tag.find("wyckoff") == std::string::npos || value.missing())
        continue;
      if (!e.normalized_wyckoff.empty())
        return fail("site_symmetry_ambiguous", "multiple_Wyckoff_aliases");
      auto letter = value.text;
      if (springer_profile && letter.size() > 2 && letter[0] == '.') {
        const auto count =
            digits(std::string_view(letter).substr(1, letter.size() - 2));
        if (count < 1 || count > 192 ||
            (e.constraint_multiplicity && *e.constraint_multiplicity != count))
          return fail("site_symmetry_inconsistent",
                      "packed_Wyckoff_multiplicity_conflict");
        e.constraint_multiplicity = static_cast<int>(count);
        e.constraint_source = "springer_packed_Wyckoff_multiplicity";
        letter = letter.back();
      }
      if (letter.size() != 1 || letter[0] < 'a' || letter[0] > 'z')
        return fail("site_symmetry_ambiguous", "unsupported_Wyckoff_token");
      e.normalized_wyckoff = letter;
    }
    if (!e.constraint_multiplicity && formula_multiplicity) {
      e.constraint_multiplicity = formula_multiplicity;
      e.constraint_source = "formula_Z_single_site_element_balance";
    }
    output_point(e.nominal_fractional, p);
    e.nominal_orbit = orbit(group, p, &e.nominal_stabilizer);
    std::vector<const ConstraintFamily *> families;
    bool known_setting = false;
    for (const auto &f : constraint_families) {
      if (f.setting != group.setting().id)
        continue;
      known_setting = true;
      if (e.constraint_multiplicity &&
          *e.constraint_multiplicity != f.multiplicity)
        continue;
      if (!e.normalized_wyckoff.empty() && e.normalized_wyckoff[0] != f.letter)
        continue;
      families.push_back(&f);
    }
    if (!e.normalized_wyckoff.empty() && !known_setting)
      return fail("site_symmetry_ambiguous",
                  "Wyckoff_setting_evidence_unavailable");
    e.naming_source =
        "spglib-v2.5.0;Phase14-exact-operations;bounded-catalogue";
    const bool exact_count =
        !e.constraint_multiplicity ||
        *e.constraint_multiplicity == static_cast<int>(e.nominal_orbit);
    const bool exact_letter =
        e.normalized_wyckoff.empty() ||
        std::any_of(families.begin(), families.end(), [&](auto f) {
          return f->multiplicity == static_cast<int>(e.nominal_orbit) &&
                 fits(*f, p);
        });
    if (exact_count && exact_letter) {
      e.state = "EXACT";
      e.reason = "nominal_satisfies_available_declared_constraints";
      e.reconciled_fractional = e.nominal_fractional;
      e.delta = {"0", "0", "0"};
      e.reconciled_orbit = e.nominal_orbit;
      e.reconciled_stabilizer = e.nominal_stabilizer;
      return result;
    }
    // Exact transported binary64 is auditable; it is not a source-derived
    // fractional quantization interval. No Cartesian reconciliation is
    // inferred.
    if (cartesian)
      return fail("site_symmetry_ambiguous",
                  "Cartesian_constraint_interval_not_proven");
    if (!known_setting)
      return fail("site_symmetry_ambiguous", "constraint_catalogue_gap");
    std::set<Point> candidates;
    bool unsupported_coupled = false, su_change = false, boundary = false;
    for (const auto *f : families) {
      bool coupled = false;
      for (int row = 0; row < f->rows; ++row)
        coupled = coupled || std::count_if(f->equations[row].begin(),
                                           f->equations[row].begin() + 3,
                                           [](int v) { return v != 0; }) != 1;
      if (coupled) {
        unsupported_coupled = true;
        continue;
      }
      Point candidate = p;
      bool valid = true;
      for (int row = 0; row < f->rows; ++row) {
        const auto &c = f->equations[row];
        const auto axis = static_cast<std::size_t>(
            std::find_if(c.begin(), c.begin() + 3,
                         [](int v) { return v != 0; }) -
            c.begin());
        const Q rhs(c[3], 24), coefficient(c[axis]);
        const auto &v = values[axis];
        if (Q(1) < v.quantum) {
          valid = false;
          boundary = true;
          break;
        }
        const I centre = (coefficient * p[axis] - rhs).floor();
        std::vector<Q> options;
        const I radius = 2 * std::abs(c[axis]) + 1;
        const I first = add(centre, -radius), last = add(centre, radius);
        for (I k = first;; k = add(k, 1)) {
          const Q q = (rhs + Q(k)) / coefficient;
          const Q delta = abs(q - p[axis]);
          if (v.su) {
            if (q == p[axis])
              options.push_back(q);
            else if (!(v.su.value() < delta))
              su_change = true;
          } else {
            if (delta < v.quantum / Q(2))
              options.push_back(q);
            if (delta == v.quantum / Q(2))
              boundary = true;
          }
          if (k == last)
            break;
        }
        if (options.size() != 1) {
          valid = false;
          break;
        }
        candidate[axis] = options[0];
      }
      if (valid &&
          orbit(group, candidate) == static_cast<std::size_t>(f->multiplicity))
        candidates.insert(candidate);
    }
    e.supported_candidates = candidates.size();
    if (candidates.size() != 1 || unsupported_coupled || boundary)
      return fail(candidates.size() > 1 || unsupported_coupled || su_change ||
                          boundary
                      ? "site_symmetry_ambiguous"
                      : "site_symmetry_inconsistent",
                  unsupported_coupled
                      ? "coupled_free_parameter_reconciliation_unsupported"
                  : su_change ? "SU_is_not_a_hard_interval"
                  : boundary  ? "quantization_endpoint_or_broad_interval"
                              : "no_unique_supported_constraint_solution");
    const auto &candidate = *candidates.begin();
    e.state = "UNIQUE_RECONCILABLE";
    e.reason = "unique_fixed_coordinate_solution_in_open_decimal_rounding_"
               "intervals;free_parameters_unchanged";
    output_point(e.reconciled_fractional, candidate);
    for (std::size_t i = 0; i < 3; ++i) {
      e.delta[i] = (candidate[i] - p[i]).str();
      result.position[i] = candidate[i].value();
    }
    e.reconciled_orbit = orbit(group, candidate, &e.reconciled_stabilizer);
    return result;
  } catch (const std::range_error &error) {
    return fail("site_symmetry_ambiguous", error.what());
  }
}
} // namespace qualpf::io::cif
