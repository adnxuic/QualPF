#include "cif_import.h"
#include "cif_parser.h"
#include <bit>
#include <charconv>
#include <cstdint>
#include <limits>

namespace qualpf::io::cif {
std::string sha256(std::string_view bytes) {
  constexpr std::array<std::uint32_t, 64> k{
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
      0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
      0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
      0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
      0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
      0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
      0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
      0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
      0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
      0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
  std::array<std::uint32_t, 8> h{0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                 0xa54ff53a, 0x510e527f, 0x9b05688c,
                                 0x1f83d9ab, 0x5be0cd19};
  const std::uint64_t bitCount = static_cast<std::uint64_t>(bytes.size()) * 8;
  const auto padded = (bytes.size() + 9 + 63) / 64 * 64;
  for (std::size_t offset = 0; offset < padded; offset += 64) {
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 64; ++i) {
      const auto pos = offset + i;
      const std::uint32_t b =
          pos < bytes.size()    ? static_cast<unsigned char>(bytes[pos])
          : pos == bytes.size() ? 128
          : pos >= padded - 8
              ? static_cast<unsigned>((bitCount >> (8 * (padded - 1 - pos))) &
                                      255)
              : 0;
      w[i / 4] |= b << (24 - 8 * (i % 4));
    }
    for (std::size_t i = 16; i < 64; ++i) {
      const auto a = w[i - 15], b = w[i - 2];
      w[i] = w[i - 16] + (std::rotr(a, 7) ^ std::rotr(a, 18) ^ (a >> 3)) +
             w[i - 7] + (std::rotr(b, 17) ^ std::rotr(b, 19) ^ (b >> 10));
    }
    auto [a, b, c, d, e, f, g, v] = h;
    for (std::size_t i = 0; i < 64; ++i) {
      const auto t1 = v +
                      (std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25)) +
                      ((e & f) ^ ((~e) & g)) + k[i] + w[i];
      const auto t2 = (std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22)) +
                      ((a & b) ^ (a & c) ^ (b & c));
      v = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    const std::array work{a, b, c, d, e, f, g, v};
    for (std::size_t i = 0; i < h.size(); ++i)
      h[i] += work[i];
  }
  std::string out;
  constexpr char hex[] = "0123456789abcdef";
  for (auto n : h)
    for (int shift = 28; shift >= 0; shift -= 4)
      out += hex[(n >> shift) & 15];
  return out;
}
std::string_view displacement_name(SourceDisplacement value) noexcept {
  switch (value) {
  case SourceDisplacement::unknown:
    return "unknown";
  case SourceDisplacement::isotropic_b:
    return "Biso";
  case SourceDisplacement::isotropic_u:
    return "Uiso";
  case SourceDisplacement::anisotropic_b:
    return "B_aniso";
  case SourceDisplacement::anisotropic_u:
    return "U_aniso";
  case SourceDisplacement::anisotropic_beta:
    return "beta_aniso";
  }
  return "unknown";
}
namespace {
void frame(std::string &out, std::string_view text) {
  out += std::to_string(text.size()) + ":";
  out += text;
}
void value_frame(std::string &out, const Value &v) {
  frame(out, v.text);
  frame(out, v.raw);
  frame(out, std::to_string(static_cast<int>(v.kind)));
  frame(out, std::to_string(v.location.offset));
  frame(out, std::to_string(v.location.line));
  frame(out, std::to_string(v.location.column));
}
void values_frame(std::string &out,
                  const std::map<std::string, Value> &values) {
  frame(out, std::to_string(values.size()));
  for (const auto &[key, value] : values) {
    frame(out, key);
    value_frame(out, value);
  }
}
ImportIssue ticket_error(std::string code) {
  return {std::move(code),
          {},
          {},
          "selection_ticket",
          {},
          "Invalid or stale CIF selection ticket",
          {},
          {},
          {}};
}
} // namespace
std::string StructureProvenance::identity() const {
  // Length framing avoids delimiter collisions in legal CIF block names.
  auto result = "sha256:" + source_sha256 +
                "/block:" + std::to_string(block_id.size()) + ":" + block_id;
  if (!source_ueq_policy.empty() || !formula_z_policy.empty())
    result += "/policy:" + sha256(source_ueq_policy + "\n" + formula_z_policy);
  return result;
}
std::string_view formula_z_state_name(FormulaZState state) noexcept {
  switch (state) {
  case FormulaZState::consistent:
    return "CONSISTENT";
  case FormulaZState::metadata_discrepancy:
    return "METADATA_DISCREPANCY";
  case FormulaZState::inconsistent:
    return "INCONSISTENT";
  case FormulaZState::insufficient_metadata:
    return "INSUFFICIENT_METADATA";
  }
  return "UNKNOWN";
}
std::string provenance_ledger(const StructureProvenance &p) {
  std::string out;
  for (const auto *v :
       {&p.schema, &p.source, &p.source_sha256, &p.selected_block, &p.block_id,
        &p.source_profile, &p.symmetry_source, &p.normalized_identifier,
        &p.registry_choice, &p.registry_transform, &p.calculation_policy})
    frame(out, *v);
  frame(out, std::to_string(p.registry_setting));
  frame(out, std::to_string(p.operation_count));
  values_frame(out, p.space_group_declarations);
  values_frame(out, p.selected_metadata);
  frame(out, std::to_string(p.sites.size()));
  for (const auto &s : p.sites) {
    frame(out, std::to_string(s.coordinate_row));
    frame(out, s.label);
    frame(out, s.occupancy_source);
    frame(out, s.occupancy ? "present" : "absent");
    if (s.occupancy)
      value_frame(out, *s.occupancy);
    frame(out, displacement_name(s.displacement));
    values_frame(out, s.displacement_values);
    frame(out, s.isotropic_row ? std::to_string(*s.isotropic_row) : "absent");
    frame(out,
          s.anisotropic_row ? std::to_string(*s.anisotropic_row) : "absent");
    for (const auto &v : s.coordinates)
      value_frame(out, v);
    values_frame(out, s.site_symmetry_declarations);
    const auto &e = s.site_symmetry;
    for (const auto *v :
         {&e.policy_version, &e.state, &e.reason, &e.constraint_source,
          &e.naming_source, &e.normalized_wyckoff})
      frame(out, *v);
    frame(out, e.constraint_multiplicity
                   ? std::to_string(*e.constraint_multiplicity)
                   : "absent");
    for (const auto &c : e.coordinates) {
      for (const auto *v : {&c.nominal, &c.standard_uncertainty, &c.quantum,
                            &c.interval_lower, &c.interval_upper})
        frame(out, *v);
      frame(out, std::to_string(c.decimal_places));
      frame(out, std::to_string(c.exponent));
    }
    for (const auto *list :
         {&e.nominal_fractional, &e.reconciled_fractional, &e.delta})
      for (const auto &v : *list)
        frame(out, v);
    frame(out, std::to_string(e.nominal_orbit));
    frame(out, std::to_string(e.reconciled_orbit));
    frame(out, std::to_string(e.supported_candidates));
    for (const auto *list : {&e.nominal_stabilizer, &e.reconciled_stabilizer}) {
      frame(out, std::to_string(list->size()));
      for (auto v : *list)
        frame(out, std::to_string(v));
    }
  }
  for (const auto *list : {&p.normalization, &p.notices}) {
    frame(out, std::to_string(list->size()));
    for (const auto &s : *list)
      frame(out, s);
  }
  if (p.schema == "qualpf.cif.structure-provenance.v3") {
    frame(out, p.source_ueq_policy);
    frame(out, p.formula_z_policy);
    frame(out, p.strict_attempt_ledger);
    frame(out, std::to_string(p.displacement_choices.size()));
    for (const auto &choice : p.displacement_choices) {
      for (const auto *v :
           {&choice.policy_version, &choice.site, &choice.source_field,
            &choice.unit, &choice.strict_field, &choice.strict_reason,
            &choice.strict_message, &choice.certificate})
        frame(out, *v);
      value_frame(out, choice.source_value);
    }
    frame(out, p.formula_z ? "present" : "absent");
    if (p.formula_z) {
      const auto &f = *p.formula_z;
      frame(out, f.policy_version);
      frame(out, formula_z_state_name(f.state));
      for (const auto *v : {&f.source_formula, &f.source_z}) {
        frame(out, *v ? "present" : "absent");
        if (*v)
          value_frame(out, **v);
      }
      for (const auto *totals :
           {&f.site_cell_composition, &f.formula_times_z}) {
        frame(out, std::to_string(totals->size()));
        for (const auto &[element, amount] : *totals) {
          frame(out, std::to_string(element));
          std::array<char, 64> buffer{};
          const auto result =
              std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                            amount, std::chars_format::general,
                            std::numeric_limits<double>::max_digits10);
          frame(out, std::string_view(buffer.data(), result.ptr));
        }
      }
      frame(out, f.notice);
    }
  }
  return out;
}
namespace {
std::optional<ImportPolicies> policies_from_versions(std::string_view adp,
                                                     std::string_view formula) {
  ImportPolicies result;
  if (!adp.empty()) {
    if (adp != source_ueq_policy_v1)
      return std::nullopt;
    result.source_ueq = SourceUeqPolicy::zincite_2300112_v1;
  }
  if (!formula.empty()) {
    if (formula != formula_z_policy_v1)
      return std::nullopt;
    result.formula_z = FormulaZPolicy::source_metadata_v1;
  }
  return result;
}
} // namespace
ImportResult<std::string> save_selection(const Document &document,
                                         const ImportedCrystal &selected) {
  const auto policies =
      policies_from_versions(selected.provenance.source_ueq_policy,
                             selected.provenance.formula_z_policy);
  if (!policies)
    return ticket_error("unknown_import_policy");
  auto checked = import_cif(
      document,
      {BlockSelectionPolicy::specified_block, selected.provenance.block_id},
      *policies);
  const auto *value = std::get_if<ImportedCrystal>(&checked);
  if (!value || provenance_ledger(value->provenance) !=
                    provenance_ledger(selected.provenance))
    return ticket_error("selection_provenance_mismatch");
  const bool v2 = !selected.provenance.source_ueq_policy.empty() ||
                  !selected.provenance.formula_z_policy.empty();
  std::string out =
      v2 ? "QUALPF_CIF_SELECTION_V2\n" : "QUALPF_CIF_SELECTION_V1\n";
  for (const auto &v :
       {document.source, selected.provenance.source_sha256,
        selected.provenance.block_id, provenance_ledger(selected.provenance),
        document.original_bytes})
    frame(out, v);
  if (v2) {
    frame(out, selected.provenance.source_ueq_policy);
    frame(out, selected.provenance.formula_z_policy);
  }
  return out;
}
ImportResult<ImportedCrystal> reload_selection(std::string_view ticket) {
  constexpr std::string_view header = "QUALPF_CIF_SELECTION_V1\n";
  const bool v2 = ticket.starts_with("QUALPF_CIF_SELECTION_V2\n");
  if (!ticket.starts_with(header) && !v2)
    return ticket_error("invalid_selection_ticket");
  ticket.remove_prefix(header.size());
  std::vector<std::string> fields(v2 ? 7 : 5);
  for (auto &field : fields) {
    const auto colon = ticket.find(':');
    if (colon == std::string_view::npos)
      return ticket_error("invalid_selection_ticket");
    std::size_t length = 0;
    const auto [end, ec] =
        std::from_chars(ticket.data(), ticket.data() + colon, length);
    if (ec != std::errc{} || end != ticket.data() + colon ||
        length > 64 * 1024 * 1024 || length > ticket.size() - colon - 1)
      return ticket_error("invalid_selection_ticket");
    field = ticket.substr(colon + 1, length);
    ticket.remove_prefix(colon + 1 + length);
  }
  if (!ticket.empty())
    return ticket_error("invalid_selection_ticket");
  if (sha256(fields[4]) != fields[1])
    return ticket_error("source_hash_mismatch");
  const auto policies =
      policies_from_versions(v2 ? fields[5] : "", v2 ? fields[6] : "");
  if (!policies)
    return ticket_error("unknown_import_policy");
  auto parsed = parse(fields[4], fields[0]);
  if (!std::holds_alternative<Document>(parsed))
    return ticket_error("invalid_selection_document");
  auto selected =
      import_cif(std::get<Document>(parsed),
                 {BlockSelectionPolicy::specified_block, fields[2]}, *policies);
  if (const auto *value = std::get_if<ImportedCrystal>(&selected)) {
    if (provenance_ledger(value->provenance) != fields[3])
      return ticket_error("selection_provenance_mismatch");
  }
  return selected;
}
} // namespace qualpf::io::cif
