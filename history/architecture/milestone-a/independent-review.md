# Independent Milestone A review

Outcome: **ACCEPTANCE_BLOCKED**. One reproduced CLI error-boundary defect prevents
final acceptance. Milestone A is not accepted. Milestone F remains preserved;
PFSM is NOT_STARTED. No production correction was made during this review.

Review date: 2026-10-09. Candidate: **`e6883edab2fb26d8f22866c0585b910322027c12`**.
See the [acceptance matrix](acceptance-matrix.md) and [gate/result](gate.json).
The Git commit containing these records is a documentation commit, not a new
implementation or an accepted milestone. Resolve it with
`git log -1 -- docs/architecture/milestone-a/gate.json`.

## Baseline and independent method

Git history identifies `e6883eda` as the actual final Phase37E closure commit.
`d2915b0bd32b3644b00bc343440dd24548571bd6` is the earlier fresh three-platform
test checkpoint; `85052dffeaf1f30dbd7698da466be43b3f3159cb` is the intervening
repository/package implementation. They are distinct baselines.

| Delta | Actual changes | Validation consequence |
| --- | --- | --- |
| `d2915b0b` to `85052dff` | Recovery documentation and CPack source exclusions for a worktree `.git` file and Python caches | Source packaging needs a real new run. All three configurations actually reconfigured/built; compile commands, compiled artifacts and normal test commands/properties stayed identical. |
| `85052dff` to `e6883eda` | Closure, current context, validation index and byte-preserving compressed raw logs | No production, test, compiler-option, runtime-resource or installation input changed. Reuse remains valid for the compiled/tested dependencies. The complete final source package needs to include these final tracked files. |

Production sources also match the accepted Phase37A-D implementation
`2610439f883dba06b609d1c2393c522bda257cfa`. The protected entry is
`8fdf0a71d7eef8b6801e5336720c19b3c936e18f`. Current reports, archived ADR/DAG and
receipts informed the review; their PASS labels were not used as sufficient proof.

Independent checks read actual source and target definitions, decompressed and
hash-checked all 73 indexed raw logs, parsed the actual JUnit/CTest identities,
rechecked 1,775 bound build inputs and 69/69/70 platform artifacts, and rechecked
317 MSVC COFF objects/archives. A detached checkout of the final candidate was
configured and packaged. Existing causal GUI/Application/SQL tests and additional
CLI observations were executed. There was no production change, golden generation,
tolerance change, historical rewrite, push or new architecture work.

## Dependency and ownership review

**Core — PASS.** Module source and CMake dependencies contain no Qt, Application,
GUI or SQLite implementation dependency. Numerical buffers have explicit owners.
Pattern/CIF parsing accepts memory input; Infrastructure supplies file access.
Actual Core/Application compile commands contain no Qt includes or definitions.

**Application — PASS.** `Workspace` owns one `ProductSearchSession` and its
`SearchCoordinator`. `ProductHandle` owns immutable state; GUI `SessionView` is a
borrowed projection with scoped subscriptions. `product_session.cpp` validates
source identity, revision and exhaustion before replacement, and prepares state
and subscriber storage before publication. Preprocessing evaluation uses private
drafts and immutable input; Apply/OK/Cancel, Range and Kalpha2 retain their distinct
no-op, dirty, provenance and invalidation contracts. The complete declared
Application target, not a mock subset, has standalone Qt-free build/run evidence.

**Infrastructure — PASS.** `SqlReferenceRepository` implements the candidate port
and returns owned records. Scoring/ranking and the cancellation source live outside
the database. SQL connection creation, use, closure and removal occur on the
execution thread; names include execution/task/role identity. Read leases exclude
managed writes through queued publication. Pure parser/format logic, codecs,
QXP/file storage and schema/writer responsibilities follow the accepted edges.

**GUI ownership and lifetime — PASS.** The entrypoint constructs Workspace,
providers, borrowing context and MainWindow in lifetime order. Widgets do not own
canonical numeric state. Subscriptions and child drafts release before their
borrowed dependencies. The Qt plot sink checks owner thread and Closing. A refused
close retains the active draft/task; an accepted close stops admission, cancels,
disables actions, unbinds subscribers and drains via the normal event loop before
destruction. Late completion cannot commit, plot or restore actions.

The [compatibility allowlist](../compatibility-allowlist.json) describes consumed
binary32/error/token adapters, Qt views and test-only observations. These do not
reintroduce Fortran state/runtime or a production mirror/AppState. Names alone were
not treated as defects. The two modal database-builder event pumps have no search
or close-drain consumer.

## Search and real workflow evidence

