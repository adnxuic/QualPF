# Repository layout

| Area | Current responsibility |
| --- | --- |
| `CHANGELOG.md` | Ongoing record of notable completed features, fixes, optimizations and maintenance |
| `apps/qualpf` | Explicit GUI/CLI composition and entry points |
| `src/core`, `src/application` | Qt-free domain/numerical kernels, canonical state, engine/use cases and ports |
| `src/infrastructure`, `src/gui` | SQL/files/codecs/settings providers; Qt views/intents/presenter/task adapters |
| `tests/support/snapshot`, `src/gui/reference` | Test-only binary32 snapshot transport and GUI presentation helpers; production compat targets are retired |
| `tests/unit`, `tests/integration`, `tests/product`, `tests/profile` | Unit, SQL/GUI integration, workflows and isolated profile properties |
| `tests/compatibility`, `tests/reference_checks` | Current C++ numerical replay and independent comparison/protocol tests |
| `tests/data`, `tests/regression/references`, `tests/reference` | Stable independent inputs/provenance and their hash manifest |
| `cmake/modules`, `cmake/templates`, `cmake/packaging` | Build policy/dependencies/options, generated headers and Windows/Linux deployment |
| `tools/architecture`, `tools/validation`, `tools/agent`, `tools/crystallography` | Current boundary/build capture, product/baseline validation, Agent context and numerical-table reproduction |
| `share/qualx`, `src/gui/resources`, `deploy`, `third_party` | Consumed runtime/GUI/deployment data and third-party sources; original resource naming/attribution remains |
| `scripts`, `install.sh` | Existing platform build/install/package helpers |
| `docs/user` | User manual: installation, operation, parameters and result interpretation |
| `docs/developer` | Developer guides: architecture, scientific contracts, formats, build and test |
| `AGENTS.md`, `.agent` | Agent working rules, task-specific navigation and one mutable handoff; product acceptance lives in project-baseline.json |
| `history` | Preserved historical reports and evidence, with a recovery and relocation index |
| Retained acceptance, consolidation and dated maintenance reports | Historical snapshots indexed by migration history; no Phase workflow for new development |
| `history/refactoring/deferred-defects` | Nine conservatively retained original investigation records; no current execution dependency |
| `build`, `releases` | Ignored local builds/logs/packages; never source prerequisites |
| `references` | Ignored local design drafts; current contracts belong in `docs`, frozen scientific references in `tests` |

Public headers remain module-local. `.inc` tables are required C++ source data.
Same-name test helpers with different behavior remain beside their consuming probes;
they are not removed as duplicates without proving their consumer contracts.
No empty template directories are required. Existing consumed resource paths and
runtime installation inputs remain unchanged. Source packages exclude local Git
metadata and generated Python caches. See [architecture](architecture/overview.md),
[building](building.md), [testing](../../tests/README.md) and
[changelog](../../CHANGELOG.md). Retained refactoring and maintenance records are
indexed in [migration history](../../history/README.md).

## Publication boundary

Publish the tracked product, build/deployment inputs, tests/tools, runtime data,
documentation, license and repository rules. Keep generated C++ `.inc` tables,
third-party notices, test scripts and frozen reference manifests. The 15 sealed
reference ZIPs are retained in the protected local branch/tag and the pinned
pre-integration main ancestor, outside the final main tree and source packages.
Provision them before regression checks with the hash-checked
[restoration tool](../../tools/validation/restore_reference_archives.py); see
[test prerequisites](../../tests/README.md#independent-references).
Do not ignore scientific extensions or archives globally.

Local `build/`, `build-*/`, `build_*/`, `cmake-build-*/`, `releases/`, `DB/`,
`.evidence/`, `references/`, SDK presets, generated manuals and installers stay
outside commits. Only `CoO.cif` is published from `tests/real_example_data/`.
Ignoring an output preserves its bytes locally; it does not remove tracked files
or historical Git objects.

Visual Studio CMake outputs in root `out/`, machine-local `CMakeSettings.json`,
root `.cache/` and Python `.mypy_cache/` / `.ruff_cache/` directories are also
excluded from Git and source packages. Keep shared editor configuration and
scientific `.lib`, `.out`, `.zip`, database and measured-input files eligible
for explicit review; broad extension exclusions would hide required inputs.

Earlier repository publication and integration instructions are preserved in
[the original layout record](../../history/development/repository-layout.md).
They describe their authorized historical tasks, not a pending publication queue.
