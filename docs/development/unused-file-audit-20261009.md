# Unused-file audit: 2026-10-09

All **2,177 tracked entry files** were inventoried against current build targets,
includes, Python imports, Qt UI/resources, installation inputs and documentation
owners. Four unconsumed files were removed in
`3e1efc3c63aef088b5d6c5ac5822eff3e3c20f07`, starting from
`ca8e830fb4c58f29e528989406c973e96f1faa5a`.
This was maintenance; it changed no compiled numerical implementation, public
header, assertion, tolerance, schema or execution strategy.

The local per-file inventory, sizes, hashes, consumers and decisions are
`build/unused-file-audit/inventory.json` and `decisions.csv`. These generated
records and validation logs are ignored local artifacts, independent of normal
build/test prerequisites. The audit covered tracked source inputs; unrelated
user files, local SDKs, builds, original evidence and Git/LFS objects were retained.

| Entry ownership | Files |
| --- | ---: |
| Product modules | 523 |
| Tests and test support | 200 |
| Numerical fixtures and protected evidence areas | 1,324 |
| Retained unresolved historical investigations | 9 |
| Documentation and Agent authority | 49 |
| Runtime resources, deployment and third party | 19 |
| Repository and development entry points | 53 |
| Total | 2,177 |

| Removed file | Evidence |
| --- | --- |
| `benchmarks/CMakeLists.txt` | Two comments; no package, executable, library or test. Its root subdirectory entry was removed. |
| `benchmarks/README.md` | Unimplemented benchmark plan with an obsolete history link; no current benchmark workflow. |
| `src/core/numerics/first_index_compatibility.h` | Private inline helper with no include, configured source or symbol consumer. |
| `src/gui/resources/images/qualx_icon.svg` | Absent from QRC, Windows RC and installation inputs. MkDocs uses the retained byte-identical `docs/user/images/qualx_icon.svg`. |

The obsolete `src/compat` attribute rules and removed archive build-script ignore
exceptions were also dropped. No active file's attribute policy changed.

Apparent unused Python modules were resolved through actual imports. The range
spinbox header is consumed by Qt Designer. The CIF constraint generator remains
a working manual reproducer of the compiled accepted table, verified with
`--check`. Manual Windows/source-package entry points, public include paths,
separately consumed identical assets/helpers and original numerical provenance
remain. This ownership audit does not claim that every function in a compiled
module is executed.

| Validation | Actual result |
| --- | --- |
| Windows GCC and MSVC | Reconfigure and explicit `qualpf` build pass. Only the mandatory gate ran; no C++ recompilation. Both complete executables remain byte-identical. |
| Linux GCC | Qualified Ubuntu 26.04.1 SDK, GCC 15.2.0, Qt 6.10.2, CMake 4.2.3 and Ninja 1.13.2. Empty native build directory; configure, 224-step `qualpf` build and CLI `--help` pass. |
| Configured dependencies | All three compile-command sets and 95-target graphs are identical before/after cleanup. Public headers and Qt-free Core/Application boundaries are preserved. |
| Strict/source checks | Pass, including negative controls on all three configured graphs. The operation index changes from 136 to 135 by removing the unused helper declaration. |
| Normal tests | All 86 identities match `tests/coverage.json`; commands, timeouts and execution properties are preserved. Four selected original tests pass on each configuration, zero failures/skips. |
| Scientific inputs | All 169 reference lengths/SHA-256 identities pass; all four table generators pass `--check`. |
| Actual source TGZ | All 2,173 validation-snapshot files match their source bytes, including all 169 reference identities. Removed files and unrelated user files are absent. |
| Linux binary/runtime | Executable `.text` is identical. Inspected ELF differences are confined to 165 RCC resource-timestamp bytes and the resulting build ID. Non-RCC data is unchanged; runtime providers resolve without Fortran libraries. |
| Preservation | All other entry files and unrelated user files match the inventory, apart from declared configuration/task and documentation-navigation updates. Original acceptance/FAIL records are unchanged. |

The selected normal tests were `qualpf.card.acceptance_gate`,
`qualpf.powder.protocol_integrity`, `qualpf.scattering.protocol_integrity` and
`qualpf.math.current_reference_protocol`. The full 86-test suite, standalone
public-header clients and installed DEB/runtime matrix were **NOT_RUN** for this
maintenance batch. Their original records retain their original snapshot scope.
The inspected source TGZ predates this closure document and final handoff update.

Two initial verification observations are preserved locally. Whole generated
CTest files change when the empty benchmark traversal is removed and the root
strict-gate call moves from line 32 to 31. Restoring only those two expected
changes reproduces the original file hashes; every execution contract remains.
The first Linux attempt waited in the WSL 9P filesystem lookup during generation;
its process stack and log were retained before stopping that audit process.
Native Linux source/build directories resolved it without a product change.

Recorded F/A and strict-layer acceptance remain intact in their original scopes.
PFSM is NOT_STARTED. The maintenance task is complete.