`SearchEngine` is the single synchronous Qt-free evaluation core used by CLI,
tests and the GUI controller. It takes a snapshot and owned candidates, produces
a private prepared result and never publishes state. Initial matching preserves
`score > minimum`, sorting/capping only when over the cap; rescoring preserves
`score > 0 && score >= minimum`, stable ties and the original scale. Scored and
metadata browsing, merge and stored strongest-index routes keep distinct contracts.
Accepting a phase, residual subtraction and related rescoring prepare one compound
publication. Duplicate acceptance does not subtract twice; removing a phase does
not reconstruct consumed residual.

All production GUI search entrances use `SearchTaskController`, including startup,
browsing/constraints, rescoring, residual and phase/card operations. Its dedicated
pool has one worker; Busy rejects another start. Frozen input and floating-point
environment enter the worker, which owns its reader. Progress is coalesced and
queued. `SearchCoordinator` owns `stop_source`, checks cancellation, TaskId,
source/revision, database selection and Closing before owner-thread publication.
Automatic peak preparation commits separately before the search takes a new
snapshot. Later cancellation leaves those already committed peaks intact.

Fresh execution passed **26/26 existing normal tests**, with zero failed/skipped
cases. These include actual `MainWindow` refused/confirmed/queued/child close,
blocked-worker GUI timer/event-loop progress, Busy, progress coalescing, cancellation
before/during work and after queued completion, stale source/database/old task,
worker exceptions, real SQL connection thread traces and queued read leases.
Existing sync/worker bit comparisons include nearest/downward rounding. GUI tests
wait on completion signals. Source and runtime checks show zero search/drain
`processEvents`, GUI blocking waits or forced thread termination. These results
do not imply that unconfigured optional external-database workflow tests ran.

## Blocking finding A-01: failed CLI browse reported as success

**Criterion:** structured errors and real CLI search behavior. **Owner:** CLI
composition in [main.cpp](../../../apps/qualpf/main.cpp), using the allowed
`SynchronousSearchService` adapter. The defect is at lines 321-332 and the success
return at line 360 of the candidate.

The SQL candidate port correctly records failed query execution and returns
`Result<...>{Error{..., "database.query", ...}}`. The shared engine/coordinator
preserves that failure without publishing state. The synchronous Qt adapter then
returns an empty vector and exposes the coordinator error. The metadata-only CLI
branch never reads that status: it prints the vector length and exits successfully.
The pattern-match CLI branch at lines 113-117 does check cancellation/error.

This violates accepted [ADR A37-5](../decisions.md): failure/cancellation/stale
must not become a successful empty search. The allowlist permits the adapter's
responsibility, not silent SQL failure. Source comparison with `185e4e36` confirms
the unchecked metadata branch is inherited; it is not a newly changed numerical
algorithm or a Phase37E packaging regression. Its age does not establish an
approved exception to A37-5. Existing normal tests do not detect this branch.

### Minimal reproduction and positive control

The actual unchanged Windows GCC product was used with isolated INI settings,
offscreen Qt and no user registry/settings edits. Product SHA-256:
`630182276457eb648298d75254b335285d31f431371410ba2258c3306cb672ff`.

1. Create a valid four-file SQLite database using the product `--createdb` command
   and unchanged `tests/data/cif/cod-2300112.cif`. It reports one card, no error,
   exit 0. Configure it as the sole enabled `cod_inorg` database.
2. Run `qualpf --nogui --search --composition Zn --settings-dir <valid-settings>`.
   It prints `Found 1 card(s)` with `zinc_oxide`, exit 0 and empty stderr.
3. In a separate directory create four actual SQLite files with extensions `.sq`,
   `.sq.info`, `.sq.infostat`, `.sq.search`. Each contains only
   `CREATE TABLE placeholder(value INTEGER)`. In `.sq` also create
   `infodb(ncard INTEGER, type TEXT)` and insert `(0, 'cod_inorg')`. Required query
   tables including `chemical`/`id` are absent. Configure this database identically.
4. Run the same Zn query using `<broken-settings>`. Actual output:

```text
Searching database...
Found 0 card(s):
```

Actual exit status is **0**; stderr is **empty**. This is a failed SQL query,
not a successful empty candidate set. The expected boundary behavior is a
non-success exit and inspectable failure diagnostic, without state publication.
A second valid-database control with `--composition Xe` returns zero cards and
exit 0 legitimately. The defective CLI therefore cannot distinguish a real empty
query from the broken-database failure.
The gate preserves the exact commands, settings/schema recipe, stdout, stderr and
their hashes. Raw observations remain under `build/milestone-a/cli-error-002`.
An earlier no-filter probe returned a legitimate empty browse and failed its
one-card control assumption; that original observation remains preserved. The
reported defect uses the corrected explicit Zn query and passing positive control.

**Bounded correction recommendation:** the CLI owner should handle failure and
cancellation after metadata browsing, before printing a successful result; add a
causal CLI regression using an actual failed SQL query and a valid-empty/control
case. Reconcile the inherited exit behavior with A37-5 in that separate correction
batch, then independently revalidate this boundary on the supported configurations.
No scorer, schema, Session, thread model or GUI lifecycle redesign is indicated.
This review did not implement the correction or approve a revised candidate.

