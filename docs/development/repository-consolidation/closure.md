# Repository consolidation closure

Outcome: **READY_FOR_MILESTONE_A_ACCEPTANCE**. Objective:
`CLEAN_CPP_PROJECT_BASELINE`. `MILESTONE_F_PRESERVED`; PFSM is NOT_STARTED.
Final independent Milestone A acceptance remains PENDING. Stop here.

## Baseline and scope

Entry: `8fdf0a71d7eef8b6801e5336720c19b3c936e18f`.
Final implementation: `85052dffeaf1f30dbd7698da466be43b3f3159cb`.
Branch: `codex/phase37e-repository-consolidation`.
Tracked files: **3,905 → 1,935**, including the retained current validation logs.
2,089 original tracked active copies retired: 2,076 historical copies and 13
obsolete migration/control/probe files, totaling **8,362,392 worktree bytes
(7.98 MiB)**. The pointer-file total is distinct from external evidence payloads.
Reference relocations are not counted as evidence deletion.

Production C++ sources, numerical implementations, assertions, tolerances,
QXP/SQL schemas, settings import, GUI search execution/lifetime and CLI behavior
are unchanged. No architecture redesign, optimization or PFSM was implemented.

## Current repository

Core/Application and Infrastructure/GUI retain their accepted boundaries and
canonical ownership. See the [architecture map](../../architecture/overview.md),
[decisions](../../architecture/decisions.md) and
[compatibility responsibilities](../../architecture/compatibility-allowlist.json).
Tests retain unit, database/GUI integration, product workflows, profile properties,
C++ compatibility replay and independent reference checks. [Coverage](../../../tests/coverage.json)
preserves all 80 normal identities, including the original 66 regressions.

137 exact reference/provenance assets move into algorithm-oriented regression
locations; 29 formerly ignored fixtures are now explicitly tracked. All **169**
original reference lengths/SHA-256 values match, with no new goldens or expected
values. [Numerical policy](../../numerical/policy.md) and the
[manifest](../../../tests/reference/manifest.json) describe the continuing contract.

Historical-only DAG, one-time acceptance, evidence audit/review-budget/restore
tools, an unbuilt CIF capture main and optional historical CTest registration
retire from the active tree. Useful validators/build runners remain under generic
names. The unused transitional CMake forwarding helper retires. Normal build/test
registration loses no product coverage. Current CI validates context, architecture,
identity and references without historical downloads; hosted CI was not run here.

Current README, build/test guides, architecture/numerical guides and `.agent`
describe C++ development without a migration queue. Consumed resources and
third-party attribution retain their locations. The [layout](../repository-layout.md)
and [build guide](../building.md) are the normal development entries.

## History and real recovery

Local annotated tags `phase37e-entry-20261009` and
`phase37e-regression-resources-20261009` protect the entry and complete reference
set. `D:/QualPF-evidence-backup/20261004` retains the actual historical ZIPs,
catalogs, maps and original Git bundle. No remote availability is claimed.

All 165 catalogs and **1,329 actual ZIP payloads (12.200 GiB)** were checked against
pointer/catalog lengths, SHA-256 and member manifests before active copies exited.
All 2,089 retired tracked originals were recovered/hash-matched from protected
Git blobs using recorded byte rules: 2,062 direct, 26 LF-to-CRLF and one precisely
recorded mixed-EOL file. All 169 current references were actually restored from
the protected reference tag. Six external samples cover a numerical reference,
original raw FAIL, numerical decision, Phase31 migration record, Milestone F
acceptance and Phase36E closure. Original sources, toolchain/license provenance,
decisions, old SHA values and failed comparisons retain their meaning and bytes.

Use [migration history](../../migration-history.md), the [recovery index](recovery-index.json)
and [inventory](inventory.json). Normal development does not restore these archives.
No history rewrite, repack of historical payloads, Git/LFS pruning or upload occurred.

## Actual verification

