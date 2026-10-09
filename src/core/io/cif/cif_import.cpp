#include "cif_import.h"
#include "cif_parser.h"
#include "cif_site_symmetry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <unordered_set>

namespace qualpf::io::cif {
namespace {
using namespace crystallography;

struct Context {
  const Document &document;
  const Block &block;
  std::vector<ImportIssue> notices;
  std::array<int, 3> origin_shift{};
  const StructureProvenance *provenance = nullptr;

  ImportIssue issue(std::string code, std::string field, std::string message,
                    const Value *value = nullptr) const {
    return {std::move(code),
            document.source,
            block.name,
            std::move(field),
            value ? value->location : block.location,
            std::move(message),
            {},
            {},
            {}};
  }
  [[noreturn]] void fail(std::string code, std::string field,
                         std::string message,
                         const Value *value = nullptr) const {
    auto error =
        issue(std::move(code), std::move(field), std::move(message), value);
    if (provenance)
      error.provenance = *provenance;
    throw error;
  }
  template <class T>
  T take(qualpf::Result<T> result, std::string field,
         const Value *value = nullptr) const {
    if (result)
      return std::move(result).value();
    auto error =
        issue("core_error", std::move(field), result.error().message, value);
    error.core_field = result.error().field;
    error.core_reason = result.error().reason;
    if (provenance)
      error.provenance = *provenance;
    throw error;
  }
  double numeric(const Value &value, std::string_view field) const {
    auto parsed = number(value);
    if (const auto *error = std::get_if<Error>(&parsed))
      fail(error_name(error->code), std::string(field), error->reason, &value);
    return std::get<double>(parsed);
  }
  const Value *scalar(std::string_view name,
                      std::string_view alias = {}) const {
    const auto *a = block.scalar(name);
    const auto *b = alias.empty() ? nullptr : block.scalar(alias);
    if (a && b && a->text != b->text)
      fail("conflicting_aliases", std::string(name),
           "Both aliases supply different values", b);
    return a ? a : b;
  }
  std::optional<LoopColumn> column(std::string_view name,
                                   std::string_view alias = {}) const {
    const auto a = block.column(name);
    const auto b = alias.empty() ? std::nullopt : block.column(alias);
    if (a && b)
      fail("duplicate_aliases", std::string(name),
           "Both loop aliases are supplied");
    return a ? a : b;
  }
};

std::string trim(std::string text) {
  const auto begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos)
    return {};
  return text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);
}
std::string name_text(const Value *value) {
  if (!value || value->missing())
    return {};
  auto text = value->text;
  // The native metadata presentation joins semicolon text lines with spaces.
  std::replace(text.begin(), text.end(), '\n', ' ');
  text = trim(std::move(text));
  // Native scalar/semicolon name extraction treats a leading question mark
  // as unspecified, including quoted text. Keep that presentation policy
  // here; the syntax value and reported metadata retain their original kind.
  return !text.empty() && text.front() == '?' ? std::string{} : text;
}
bool same_operators(std::span<const SymmetryOperation> a,
                    std::span<const SymmetryOperation> b) {
  return a.size() == b.size() &&
         std::all_of(a.begin(), a.end(), [&](const auto &op) {
           return std::find(b.begin(), b.end(), op) != b.end();
         });
}

// Match an explicit origin to a known registry group without inventing a new
// SpaceGroup identity. Translation only: fractional axes and ADP tensors stay
// unchanged. Every operation is proved equal modulo lattice translation.
std::optional<std::array<int, 3>>
origin_to_registry(std::span<const SymmetryOperation> supplied,
                   const SpaceGroup &registry) {
  if (supplied.size() != registry.operations().size())
    return std::nullopt;
  for (const auto &op : supplied)
    if (std::none_of(
            registry.operations().begin(), registry.operations().end(),
            [&](const auto &r) { return r.rotation() == op.rotation(); }))
      return std::nullopt;
  const auto mod24 = [](int n) { return (n % 24 + 24) % 24; };
  for (int x = 0; x < 24; ++x)
    for (int y = 0; y < 24; ++y)
      for (int z = 0; z < 24; ++z) {
        const std::array<int, 3> origin{x, y, z};
        bool matches = true;
        for (const auto &op : supplied) {
          std::array<int, 3> shift;
          for (std::size_t i = 0; i < 3; ++i) {
            int t = 2 * op.translation_twelfths()[i] - origin[i];
            for (std::size_t j = 0; j < 3; ++j)
              t += op.rotation()[3 * i + j] * origin[j];
            shift[i] = mod24(t);
          }
          if (std::none_of(registry.operations().begin(),
                           registry.operations().end(), [&](const auto &r) {
                             if (r.rotation() != op.rotation())
                               return false;
                             for (std::size_t i = 0; i < 3; ++i)
                               if (shift[i] != 2 * r.translation_twelfths()[i])
                                 return false;
                             return true;
                           })) {
            matches = false;
            break;
          }
        }
        if (matches)
          return origin;
      }
  return std::nullopt;
}

SpaceGroup group(Context &context) {
  SpaceGroupRegistry registry;
  const auto *hall = context.scalar("_symmetry_space_group_name_hall",
                                    "_space_group_name_hall");
  const auto *hm = context.scalar("_symmetry_space_group_name_h-m",
                                  "_space_group_name_h-m_alt");
  std::optional<SpaceGroup> named;
  if (hall && !hall->missing()) {
    const auto result = registry.find(hall->text, SymbolKind::hall);
    if (result)
      named = result.value();
    else
      context.notices.push_back(context.issue("unknown_hall",
                                              "_space_group_name_hall",
                                              result.error().reason, hall));
  }
  if (!named && hm && !hm->missing()) {
    const auto result = registry.find(hm->text, SymbolKind::hermann_mauguin);
    if (result)
      named = result.value();
  }
  const auto operations = context.column("_symmetry_equiv_pos_as_xyz",
                                         "_space_group_symop_operation_xyz");
  if (operations) {
    std::vector<SymmetryOperation> parsed;
    for (std::size_t i = 0; i < operations->loop->rows(); ++i) {
      const auto &value = operations->at(i);
      auto result =
          parse_operation(value, context.document.source, context.block.name);
      if (const auto *error = std::get_if<ImportIssue>(&result))
        throw *error;
      const auto op = std::get<SymmetryOperation>(result);
      if (std::find(parsed.begin(), parsed.end(), op) != parsed.end())
        context.fail("duplicate_operator", "_space_group_symop_operation_xyz",
                     "Repeated exact symmetry operation", &value);
      parsed.push_back(op);
    }
    // Explicit operators are authoritative. Registry identity remains exact;
    // unmatched standalone sets are unsupported by the frozen Core owner.
    for (int id = 0; id < SpaceGroupRegistry::setting_count; ++id) {
      const auto candidate = registry.by_id(id).value();
      if (!same_operators(parsed, candidate.operations()))
        continue;
      if (named && !same_operators(parsed, named->operations()))
        context.notices.push_back(context.issue(
            "operators_override_symbol", "_space_group_symop_operation_xyz",
            "Explicit operators select a different known group from the symbol",
            &operations->at(0)));
      return candidate;
    }
    if (named) {
      if (const auto shift = origin_to_registry(parsed, *named)) {
        context.origin_shift = *shift;
        context.notices.push_back(context.issue(
            "origin_shift_to_registry", "_space_group_symop_operation_xyz",
            "Exact origin translation maps all supplied operators to the named "
            "registry setting",
            &operations->at(0)));
        return *named;
      }
    }
    context.fail(
        "unsupported_operator_set", "_space_group_symop_operation_xyz",
        "Exact operators do not match a known Phase 14 registry setting",
        &operations->at(0));
  }
  if (context.scalar("_symmetry_equiv_pos_as_xyz",
                     "_space_group_symop_operation_xyz"))
    context.fail("operator_loop_required", "_space_group_symop_operation_xyz",
                 "Symmetry operators require a loop");
  // Without explicit operators, declarations must agree. IT number names a
  // group, not an axis/origin setting; a compatible HM supplies that setting.
  const auto *it =
      context.scalar("_symmetry_int_tables_number", "_space_group_it_number");
  if (hall && !hall->missing() && hm && !hm->missing()) {
    const auto h = registry.find(hall->text, SymbolKind::hall);
    const auto m = registry.find(hm->text, SymbolKind::hermann_mauguin);
    if (h && m && h.value().setting().id != m.value().setting().id)
      context.fail("inconsistent_symmetry_definition", "space_group",
                   "Hall and HM declare different settings", hm);
  }
  if (it && !it->missing()) {
    const double n = context.numeric(*it, "_space_group_it_number");
    if (n < 1 || n > 230 || n != std::floor(n))
      context.fail("invalid_symmetry_number", "_space_group_it_number",
                   "IT number must be 1..230", it);
    if (named && named->number() != static_cast<int>(n))
      context.fail("inconsistent_symmetry_definition", "space_group",
                   "Symbol and IT number declare incompatible groups", it);
    if (!named && ((!hall || hall->missing()) && (!hm || hm->missing())))
      named = registry.by_number(static_cast<int>(n)).value();
  }
  if (named)
    return *named;
  if ((!hall || hall->missing()) && (!hm || hm->missing()))
    context.fail("missing_symmetry", "space_group",
                 "HM, Hall, IT number or explicit operators are required");
  context.fail("unknown_symmetry", "space_group",
               "No known space group matches the supplied identifiers",
               hm ? hm : hall);
}

