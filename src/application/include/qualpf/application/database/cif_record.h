#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace qualpf::database {

struct CifLineRecord {
  std::array<std::int32_t, 3> hkl{};
  std::int32_t multiplicity = 0;
  double d = 0;
  double relative_intensity = 0;
};

// Plain storage boundary. Numerical values come from a validated card; no
// Fortran buffers, hidden crystal state, database handles or Qt types.
struct CifRecord {
  std::int32_t identity = 0;
  std::string source;
  std::string block;
  std::string source_sha256;
  std::string structure_identity;
  std::string provenance; // qualpf.cif.structure-provenance.v1 ledger
  std::string name;
  std::string mineral;
  std::string formula;
  std::string space_group;
  std::string crystal_system;
  std::string subfile;
  std::vector<std::string> elements;
  std::int32_t atom_count = 0;
  std::int32_t reflection_count = 0;
  std::int32_t formula_units = 0;
  std::array<double, 6> cell{};
  double volume = 0;
  double density = 0;
  double linear_absorption = 0;
  double rir = 0;
  // Descending card intensity, with the card's existing stable tie order.
  // The writer must not change selection, merge d values or choose new ties.
  std::vector<CifLineRecord> lines;
};

struct WriteError {
  std::string code;
  std::string source;
  std::string block;
  std::string field;
  std::string reason;
};

} // namespace qualpf::database