Fresh product builds and full normal suites used an independent detached checkout
of `d2915b0bd32b3644b00bc343440dd24548571bd6`, with empty build/cache directories.
Linux used a byte-verified copy of all 1,859 checkout files in its isolated SDK;
only Git metadata was mounted separately. No old build, historical runner/evidence
archive or Fortran compiler/runtime was a prerequisite.

| Configuration | Actual compiler / Qt | Product build / full normal suite | Qt-free Application |
| --- | --- | --- | --- |
| Windows x64 GCC | GCC 13.1.0 / Qt 6.8.3 | PASS / 80 of 80, zero skip | PASS |
| Windows x64 MSVC | MSVC 19.40.33811 / Qt 6.8.3 | PASS / 80 of 80, zero skip | PASS |
| Linux x64 GCC | GCC 15.2.0 / Qt 6.10.2 | PASS / 80 of 80, zero skip | PASS |

These are **FRESH_VALIDATION**, preserving existing inputs/assertions/timeouts.
They cover Pattern/CIF/powder-CIF, crystallography, SQLite, preprocessing,
search/ranking/residual, QXP/provenance, GUI lifecycle/background work and CLI.
70 Core/Application compile units per configuration have no Qt contamination;
separate standalone Application clients compile/run without Qt. Current source
fitness/negative controls confirm dependency direction, unique canonical ownership,
shared search engine and zero search event pumps.

Source-package inspection exposed a worktree `.git` file and generated Python
caches in TGZ output. The only subsequent bound-input change is the CPack source
exclusion rule. All three configurations then actually reconfigured/built:
**compile commands, every 69/69/70 checked binary, normal test commands/properties
and 317 MSVC COFF/archive identities are unchanged**. Numerical/product suite
results therefore use **REUSED_UNCHANGED_DEPENDENCIES** after this packaging-only
correction, not an unexecuted second full suite. The committed implementation's
actual source package matches all 1,859 inputs and 169 references, excluding
Git/cache/build/historical evidence data.

Actual compilation is CXX only: 302/299/301 units. Language/cache/link graph,
imports and origin-symbol checks pass. MSVC stripped PE observations are supported
by a real scan of 317 COFF objects/archives, 1,510,321 symbol lines and zero Fortran
origin matches. No Fortran ABI/callback, owned state or provider is reintroduced.
This protects current Milestone F without changing its historical acceptance.

GNU isolated installation, deployed CLI version and CIF-to-SQLite creation pass
using current unchanged fixtures (four files, one card). Existing installer/provider
configuration, scripts and runtime resources are unchanged; their accepted
configuration evidence is reused with those dependencies, without claiming a new
installer/provider matrix. Source packaging itself was freshly verified. Original
loader/recovery failures and staged-probe observations remain beside corrected
results; the fresh normal suites have no failure or skip.

[Validation index](validation-index.json) binds original raw build/test/compile/
artifact observations and their byte-preserving compressed logs. Full local raw
outputs remain under `build/phase37e`; these are not normal-build dependencies.

## Retained items and handoff

Nine small original deferred-investigation records remain UNCERTAIN/KEEP under
`docs/refactoring/deferred-defects`, with reasons in the inventory. They are not
reproduced current blockers or an execution queue. No new architectural deferred
item was introduced. Unrelated user proposal `docs/maintenance/settings-identity/report.md`
and unrelated ignored local work/builds were not modified or deleted.

Milestone A receives the current map/owners/boundaries, normal suite, numerical
references, three supported release configurations and historical recovery index.
PFSM architecture readiness concerns the accepted structure only; no PFSM begins.

Local commits: `f16242b4` (archive readiness), `084347cd` (tests/tools), `d2915b0b`
(historical separation/current guides), `85052dff` (recovery/package correction),
followed by this closure/log/current-state commit. Resolve the last using
`git log -1 -- docs/development/repository-consolidation/closure.md`; no self-hash
is required. No push, remote change or protection-tag deletion. No Phase37E
uncommitted work remains after that commit; the unrelated user proposal remains.