UnitCell cell(Context &context, SpaceGroup &spaceGroup) {
  static constexpr std::array<const char *, 6> names{
      "_cell_length_a",    "_cell_length_b",   "_cell_length_c",
      "_cell_angle_alpha", "_cell_angle_beta", "_cell_angle_gamma"};
  std::array<double, 6> p{};
  std::array<bool, 6> present{};
  std::size_t count = 0;
  for (std::size_t i = 0; i < p.size(); ++i) {
    const auto *value = context.scalar(names[i]);
    if (!value || value->missing())
      continue;
    p[i] = context.numeric(*value, names[i]);
    if (p[i] <= 0)
      context.fail("invalid_cell", names[i], "Cell parameters must be positive",
                   value);
    present[i] = true;
    ++count;
  }
  if (count == 0)
    context.fail("missing_cell", "cell", "No cell parameters supplied");
  const auto set = [&](std::size_t i, double value) {
    p[i] = value;
    present[i] = value > 0;
  };
  const int number = spaceGroup.number();
  if (count < 6) {
    if (number >= 195) {
      set(1, p[0]);
      set(2, p[0]);
      set(3, 90);
      set(4, 90);
      set(5, 90);
    } else if (number >= 143) {
      const bool rhombohedral =
          spaceGroup.hermann_mauguin().find(":R") != std::string_view::npos;
      if (rhombohedral) {
        set(1, p[0]);
        set(2, p[0]);
        set(4, p[3]);
        set(5, p[3]);
      } else {
        if (!present[0])
          set(0, p[1]);
        set(1, p[0]);
        set(3, 90);
        set(4, 90);
        set(5, 120);
      }
    } else if (number >= 75) {
      set(1, p[0]);
      set(3, 90);
      set(4, 90);
      set(5, 90);
    } else if (number >= 16) {
      set(3, 90);
      set(4, 90);
      set(5, 90);
    } else if (number >= 3) {
      const auto &r = spaceGroup.operations()[1].rotation();
      if (r[0] == r[4]) {
        set(3, 90);
        set(4, 90);
      } else if (r[0] == r[8]) {
        set(3, 90);
        set(5, 90);
      } else if (r[4] == r[8]) {
        set(4, 90);
        set(5, 90);
      }
    }
    for (std::size_t i = 0; i < p.size(); ++i)
      if (!present[i])
        context.fail("missing_cell", names[i],
                     "Symmetry cannot complete this missing cell parameter");
    context.notices.push_back(context.issue(
        "completed_cell", "cell",
        "Missing cell parameters completed from the known group"));
  }
  auto symbol = std::string(spaceGroup.hermann_mauguin());
  const auto near = [](double a, double b) { return std::abs(a - b) < .01; };
  const bool hex =
      near(p[0], p[1]) && near(p[3], 90) && near(p[4], 90) && near(p[5], 120);
  const bool rhomb = near(p[0], p[1]) && near(p[1], p[2]) && near(p[3], p[4]) &&
                     near(p[4], p[5]);
  const auto suffix = symbol.find(':');
  if (suffix != std::string::npos &&
      ((symbol.substr(suffix) == ":H" && rhomb) ||
       (symbol.substr(suffix) == ":R" && hex))) {
    symbol.back() = hex ? 'H' : 'R';
    spaceGroup = context.take(
        SpaceGroupRegistry{}.find(symbol, SymbolKind::hermann_mauguin),
        "space_group.setting");
    context.notices.push_back(context.issue("cell_selects_setting",
                                            "space_group.setting",
                                            "Cell selects the H/R setting"));
  }
  return context.take(
      UnitCell::create({{p[0], p[1], p[2]}, {p[3], p[4], p[5]}}), "cell");
}

AtomicSpecies species(Context &context, const Value *value,
                      const Value *label) {
  ScatteringRegistry registry;
  if (value && !value->missing()) {
    const auto raw = trim(value->text);
    if (raw.size() <= 4) {
      auto exact = registry.find(raw);
      if (exact)
        return exact.value();
      // Native pxen_from_charge uses the closest registered charge for this
      // element, in resource order. This is a CIF compatibility policy only.
      auto end = raw.find_first_of("0123456789+-");
      if (end != std::string::npos && end > 0) {
        const auto neutral = registry.find(raw.substr(0, end));
        const auto charge = raw.substr(end);
        int sign = 1;
        int magnitude = 0;
        bool valid = true;
        for (char c : charge) {
          if (c >= '0' && c <= '9')
            magnitude = c - '0';
          else if (c == '-')
            sign = -1;
          else if (c != '+')
            valid = false;
        }
        if (neutral && valid) {
          const int desired = sign * magnitude;
          if (desired == 0)
            return neutral.value();
          std::optional<AtomicSpecies> best;
          int distance = std::numeric_limits<int>::max();
          for (std::size_t i = 0; i < registry.size(); ++i) {
            const auto candidate = registry.by_index(i).value();
            if (candidate.atomic_number() != neutral.value().atomic_number())
              continue;
            const int difference = std::abs(candidate.charge() - desired);
            if (difference < distance) {
              best = candidate;
              distance = difference;
            }
            if (difference == 0)
              break;
          }
          if (best) {
            context.notices.push_back(
                context.issue("legacy_charge_mapping", "_atom_site_type_symbol",
                              "Mapped ionic spelling using the native "
                              "registered-charge policy",
                              value));
            return *best;
          }
        }
      }
    }
  }
  if (label && !label->missing()) {
    const auto text = trim(label->text);
    for (auto count = std::min<std::size_t>(4, text.size()); count > 0;
         --count) {
      const auto result = registry.find(text.substr(0, count));
      if (result) {
        context.notices.push_back(
            context.issue("species_from_label", "_atom_site_label",
                          "Species resolved from the atom label", label));
        return result.value();
      }
    }
  }
  context.fail("unsupported_species", "_atom_site_type_symbol",
               "No registered species or supported label fallback",
               value ? value : label);
}