## Numerical, product and platform preservation

All **169 reference entries** match protected authority in byte length and SHA-256,
including preserved duplicate provenance entries; they are not 169 newly generated
datasets. Actual reference contents were checked, not only a copied manifest.
Current exactness/tolerances, expected values and approved floating-point decisions
are unchanged. Existing suites cover preprocessing, classic scoring/ranking/residual,
crystallography, Pattern/CIF/powder-CIF, SQLite, QXP/provenance, GUI, normal CLI and
import/export. The additional failure above limits CLI/error acceptance even though
all existing normal tests pass.

| Supported actual configuration | Full current normal identities | Acceptance evidence mode |
| --- | --- | --- |
| Windows x64 GCC 13.1.0 / Qt 6.8.3 | 80/80, zero failure/skip | `REUSED_UNCHANGED_DEPENDENCIES` |
| Windows x64 MSVC 19.40.33811 / Qt 6.8.3 | 80/80, zero failure/skip | `REUSED_UNCHANGED_DEPENDENCIES` |
| Ubuntu 26.04 x64 GCC 15.2.0 / Qt 6.10.2 | 80/80, zero failure/skip | `REUSED_UNCHANGED_DEPENDENCIES` |

The original full runs were `FRESH_VALIDATION` at `d2915b0b`. Raw clean-start
markers, configure/build outputs, test identities, compile commands and artifact
hashes were independently checked against the final candidate's dependencies.
Linux SDK OS, compiler/Qt versions, 1,775 actual source inputs and 70 binaries were
rechecked directly. MSVC stripped product observations remain supported by the
original 317-object/archive, 1,510,321-symbol-line origin scan; their inputs were
rehashed. Three previously clean-built complete Qt-free Application clients were
freshly executed and passed. Full three-platform suites were not rerun and are
not labeled fresh at `e6883eda`.

Compilation remains CXX only (302/299/301 units), with 70 Qt-free units per
configuration. Source/dependency, link/import and origin evidence shows no active
Fortran compilation, ABI/callback, owned state or runtime/provider. This is
**MILESTONE_F_PRESERVED**, not a rewrite or reapproval of historical Milestone F.
Hosted GitHub CI, Clang and macOS validation were not executed or claimed PASS.

## Clean checkout, packaging and history

A new detached checkout of **`e6883eda`**, not the earlier test checkout, has
1,935 tracked files. Fresh configure and actual CPack TGZ generation passed.
Every packaged file exactly matches the final candidate, including all 169
references. Real `.git`, Python bytecode/cache and generated-build positive
controls existed in the checkout and were absent from the package. Historical
evidence/runner directories and migration archives are absent. Inert reference
source text remains an approved regression asset, not an active Fortran build.
Package SHA-256: `c0e43ee07fb851d678fa66bd90e43c8aadb45c6502292c36625ecddd216ab680`;
37,137,914 bytes. Normal build/tests use current inputs and do not restore archives.
Original clean-checkout product builds/full suites retain the valid dependency
reuse described above. Unchanged installer/deployment inputs retain their bound
evidence; no new complete installer/provider matrix is claimed.

Fresh restoration fetched six real local archive objects, validating object and
catalog identities, lengths, member manifests and bytes: numerical reference,
raw FAIL, numerical approval, Phase31 migration record, Milestone F independent
acceptance and Phase36E closure. Three important protected Git receipts/reports
also restore with their original byte rules and SHA. Historical records, failures,
licenses/source/toolchain provenance and old SHA values were not changed.
Use the [recovery index](../../development/repository-consolidation/recovery-index.json)
and [migration history](../../migration-history.md).

**Residual availability risk:** protected tags, the Git bundle and actual evidence
objects are local backups. `D:/QualPF-evidence-backup/20261004` has no verified remote
or offsite recovery guarantee. This limits disaster recovery claims; it is not a
new Milestone A architecture blocker and does not invalidate successful local
restoration. Nine retained uncertain historical investigation records are not
newly reproduced current defects.

## Gate and stop

Layering, ownership, shared engine, worker/SQL lifetime, numerical references,
existing regression/build evidence, source packaging and historical recovery pass.
A-01 fails mandatory error/CLI acceptance. Overall **ACCEPTANCE_BLOCKED**; final
Milestone A acceptance remains pending. The earlier implementation receipt and
Phase37E readiness remain historical facts, not this final approval.

Current architectural separation supports later PFSM work, but the handoff is
withheld until the blocker is corrected and independently rechecked. PFSM remains
NOT_STARTED. This task ends after recording and locally committing the review;
it does not start the correction, Phase38 or PFSM.
