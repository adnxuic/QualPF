# Repository consolidation batches

Phase37E changes organization and retires migration infrastructure only. Entry:
`8fdf0a71d7eef8b6801e5336720c19b3c936e18f`; protected by local annotated tag
`phase37e-entry-20261009`. Branch: `codex/phase37e-repository-consolidation`.
No push, history rewrite, remote change, LFS/Git pruning or PFSM is authorized.
Exit is `READY_FOR_MILESTONE_A_ACCEPTANCE`, not final Milestone A acceptance.

| Batch | Risk | Scope | Exit |
| --- | --- | --- | --- |
| E1 | A | Entry, classification, immutable recovery reference and real archive checks | Complete |
| E2 | B/C | Current reference organization, ownerless probes, historical-only tools/registration | Complete |
| E3/E4 | A/C | Historical separation and coupled current guides/context/build/CI cleanup | Complete |
| E5 | C | Fresh checkout, complete current suites on three configurations, architecture and F preservation | Complete |

## E1 — archive readiness

`FORTRAN_MIGRATION_COMPLETE` is accepted at implementation `f386bfa4`.
Phase36E's restored closure is `F_PRESERVED_AFTER_HYGIENE`. Phase37A-D has
independently accepted `CPP_ARCHITECTURE_REFACTOR_COMPLETE` at `2610439f`,
with all 16 batches complete. Entry boundary checks pass; product sources have
no uncommitted changes. The unrelated untracked settings proposal is excluded.

FRESH_VALIDATION: all 165 current catalogs equal their external copies. All
1,329 LFS pointers resolve to actual ZIP bytes on D, verified by complete payload
SHA-256/length and member manifest/directory identities against catalogs.
Actual external payloads total 12.200 GiB; pointers alone were not accepted as
archives. Six containing objects were fully checked by the existing restore
tool and six selected members recovered/hash-verified in an isolated directory.
The original failed sample attempts (wrong hygiene filename/insufficient catalog
selection) remain in `build/phase37e/restoration*.log`; corrected recovery passes.

The 169 current frozen-reference hashes and existing source fitness/context
checks pass. [Inventory](inventory.json) classifies all 3,905 entry tracked files;
[recovery index](recovery-index.json) binds archive locations and sample hashes.
No active file has yet been removed. No new archive system or duplicate evidence
package was created. The original Git bundle and external archive remain untouched.

Later batches preserve all numerical expected values, tolerances, assertions,
product sources, QXP/SQL contracts and search execution. Historical checks bind
their sealed snapshots, not mutable current agent files. Only genuinely affected
targets/tests run per batch; one complete fresh three-platform checkpoint closes E5.

## E2 — current regressions and tool retirement

137 byte-identical reference/provenance files move into `tests/regression/references`;
the unchanged 169-input manifest binds their identities. Existing reference lookup
now resolves stable dataset locations. Frozen source comparisons stay inert `.txt`
resources; live numerical provider checks and every numerical/protocol assertion
remain. Historical names in capture manifests describe provenance, not execution.

29 listed reference fixtures had previously been ignored rather than tracked.
They are now explicitly versioned at stable locations. Nine match real external
members by content; the remaining 20 retain their original manifest hashes in
the current asset set and the protected E2 reference snapshot. None is discarded
or regenerated. This closes a clean-checkout gap without new expected values.

Retired: phase DAG/control tests, one-time architecture acceptance verifier,
historical bundle/audit/review-budget tools, unbuilt CIF writer capture main and
optional history CTest registration. The protected entry retains every retired
tool. Six useful validators/build runners remain under generic names. The unused
transitional CMake forwarding function is removed after its only remaining
consumers became inert reference text. Current CI checks context, boundaries,
product identity and independent references without historical downloads.

FRESH_VALIDATION: 169 byte hashes; registry reproduction; boundary and context
positive/negative checks; current product identity; GNU incremental configure/build.
25 affected normal reference/protocol checks pass (21 initial and four Pattern
reruns after supplying the existing Qt DLL PATH). Original loader-failure logs
and corrected logs/XML remain in `build/phase37e`. No tolerance, assertion or
production source change. The original 80 normal registrations remain intact.

## E3/E4 — historical separation and current project guides

These documentation/context batches are combined because retirement and current
link replacement share the same consumers. 2,076 tracked historical copies exit
the active tree (8,296,157 physical worktree bytes, mostly small LFS pointers and
compressed reports). Tag blob existence and unchanged pending-deletion hashes
are verified before unlinking. External payload storage remains untouched.
No Git/LFS object is pruned, no archive is repacked and no remote is changed.

Current architecture/decisions/compatibility, numerical policy, README,
development/test instructions and `migration-history.md` describe the actual
C++ project. The preserved 80-entry correspondence is `tests/coverage.json`.
AGENTS/.agent now describe current layers, numerical/state/thread/Git rules and
one current task state, without a Phase31-36 queue or repeated acceptance protocol.
Four completed migration control/context files retire; their originals stay at
the entry tag. Stale attribute rules exit, existing numerical byte rules remain.

Nine small original defect/investigation records remain UNCERTAIN/KEEP: they are
not current reproduced blockers or build/test/agent inputs. Consumed precision,
settings import, GUI utilities, test observations and same-name distinct probe
helpers remain with documented purposes. All current production sources and
deployment/install/resource inputs are unchanged; no architectural repair is made.

FRESH_VALIDATION: current context/identity/boundary negative controls, all 169
reference identities, 28 current Markdown file-link checks and whitespace/diff.
Normal source/build/test registration still has 80 entries. CI is current quality
validation, not a migration review budget. Full fresh three-platform checkout
validation follows in E5; no prior result is presented as that checkpoint.

## E5 — clean baseline verification

FRESH_VALIDATION: independent checkout at `d2915b0b`, empty build/cache starts,
byte-verified Linux SDK mirror, actual Windows GNU/MSVC and Linux GNU product
builds, standalone Qt-free Application clients and complete 80/80 normal suites
on each configuration, zero skip/failure. Source boundaries/negative controls,
169 original references and current F preservation pass. MSVC origin verification
also scans 317 actual COFF/archive files and 1,510,321 symbol lines.

Real TGZ inspection found local worktree Git metadata and Python caches. One
source-package exclusion correction follows. All three configurations actually
reconfigure/build with unchanged compile commands, 69/69/70 binaries and normal
test commands/properties; full numerical/product results are explicitly reused
with unchanged dependencies. Committed implementation `85052dff` produces an
actual package matching 1,859 checkout inputs and all 169 reference hashes.
GNU isolated install/CLI/SQLite/resource checks pass; accepted unchanged installer
configuration is reused, without claiming an unexecuted provider matrix.

All 2,089 retired original tracked bytes and the protected 169-reference snapshot
were actually recovered/hash-matched. Original failed restore/loader/probe
observations remain with corrected results. [Closure](closure.md) and
[validation index](validation-index.json) provide the current handoff and raw logs.
Outcome: `READY_FOR_MILESTONE_A_ACCEPTANCE`; final Milestone A remains pending.
PFSM remains NOT_STARTED. STOP.