double card_default_b(AtomicSpecies species) {
  switch (species.atomic_number()) {
  case 1:
    return 6;
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 15:
  case 16:
  case 17:
  case 35:
  case 53:
    return 3;
  default:
    return 1;
  }
}

std::array<double, 3> fractional(std::array<double, 3> cart,
                                 const UnitCell &cell) {
  const auto &p = cell.parameters();
  const auto rad = [](double degrees) {
    return degrees * std::numbers::pi / 180;
  };
  const double ca = std::cos(rad(p.angles_degrees[0]));
  const double cb = std::cos(rad(p.angles_degrees[1]));
  const double cg = std::cos(rad(p.angles_degrees[2]));
  const double sg = std::sin(rad(p.angles_degrees[2]));
  const double cy = p.lengths_angstrom[2] * (ca - cb * cg) / sg;
  const double cz = cell.volume_angstrom_cubed() /
                    (p.lengths_angstrom[0] * p.lengths_angstrom[1] * sg);
  const double z = cart[2] / cz;
  const double y = (cart[1] - cy * z) / (p.lengths_angstrom[1] * sg);
  const double x = (cart[0] - p.lengths_angstrom[1] * cg * y -
                    p.lengths_angstrom[2] * cb * z) /
                   p.lengths_angstrom[0];
  return {x, y, z};
}

// Formula sum is a declaration about the complete cell when Z is supplied.
// The final balance check does not change coordinates or Core coincidence.
void validate_formula_units(Context &context, const CrystalStructure &structure,
                            FormulaZConsistency *evidence = nullptr) {
  const auto *formula = context.scalar("_chemical_formula_sum");
  const auto *zValue = context.scalar("_cell_formula_units_z");
  if (evidence) {
    if (formula)
      evidence->source_formula = *formula;
    if (zValue)
      evidence->source_z = *zValue;
    for (const auto &site : structure.expanded_sites())
      evidence->site_cell_composition[site.species.atomic_number()] +=
          site.occupancy.value();
  }
  if (!formula || formula->missing() || !zValue || zValue->missing())
    return;
  const double z = context.numeric(*zValue, "_cell_formula_units_z");
  if (z <= 0 || z != std::floor(z))
    context.fail("invalid_formula_units", "_cell_formula_units_z",
                 "Z must be a positive integer", zValue);
  std::map<int, double> declared, observed;
  const auto &text = formula->text;
  std::size_t i = 0;
  while (i < text.size()) {
    if (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' ||
        text[i] == '\n') {
      ++i;
      continue;
    }
    if (text[i] < 'A' || text[i] > 'Z')
      context.fail("unsupported_formula_sum", "_chemical_formula_sum",
                   "Cannot validate Z against this formula sum; explicit "
                   "element/count syntax required",
                   formula);
    std::string symbol(1, text[i++]);
    if (i < text.size() && text[i] >= 'a' && text[i] <= 'z')
      symbol += text[i++];
    const auto element = ScatteringRegistry{}.find(symbol);
    if (!element)
      context.fail("unsupported_formula_sum", "_chemical_formula_sum",
                   "Unknown formula element " + symbol, formula);
    const auto begin = i;
    while (i < text.size() &&
           ((text[i] >= '0' && text[i] <= '9') || text[i] == '.'))
      ++i;
    double amount = 1;
    if (i != begin) {
      auto value = *formula;
      value.text = text.substr(begin, i - begin);
      amount = context.numeric(value, "_chemical_formula_sum");
    }
    if (amount <= 0 || !std::isfinite(amount * z))
      context.fail("invalid_formula_sum", "_chemical_formula_sum",
                   "Formula amounts must give positive finite cell totals",
                   formula);
    declared[element.value().atomic_number()] += amount * z;
    if (!std::isfinite(declared[element.value().atomic_number()]))
      context.fail("invalid_formula_sum", "_chemical_formula_sum",
                   "Formula cell amount overflows", formula);
  }
  for (const auto &site : structure.expanded_sites())
    observed[site.species.atomic_number()] += site.occupancy.value();
  // Only floating summation roundoff is allowed here: no experimental SU,
  // coordinate precision, crystallographic or Legacy tolerance is inferred.
  const auto bound =
      64 * std::numeric_limits<double>::epsilon() *
      std::max<std::size_t>(1, structure.expanded_sites().size());
  bool matches = declared.size() == observed.size();
  for (const auto &[element, amount] : declared)
    matches = matches && observed.contains(element) &&
              std::abs(observed[element] - amount) <=
                  bound * std::max(1.0, std::abs(amount));
  if (evidence) {
    evidence->formula_times_z = declared;
    evidence->state = matches ? FormulaZState::consistent
                              : FormulaZState::metadata_discrepancy;
    if (!matches) {
      evidence->notice =
          "Source formula times source Z differs from site-derived cell "
          "composition. Source metadata retained; site model unchanged. "
          "No rounding or statistical compatibility is asserted.";
      context.notices.push_back(context.issue("formula_z_metadata_discrepancy",
                                              "_chemical_formula_sum",
                                              evidence->notice, formula));
    }
    return;
  }
  if (!matches)
    context.fail("inconsistent_formula_units", "_chemical_formula_sum",
                 "Validated symmetry-expanded occupancy totals disagree with "
                 "formula sum times Z; no card is published",
                 formula);
}

// Document is a public syntax DTO. Explicit source-bound policies must reject
// a caller-edited tree that still carries the hash of different original bytes.
std::string document_binding(const Document &document) {
  std::string result;
  const auto frame = [&](std::string_view text) {
    result += std::to_string(text.size()) + ":";
    result += text;
  };
  const auto location = [&](Location value) {
    frame(std::to_string(value.offset));
    frame(std::to_string(value.line));
    frame(std::to_string(value.column));
  };
  const auto tag = [&](const Tag &value) {
    frame(value.name);
    frame(value.spelling);
    location(value.location);
  };
  const auto value = [&](const Value &v) {
    frame(v.text);
    frame(v.raw);
    frame(std::to_string(static_cast<int>(v.kind)));
    location(v.location);
  };
  frame(std::to_string(document.blocks.size()));
  for (const auto &b : document.blocks) {
    frame(b.name);
    location(b.location);
    frame(std::to_string(b.items.size()));
    for (const auto &item : b.items) {
      tag(item.tag);
      value(item.value);
    }
    frame(std::to_string(b.loops.size()));
    for (const auto &loop : b.loops) {
      location(loop.location);
      frame(std::to_string(loop.tags.size()));
      for (const auto &t : loop.tags)
        tag(t);
      frame(std::to_string(loop.values.size()));
      for (const auto &v : loop.values)
        value(v);
    }
  }
  return sha256(result);
}

