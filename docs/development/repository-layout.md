# Repository layout

| Area | Current responsibility |
| --- | --- |
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
| `docs/architecture`, `docs/numerical`, `docs/development`, `docs/user` | Current architecture, scientific contracts, development guides and manual |
| `AGENTS.md`, `.agent` | Agent working rules, task-specific navigation and one mutable handoff; product acceptance lives in project-baseline.json |
| `docs/migration-history.md` | Protected-tag/external archive recovery, independent of normal builds |
| `docs/refactoring/deferred-defects` | Nine conservatively retained original investigation records; no current execution dependency |
| `build`, `releases` | Ignored local builds/logs/packages; never source prerequisites |

Public headers remain module-local. `.inc` tables are required C++ source data.
Same-name test helpers with different behavior remain beside their consuming probes;
they are not removed as duplicates without proving their consumer contracts.
No empty template directories are required. Existing consumed resource paths and
runtime installation inputs remain unchanged. Source packages exclude local Git
metadata and generated Python caches. See [architecture](../architecture/overview.md),
[building](building.md), [testing](../../tests/README.md) and
[consolidation inventory](repository-consolidation/inventory.json).

The [Agent context maintenance result](agent-governance.md) describes the six-file
context and the separation of task state, product baseline and retained decisions.

The [unused-file audit](unused-file-audit-20261009.md) records the complete tracked
file inventory, bounded removals and preservation checks after Docker retirement.