ImportedCrystal import_block(const Document &document, const Block &block,
                             ImportPolicies policies) {
  Context context{document, block, {}};
  if ((policies.source_ueq != SourceUeqPolicy::strict &&
       policies.source_ueq != SourceUeqPolicy::zincite_2300112_v1) ||
      (policies.formula_z != FormulaZPolicy::strict &&
       policies.formula_z != FormulaZPolicy::source_metadata_v1))
    context.fail("unknown_import_policy", "policy",
                 "Unknown explicit import policy");
  if (policies.source_ueq != SourceUeqPolicy::strict ||
      policies.formula_z != FormulaZPolicy::strict) {
    const auto parsed = parse(document.original_bytes, document.source);
    const auto *original = std::get_if<Document>(&parsed);
    if (!original || document_binding(*original) != document_binding(document))
      context.fail("source_document_mismatch", "source",
                   "Policy source bytes and parsed document disagree");
  }
  auto spaceGroup = group(context);
  const auto unitCell = cell(context, spaceGroup);
  StructureProvenance source;
  context.provenance = &source;
  source.source = document.source;
  source.source_sha256 = sha256(document.original_bytes);
  source.selected_block = block.name;
  source.block_id = ascii_lower(block.name);
  source.source_profile = "generic_cif";
  source.symmetry_source = context.column("_symmetry_equiv_pos_as_xyz",
                                          "_space_group_symop_operation_xyz")
                               ? "explicit_operators"
                               : "phase14_registry";
  source.normalized_identifier = spaceGroup.hermann_mauguin();
  source.registry_setting = spaceGroup.setting().id;
  source.registry_choice = spaceGroup.setting().choice;
  source.registry_transform = spaceGroup.setting().to_setting;
  source.operation_count = spaceGroup.operations().size();
  if (const auto operations = context.column(
          "_symmetry_equiv_pos_as_xyz", "_space_group_symop_operation_xyz"))
    for (std::size_t row = 0; row < operations->loop->rows(); ++row)
      source.space_group_declarations.emplace("explicit_operator_row_" +
                                                  std::to_string(row + 1),
                                              operations->at(row));
  for (const auto &item : block.items) {
    source.selected_metadata.emplace(item.tag.name, item.value);
    if (item.tag.name.starts_with("_sm_"))
      source.source_profile = "springer_pauling_file";
    if (item.tag.name.starts_with("_symmetry_") ||
        item.tag.name.starts_with("_space_group_"))
      source.space_group_declarations.emplace(item.tag.name, item.value);
  }
  std::array<std::optional<LoopColumn>, 3> coords;
  bool cartesian = false;
  for (std::size_t i = 0; i < 3; ++i)
    coords[i] = context.column(std::string("_atom_site_fract_") + "xyz"[i]);
  if (!std::all_of(coords.begin(), coords.end(),
                   [](const auto &c) { return c.has_value(); })) {
    cartesian = true;
    for (std::size_t i = 0; i < 3; ++i)
      coords[i] = context.column(std::string("_atom_site_cartn_") + "xyz"[i]);
  }
  if (!std::all_of(coords.begin(), coords.end(),
                   [](const auto &c) { return c.has_value(); }))
    context.fail("missing_coordinates", "_atom_site_fract",
                 "A complete fractional or Cartesian atom loop is required");
  const Loop *atoms = coords[0]->loop;
  for (const auto &coord : coords)
    if (coord->loop != atoms)
      context.fail("coordinate_loop_mismatch", "_atom_site_fract",
                   "Coordinates must share one atom loop");
  struct BoundColumn {
    const Loop *loop;
    std::size_t column;
    std::vector<std::size_t> rows;
    const Value &at(std::size_t row) const {
      return loop->values.at(rows.at(row) * loop->tags.size() + column);
    }
  };
  const auto site_column = [&](const char *name) -> std::optional<BoundColumn> {
    const auto col = context.column(name);
    if (!col)
      return std::nullopt;
    BoundColumn result{col->loop, col->column, {}};
    if (col->loop == atoms) {
      for (std::size_t row = 0; row < atoms->rows(); ++row)
        result.rows.push_back(row);
      return result;
    }
    // Repository SpringerMaterials CIFs put isotropic displacement fields in
    // a separate _atom_site_label_1 loop. Join by label, never row position.
    const auto atomLabels = context.column("_atom_site_label");
    const auto extraLabels = context.column("_atom_site_label_1");
    if (!atomLabels || atomLabels->loop != atoms || !extraLabels ||
        extraLabels->loop != col->loop)
      context.fail(
          "atom_loop_mismatch", name,
          "Separate atom fields require a matching atom-site label loop");
    std::map<std::string, std::size_t> rows;
    for (std::size_t row = 0; row < col->loop->rows(); ++row) {
      const auto &label = extraLabels->at(row);
      if (label.missing() || !rows.emplace(label.text, row).second)
        context.fail("ambiguous_atom_loop_label", "_atom_site_label_1",
                     "Separate atom loop labels must be present and unique",
                     &label);
    }
    if (rows.size() != atoms->rows())
      context.fail("atom_loop_mismatch", name,
                   "Separate atom loop must identify every coordinate site");
    for (std::size_t row = 0; row < atoms->rows(); ++row) {
      const auto match = rows.find(atomLabels->at(row).text);
      if (match == rows.end())
        context.fail("unmatched_atom_loop_label", name,
                     "Separate atom field has no label match",
                     &atomLabels->at(row));
      result.rows.push_back(match->second);
    }
    return result;
  };
  const auto labels = site_column("_atom_site_label");
  const auto types = site_column("_atom_site_type_symbol");
  const auto occ = site_column("_atom_site_occupancy");
  const auto uiso = site_column("_atom_site_u_iso_or_equiv");
  const auto biso = site_column("_atom_site_b_iso_or_equiv");
  const auto adptype =
      site_column("_atom_site_thermal_displace_type").has_value()
          ? site_column("_atom_site_thermal_displace_type")
          : site_column("_atom_site_adp_type");
  const auto calc = site_column("_atom_site_calc_flag");
  std::map<std::string, BoundColumn> siteSymmetryColumns;
  for (const char *name :
       {"_atom_site_wyckoff_symbol", "_atom_site.wyckoff_symbol",
        "_atom_site_site_symmetry_order", "_atom_site_site_symmetry_symbol",
        "_sm_site_symmetry", "_atom_site_refinement_flags",
        "_atom_site_refinement_flags_posn"})
    if (auto column = site_column(name))
      siteSymmetryColumns.emplace(name, std::move(*column));
  std::optional<BoundColumn> multiplicity;
  for (const char *name : {"_atom_site_symmetry_multiplicity",
                           "_atom_site_site_symmetry_multiplicity",
                           "_atom_site.site_symmetry_multiplicity",
                           "_atom_site.symmetry_multiplicity"}) {
    if (auto column = site_column(name)) {
      if (multiplicity)
        context.fail("duplicate_aliases", name,
                     "Multiple site multiplicity aliases supplied");
      multiplicity = std::move(column);
    }
  }
  const auto anisLabels = context.column("_atom_site_aniso_label");
  static constexpr std::array<const char *, 6> components{"11", "22", "33",
                                                          "12", "13", "23"};
  std::array<std::optional<LoopColumn>, 6> anisU, anisB, anisBeta;
  std::size_t nU = 0, nB = 0, nBeta = 0;
  for (std::size_t i = 0; i < 6; ++i) {
    anisU[i] =
        context.column(std::string("_atom_site_aniso_u_") + components[i]);
    anisB[i] =
        context.column(std::string("_atom_site_aniso_b_") + components[i]);
    anisBeta[i] =
        context.column(std::string("_atom_site_aniso_beta_") + components[i]);
    nU += anisU[i].has_value();
    nB += anisB[i].has_value();
    nBeta += anisBeta[i].has_value();
  }
  if ((nU && nU != 6) || (nB && nB != 6) || (nBeta && nBeta != 6) ||
      ((nU || nB || nBeta) && !anisLabels))
    context.fail("incomplete_anisotropic_tensor", "_atom_site_aniso",
                 "All six tensor components and labels are required");
  std::map<std::string, Displacement> tensors;
  const std::string tensorField = nU   ? "_atom_site_aniso_u"
                                  : nB ? "_atom_site_aniso_b"
                                       : "_atom_site_aniso_beta";
  if (nU || nB || nBeta) {
    const auto &cols = nU ? anisU : nB ? anisB : anisBeta;
    for (const auto &col : cols)
      if (col->loop != anisLabels->loop)
        context.fail("anisotropic_loop_mismatch", "_atom_site_aniso",
                     "Tensor components and labels must share a loop");
    for (std::size_t row = 0; row < anisLabels->loop->rows(); ++row) {
      const auto &label = anisLabels->at(row);
      if (label.missing())
        context.fail("missing_anisotropic_label", "_atom_site_aniso_label",
                     "Tensor label is missing", &label);
      DisplacementTensor tensor{};
      for (std::size_t i = 0; i < 6; ++i)
        tensor[i] = context.numeric(cols[i]->at(row),
                                    cols[i]->loop->tags[cols[i]->column].name);
      // CIF B is in angstrom squared, not dimensionless beta. Keep the
      // conversion in the already validated Core displacement owner.
      auto displacement =
          context.take(nU   ? Displacement::anisotropic_u(tensor)
                       : nB ? Displacement::anisotropic_b(tensor)
                            : Displacement::anisotropic_beta(tensor),
                       tensorField, &label);
      if (!tensors.emplace(label.text, displacement).second)
        context.fail("duplicate_anisotropic_label", "_atom_site_aniso_label",
                     "Multiple tensors for one atom label", &label);
    }
  }
  std::vector<AtomicSite> sites;
  std::vector<ImportedSiteRecord> provenance;
  std::unordered_set<std::string> usedLabels;
  std::unordered_set<std::string> matchedTensors;
  for (std::size_t row = 0; row < atoms->rows(); ++row) {
    const auto at =
        [&](const std::optional<BoundColumn> &col) -> const Value * {
      return col ? &col->at(row) : nullptr;
    };
    const auto *label = at(labels);
    if (label && !label->missing() && !usedLabels.insert(label->text).second)
      context.fail("duplicate_atom_label", "_atom_site_label",
                   "Atom labels must identify sites uniquely", label);
    if (calc && ascii_lower(calc->at(row).text) == "dum") {
      // Legacy retains a ghost in its import workspace but removes it
      // before card calculation. The physical Core owns only real sites.
      // Keep a located notice instead of manufacturing an AtomicSpecies.
      context.notices.push_back(context.issue(
          "nonphysical_dummy_site", "_atom_site_calc_flag",
          "Explicit dummy site " + (label ? label->text : std::string{}) +
              " is excluded from the theoretical-card structure",
          at(calc)));
      if (label && tensors.contains(label->text))
        matchedTensors.insert(label->text);
      continue;
    }
    if (label && label->missing())
      context.fail("missing_atom_label", "_atom_site_label",
                   "Supplied atom label is missing", label);
    const auto element = species(context, at(types), label);
    std::array<double, 3> position;
    for (std::size_t i = 0; i < 3; ++i)
      position[i] = context.numeric(coords[i]->at(row),
                                    atoms->tags[coords[i]->column].name);
    if (cartesian)
      position = fractional(position, unitCell);
    for (std::size_t i = 0; i < 3; ++i)
      position[i] -= double(context.origin_shift[i]) / 24;
    const auto point = context.take(FractionalPosition::create(position),
                                    "_atom_site_fract", &coords[0]->at(row));
    if (std::any_of(position.begin(), position.end(),
                    [](double v) { return v < 0 || v >= 1; }) &&
        std::find(source.normalization.begin(), source.normalization.end(),
                  "fractional_wrap") == source.normalization.end())
      source.normalization.push_back("fractional_wrap");
    const bool defaultOcc = !occ || occ->at(row).missing();
    const auto occupancy = context.take(
        Occupancy::create(
            defaultOcc ? 1
                       : context.numeric(occ->at(row), "_atom_site_occupancy")),
        "_atom_site_occupancy", at(occ));
    const auto chosen = uiso ? uiso : biso;
    const bool defaultAdp = !chosen || chosen->at(row).missing();
    std::optional<double> isotropic;
    if (!defaultAdp) {
      isotropic = context.numeric(chosen->at(row),
                                  chosen->loop->tags[chosen->column].name);
      std::string type =
          adptype ? ascii_lower(adptype->at(row).text) : std::string{};
      if (type != "biso" && type != "uiso")
        type = uiso ? "uiso" : "biso";
      if (type == "uiso")
        *isotropic *= 8 * std::numbers::pi * std::numbers::pi;
    }
    const auto tensor = label ? tensors.find(label->text) : tensors.end();
    SiteProvenance origin;
    origin.coordinate_row = row + 1;
    origin.label = label ? label->text : std::string{};
    origin.occupancy_source = !occ         ? "absent_default_one"
                              : defaultOcc ? "unknown_default_one"
                                           : "explicit";
    if (occ)
      origin.occupancy = occ->at(row);
    origin.coordinates = {coords[0]->at(row), coords[1]->at(row),
                          coords[2]->at(row)};
    for (const auto &[tag, column] : siteSymmetryColumns)
      origin.site_symmetry_declarations.emplace(tag, column.at(row));
    if (multiplicity)
      origin.site_symmetry_declarations.emplace(
          multiplicity->loop->tags[multiplicity->column].name,
          multiplicity->at(row));
    if (chosen) {
      origin.isotropic_row = chosen->rows[row] + 1;
      origin.displacement_values.emplace(
          chosen->loop->tags[chosen->column].name, chosen->at(row));
      if (!defaultAdp)
        origin.displacement = uiso ? SourceDisplacement::isotropic_u
                                   : SourceDisplacement::isotropic_b;
    }
    for (const auto *col : {&uiso, &biso, &adptype})
      if (*col)
        origin.displacement_values.emplace(
            (*col)->loop->tags[(*col)->column].name, (*col)->at(row));
    if (!defaultAdp && adptype) {
      const auto type = ascii_lower(adptype->at(row).text);
      if (type == "biso")
        origin.displacement = SourceDisplacement::isotropic_b;
      if (type == "uiso")
        origin.displacement = SourceDisplacement::isotropic_u;
    }
    if (uiso && biso)
      source.notices.push_back("multiple_isotropic_records:" + origin.label +
                               ":phase19a_U_column_precedence");
    if (tensor != tensors.end()) {
      origin.displacement = nU   ? SourceDisplacement::anisotropic_u
                            : nB ? SourceDisplacement::anisotropic_b
                                 : SourceDisplacement::anisotropic_beta;
      const auto &columns = nU ? anisU : nB ? anisB : anisBeta;
      for (std::size_t t = 0; t < anisLabels->loop->rows(); ++t) {
        if (anisLabels->at(t).text != label->text)
          continue;
        origin.anisotropic_row = t + 1;
        for (const auto &col : columns)
          origin.displacement_values.emplace(col->loop->tags[col->column].name,
                                             col->at(t));
      }
    }
    if (origin.displacement == SourceDisplacement::unknown)
      source.notices.push_back("missing_adp:" + origin.label +
                               ":unknown;phase19a_card_policy_applied");
    if (defaultOcc)
      source.notices.push_back("default_occupancy:" + origin.label + ":one");
    source.sites.push_back(std::move(origin));
    std::optional<Displacement> displacement;
    DisplacementTensor importedBeta{};
    if (tensor != tensors.end()) {
      displacement = tensor->second;
      importedBeta =
          context.take(displacement->beta(unitCell), tensorField, label);
      matchedTensors.insert(tensor->first);
      if (!isotropic)
        isotropic = context.take(equivalent_isotropic_b(importedBeta, unitCell),
                                 tensorField, label);
    }
    if (!isotropic)
      isotropic = element.atomic_number() == 1 ? 4 : 1;
    const double effectiveB =
        displacement
            ? context.take(equivalent_isotropic_b(importedBeta, unitCell),
                           tensorField, label)
            : *isotropic;
    const bool cardDefault = effectiveB <= 0 || effectiveB > 100;
    if (cardDefault) {
      displacement =
          context.take(Displacement::isotropic_b(card_default_b(element)),
                       "_atom_site_b_iso_or_equiv");
      context.notices.push_back(
          context.issue("card_default_adp", "_atom_site_b_iso_or_equiv",
                        "Card policy replaces nonpositive or above-100 "
                        "equivalent B with element defaults",
                        at(chosen)));
    } else if (!displacement) {
      displacement = context.take(Displacement::isotropic_b(*isotropic),
                                  "_atom_site_b_iso_or_equiv", at(chosen));
    }
    provenance.push_back(
        {label ? label->text : std::string{},
         position,
         *isotropic,
         importedBeta,
         defaultOcc,
         defaultAdp,
         cardDefault,
         {coords[0]->at(row), coords[1]->at(row), coords[2]->at(row)},
         std::nullopt,
         0});
    if (multiplicity && !multiplicity->at(row).missing()) {
      const auto &value = multiplicity->at(row);
      const double count =
          context.numeric(value, "_atom_site_symmetry_multiplicity");
      if (count < 1 || count > 192 || count != std::floor(count))
        context.fail(
            "invalid_site_multiplicity", "_atom_site_symmetry_multiplicity",
            "Site multiplicity must be an integer from 1 through 192", &value);
      provenance.back().declared_multiplicity = static_cast<int>(count);
    }
    sites.push_back({element, point, occupancy, *displacement});
  }
  if (matchedTensors.size() != tensors.size())
    context.fail("unmatched_anisotropic_label", "_atom_site_aniso_label",
                 "Tensor does not identify an imported atom");
  std::map<int, std::size_t> elementSites;
  for (const auto &site : sites)
    ++elementSites[site.species.atomic_number()];
  for (std::size_t i = 0; i < sites.size(); ++i) {
    auto &site = sites[i];
    const auto formulaMultiplicity =
        policies.formula_z == FormulaZPolicy::strict &&
                elementSites[site.species.atomic_number()] == 1
            ? single_site_formula_multiplicity(block,
                                               site.species.atomic_number(),
                                               source.sites[i].occupancy)
            : std::nullopt;
    auto reconciled = reconcile_site(
        spaceGroup, source.sites[i], provenance[i].input_fractional,
        context.origin_shift, cartesian, provenance[i].declared_multiplicity,
        formulaMultiplicity, source.source_profile == "springer_pauling_file");
    source.sites[i].site_symmetry = reconciled.evidence;
    if (!reconciled.error.empty())
      context.fail(reconciled.error, "_atom_site_fract",
                   "Site " + provenance[i].label + ": " +
                       reconciled.evidence.reason,
                   &provenance[i].source_coordinates[0]);
    if (reconciled.evidence.state == "UNIQUE_RECONCILABLE") {
      site.position = context.take(
          FractionalPosition::create(reconciled.position), "_atom_site_fract");
      source.normalization.push_back("site_symmetry_reconciliation:" +
                                     provenance[i].label);
    }
  }
  // Always establish the unchanged strict result first. The narrow policy is
  // bound to an independently reviewed source, not any failed tensor in a DB.
  auto strict = CrystalStructure::create(
      unitCell, spaceGroup,
      policies.source_ueq == SourceUeqPolicy::strict ? std::move(sites)
                                                     : sites);
  if (!strict && policies.source_ueq == SourceUeqPolicy::zincite_2300112_v1 &&
      strict.error().reason == "incompatible_special_position_tensor" &&
      source.source_sha256 ==
          "bf25e35b06c55d9f35dc51f8afc992b4654844f433c5192327beacf3bffb038b" &&
      source.selected_block == "2300112" && source.registry_setting == 479 &&
      sites.size() == 2 && !cartesian &&
      context.origin_shift == std::array<int, 3>{}) {
    std::optional<std::size_t> chosenSite;
    bool eligible = true;
    for (std::size_t i = 0; i < sites.size(); ++i) {
      const auto expanded = expand_site(sites[i], unitCell, spaceGroup);
      if (expanded)
        continue;
      const auto &origin = source.sites[i];
      const auto &e = origin.site_symmetry;
      const auto u =
          origin.displacement_values.find("_atom_site_u_iso_or_equiv");
      auto type =
          origin.displacement_values.find("_atom_site_thermal_displace_type");
      if (type == origin.displacement_values.end())
        type = origin.displacement_values.find("_atom_site_adp_type");
      auto nominal = sites[i];
      nominal.position = context.take(
          FractionalPosition::create(provenance[i].input_fractional),
          "_atom_site_fract");
      const auto nominalExpansion = expand_site(nominal, unitCell, spaceGroup);
      bool completeTensor = true;
      for (const char *component : components)
        completeTensor = completeTensor &&
                         origin.displacement_values.contains(
                             std::string("_atom_site_aniso_u_") + component);
      const bool valid =
          !chosenSite && origin.label == "O" &&
          sites[i].species.atomic_number() == 8 &&
          expanded.error().reason == "incompatible_special_position_tensor" &&
          origin.displacement == SourceDisplacement::anisotropic_u &&
          completeTensor && origin.anisotropic_row && origin.isotropic_row &&
          !provenance[i].default_import_adp &&
          !provenance[i].default_card_adp &&
          type != origin.displacement_values.end() &&
          type->second.text == "Uani" &&
          u != origin.displacement_values.end() &&
          u->second.text == "0.0075(11)" && e.state == "UNIQUE_RECONCILABLE" &&
          e.nominal_orbit == 6 && e.reconciled_orbit == 2 &&
          e.nominal_stabilizer.size() == 2 &&
          e.reconciled_stabilizer.size() == 6 && nominalExpansion &&
          nominalExpansion.value().size() == 6;
      eligible = eligible && valid;
      chosenSite = i;
    }
    if (eligible && chosenSite) {
      const auto i = *chosenSite;
      const auto &u =
          source.sites[i].displacement_values.at("_atom_site_u_iso_or_equiv");
      const double value = context.numeric(u, "_atom_site_u_iso_or_equiv");
      if (std::isfinite(value) && value > 0 &&
          8 * std::numbers::pi * std::numbers::pi * value <= 100) {
        source.strict_attempt_ledger = provenance_ledger(source);
        source.displacement_choices.push_back(
            {std::string(source_ueq_policy_v1), "O",
             "_atom_site_u_iso_or_equiv", "angstrom_squared", u,
             strict.error().field, strict.error().reason,
             strict.error().message,
             "A1:"
             "3edc55e382ec138bc94d3e9ef8b49bdc26ca86006fd01ad2d36db9819ed11964;"
             "positive-definite nominal tensor; rounding-feasible q in "
             "(0.0071,0.0073);"
             "source/block/site certificate, no tensor selected or corrected"});
        sites[i].displacement = context.take(Displacement::isotropic_u(value),
                                             "_atom_site_u_iso_or_equiv", &u);
        context.notices.push_back(context.issue(
            "explicit_source_ueq_selected", "_atom_site_u_iso_or_equiv",
            "O: anisotropic tensor rejected for calculation; source Ueq "
            "explicitly selected by policy. Original tensor and strict failure "
            "retained; tensor not corrected.",
            &u));
        strict =
            CrystalStructure::create(unitCell, spaceGroup, std::move(sites));
      }
    }
  }
  auto structure = context.take(std::move(strict), "structure");
  for (std::size_t i = 0; i < provenance.size(); ++i) {
    auto &record = provenance[i];
    record.orbit_multiplicity =
        context
            .take(expand_site(structure.sites()[i], unitCell, spaceGroup),
                  "structure")
            .size();
    if (record.declared_multiplicity &&
        record.orbit_multiplicity !=
            static_cast<std::size_t>(*record.declared_multiplicity))
      context.fail("site_symmetry_inconsistent",
                   "_atom_site_symmetry_multiplicity",
                   "Site " + record.label + " declares multiplicity " +
                       std::to_string(*record.declared_multiplicity) +
                       " but validated coordinates generate " +
                       std::to_string(record.orbit_multiplicity) +
                       "; exact importer and Core orbit contracts disagree",
                   &record.source_coordinates[0]);
  }
  if (policies.formula_z == FormulaZPolicy::source_metadata_v1) {
    FormulaZConsistency evidence;
    evidence.policy_version = formula_z_policy_v1;
    try {
      validate_formula_units(context, structure, &evidence);
    } catch (const ImportIssue &error) {
      // Uninterpretable source metadata cannot rewrite an otherwise validated
      // site model. Retain the exact diagnostic, not a guessed formula/Z.
      evidence.state = FormulaZState::insufficient_metadata;
      evidence.notice = error.code + ":" + error.message;
      context.notices.push_back(context.issue(
          "formula_z_uninterpretable_metadata", error.field, evidence.notice));
    }
    source.formula_z = std::move(evidence);
  } else {
    validate_formula_units(context, structure);
  }
  if (policies.source_ueq != SourceUeqPolicy::strict ||
      policies.formula_z != FormulaZPolicy::strict) {
    source.schema = "qualpf.cif.structure-provenance.v3";
    if (policies.source_ueq == SourceUeqPolicy::zincite_2300112_v1)
      source.source_ueq_policy = source_ueq_policy_v1;
    if (policies.formula_z == FormulaZPolicy::source_metadata_v1)
      source.formula_z_policy = formula_z_policy_v1;
  }
  StructureMetadata metadata{document.source,      block.name, {}, {},
                             context.origin_shift, {}};
  metadata.chemical_name =
      name_text(context.scalar("_chemical_name_systematic"));
  if (metadata.chemical_name.empty())
    metadata.chemical_name = name_text(context.scalar("_chemical_name_common"));
  metadata.mineral_name = name_text(context.scalar("_chemical_name_mineral"));
  for (const char *field :
       {"_chemical_formula_sum", "_chemical_formula_structural",
        "_cell_formula_units_z", "_cell_volume", "_diffrn_ambient_temperature",
        "_chemical_name_systematic", "_chemical_name_common",
        "_chemical_name_mineral", "_database_code_cod",
        "_publ_section_references", "_journal_name_full", "_journal_year",
        "_diffrn_radiation_wavelength"})
    if (const auto *value = context.scalar(field))
      metadata.reported.emplace(field, *value);
  if (cartesian)
    source.normalization.push_back("cartesian_to_fractional");
  for (const auto &notice : context.notices) {
    source.notices.push_back(notice.code + ":" + notice.field + ":" +
                             notice.message);
    if (notice.code == "origin_shift_to_registry" ||
        notice.code == "completed_cell" ||
        notice.code == "cell_selects_setting" ||
        notice.code == "legacy_charge_mapping" ||
        notice.code == "nonphysical_dummy_site" ||
        notice.code == "species_from_label")
      source.normalization.push_back(
          notice.code == "origin_shift_to_registry"
              ? notice.code + ":twenty_fourths:" +
                    std::to_string(context.origin_shift[0]) + "," +
                    std::to_string(context.origin_shift[1]) + "," +
                    std::to_string(context.origin_shift[2])
              : notice.code);
  }
  if (source.normalization.empty())
    source.normalization.push_back("none");
  return {std::move(structure), std::move(metadata), std::move(provenance),
          std::move(context.notices), std::move(source)};
}
} // namespace

std::string_view classification_name(BlockClassification value) noexcept {
  switch (value) {
  case BlockClassification::metadata_only:
    return "metadata_only";
  case BlockClassification::complete_structure:
    return "complete_structure";
  case BlockClassification::partial_structure:
    return "partial_structure";
  case BlockClassification::placeholder_structure:
    return "placeholder_structure";
  case BlockClassification::invalid_structure:
    return "invalid_structure";
  }
  return "invalid_structure";
}

std::vector<StructureBlockCandidate>
structure_candidates(const Document &document, ImportPolicies policies) {
  std::vector<StructureBlockCandidate> result;
  for (const auto &block : document.blocks) {
    StructureBlockCandidate c;
    c.block_id = ascii_lower(block.name);
    c.block_name = block.name;
    Context context{document, block, {}};
    constexpr std::array names{"_cell_length_a",   "_cell_length_b",
                               "_cell_length_c",   "_cell_angle_alpha",
                               "_cell_angle_beta", "_cell_angle_gamma"};
    std::array<double, 6> cellValues{};
    std::size_t validCellFields = 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (const auto *v = block.scalar(names[i])) {
        c.cell_present = true;
        c.cell_parameters[i] = *v;
        auto numeric = number(*v);
        if (const auto *n = std::get_if<double>(&numeric)) {
          cellValues[i] = *n;
          ++validCellFields;
        }
      }
    }
    if (validCellFields == 6)
      c.cell_valid = bool(crystallography::UnitCell::create(
          {{cellValues[0], cellValues[1], cellValues[2]},
           {cellValues[3], cellValues[4], cellValues[5]}}));
    for (const auto &item : block.items) {
      c.source_metadata.emplace(item.tag.name, item.value);
      if (item.tag.name.starts_with("_symmetry_") ||
          item.tag.name.starts_with("_space_group_"))
        c.space_group_declarations.emplace(item.tag.name, item.value);
    }
    c.explicit_operations =
        block.column("_symmetry_equiv_pos_as_xyz").has_value() ||
        block.column("_space_group_symop_operation_xyz").has_value() ||
        block.scalar("_symmetry_equiv_pos_as_xyz") ||
        block.scalar("_space_group_symop_operation_xyz");
    try {
      const auto resolved = group(context);
      c.registry_setting = resolved.setting().id;
      c.normalized_identifier = resolved.hermann_mauguin();
      c.registry_choice = resolved.setting().choice;
      c.operation_count = resolved.operations().size();
    } catch (const ImportIssue &error) {
      c.symmetry_rejection = error;
    }
    bool atomFields = false;
    for (const auto &item : block.items)
      atomFields = atomFields || item.tag.name.starts_with("_atom_site_");
    for (const auto &loop : block.loops) {
      bool coordinateLoop = false;
      for (const auto &tag : loop.tags) {
        atomFields = atomFields || tag.name.starts_with("_atom_site_");
        coordinateLoop = coordinateLoop ||
                         tag.name.starts_with("_atom_site_fract_") ||
                         tag.name.starts_with("_atom_site_cartn_");
        if (tag.name == "_atom_site_u_iso_or_equiv" ||
            tag.name == "_atom_site_b_iso_or_equiv" ||
            tag.name == "_atom_site_aniso_u_11" ||
            tag.name == "_atom_site_aniso_b_11" ||
            tag.name == "_atom_site_aniso_beta_11")
          c.adp_summary[tag.name] = loop.rows();
      }
      if (!coordinateLoop)
        continue;
      c.atom_site_count += loop.rows();
      std::array<std::optional<LoopColumn>, 3> coords;
      for (std::size_t i = 0; i < 3; ++i)
        coords[i] = block.column(std::string("_atom_site_fract_") + "xyz"[i]);
      if (!std::all_of(coords.begin(), coords.end(),
                       [](auto &v) { return v.has_value(); }))
        for (std::size_t i = 0; i < 3; ++i)
          coords[i] = block.column(std::string("_atom_site_cartn_") + "xyz"[i]);
      for (std::size_t row = 0; row < loop.rows(); ++row) {
        bool missing = false, malformed = false, placeholder = true;
        for (const auto &col : coords) {
          placeholder = placeholder && col && col->loop == &loop &&
                        col->at(row).missing();
          if (!col || col->loop != &loop || col->at(row).missing())
            missing = true;
          else if (!std::holds_alternative<double>(number(col->at(row))))
            malformed = true;
        }
        if (placeholder)
          ++c.placeholder_coordinate_count;
        if (missing)
          ++c.missing_coordinate_count;
        else if (malformed)
          ++c.malformed_coordinate_count;
        else
          ++c.usable_coordinate_count;
        const auto occ = block.column("_atom_site_occupancy");
        if (occ && occ->loop == &loop && !occ->at(row).missing() &&
            std::holds_alternative<double>(number(occ->at(row))))
          ++c.occupancy_present_count;
      }
    }
    if (c.atom_site_count == 0) {
      if (const auto labels = block.column("_atom_site_label")) {
        c.atom_site_count = labels->loop->rows();
        c.missing_coordinate_count = c.atom_site_count;
      }
    }
    if (c.atom_site_count && c.adp_summary.empty())
      c.adp_summary["unknown"] = c.atom_site_count;
    c.occupancy_complete =
        c.atom_site_count && c.occupancy_present_count == c.atom_site_count;
    if (!atomFields) {
      c.classification = BlockClassification::metadata_only;
      c.rejection = context.issue("missing_structure", "_atom_site_fract",
                                  "Metadata block has no atom sites");
    } else if (c.atom_site_count &&
               c.placeholder_coordinate_count == c.atom_site_count) {
      c.classification = BlockClassification::placeholder_structure;
      c.rejection = context.issue(
          "missing_coordinates", "_atom_site_fract",
          "No row has usable coordinates; missing values are not zero");
    } else {
      try {
        auto imported = import_block(document, block, policies);
        c.importable = true;
        c.cell_valid = true;
        c.provenance = std::move(imported.provenance);
        c.registry_setting = c.provenance->registry_setting;
        c.normalized_identifier = c.provenance->normalized_identifier;
        c.registry_choice = c.provenance->registry_choice;
        c.operation_count = c.provenance->operation_count;
        c.adp_summary.clear();
        for (const auto &site : c.provenance->sites)
          ++c.adp_summary[std::string(displacement_name(site.displacement))];
        c.classification = c.occupancy_complete &&
                                   !c.adp_summary.contains("unknown") &&
                                   !c.missing_coordinate_count
                               ? BlockClassification::complete_structure
                               : BlockClassification::partial_structure;
      } catch (const ImportIssue &error) {
        c.rejection = error;
        c.provenance = error.provenance;
        c.classification = c.missing_coordinate_count || !c.atom_site_count
                               ? BlockClassification::partial_structure
                               : BlockClassification::invalid_structure;
      }
    }
    result.push_back(std::move(c));
  }
  return result;
}

ImportResult<ImportedCrystal> import_cif(const Document &document,
                                         BlockSelection selection,
                                         ImportPolicies policies) {
  if ((policies.source_ueq != SourceUeqPolicy::strict &&
       policies.source_ueq != SourceUeqPolicy::zincite_2300112_v1) ||
      (policies.formula_z != FormulaZPolicy::strict &&
       policies.formula_z != FormulaZPolicy::source_metadata_v1))
    return ImportIssue{"unknown_import_policy",
                       document.source,
                       {},
                       "policy",
                       {},
                       "Unknown explicit import policy",
                       {},
                       {},
                       {}};
  auto candidates = structure_candidates(document, policies);
  std::optional<std::size_t> selected;
  if (selection.policy == BlockSelectionPolicy::specified_block) {
    for (std::size_t i = 0; i < candidates.size(); ++i)
      if (candidates[i].block_id == ascii_lower(selection.block_id))
        selected = i;
    if (!selected)
      return ImportIssue{"unknown_block",
                         document.source,
                         selection.block_id,
                         "data_",
                         {},
                         "Specified block does not exist",
                         {},
                         {},
                         std::move(candidates)};
  } else if (selection.policy == BlockSelectionPolicy::require_unique &&
             selection.block_id.empty()) {
    for (std::size_t i = 0; i < candidates.size(); ++i) {
      if (!candidates[i].importable)
        continue;
      if (selected)
        return ImportIssue{
            "multiple_structure_blocks",
            document.source,
            document.blocks[i].name,
            "data_",
            document.blocks[i].location,
            "Multiple importable structures require explicit block selection",
            {},
            {},
            std::move(candidates)};
      selected = i;
    }
    if (!selected) {
      // A single failing structure preserves its precise pre-existing
      // diagnostic.
      std::vector<std::size_t> structures;
      for (std::size_t i = 0; i < candidates.size(); ++i)
        if (candidates[i].classification != BlockClassification::metadata_only)
          structures.push_back(i);
      if (structures.size() == 1) {
        auto error = *candidates[structures[0]].rejection;
        error.candidates = std::move(candidates);
        return error;
      }
      return ImportIssue{"no_importable_structure_blocks",
                         document.source,
                         {},
                         "data_",
                         {},
                         "No importable structure block",
                         {},
                         {},
                         std::move(candidates)};
    }
  } else
    return ImportIssue{"invalid_block_selection",
                       document.source,
                       selection.block_id,
                       "data_",
                       {},
                       "Selection policy and block id disagree",
                       {},
                       {},
                       std::move(candidates)};
  if (!candidates[*selected].importable) {
    auto error = *candidates[*selected].rejection;
    error.candidates = std::move(candidates);
    return error;
  }
  try {
    return import_block(document, document.blocks[*selected], policies);
  } catch (const ImportIssue &error) {
    return error;
  }
}
ImportResult<ImportedCrystal>
import_for_theoretical_card(const Document &document) {
  return import_cif(document, {BlockSelectionPolicy::require_unique, {}});
}
} // namespace qualpf::io::cif
