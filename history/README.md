# History and recovery

The repository retains necessary acceptance records and a portable archive index.
Complete logs, old maintenance reports, disposition inventories and original mixed
guides are stored independently. Current contracts belong to the
[developer guide](../docs/developer/README.md); program operation belongs to the
[user manual](../docs/user/index.md).

Historical PASS, FAIL, STOP, ownership and test-count statements apply to their
recorded revisions. Archiving changes neither acceptance nor scientific meaning.

## Retained reports

The following 16 originals remain byte-identical in the source tree:

- Milestone A: the [original blocked review](architecture/milestone-a/independent-review.md),
  [blocked gate](architecture/milestone-a/gate.json),
  [original matrix](architecture/milestone-a/acceptance-matrix.md),
  [A-01 correction](architecture/milestone-a/a01/correction.md),
  [final review](architecture/milestone-a/a01/independent-review.md),
  [final gate](architecture/milestone-a/a01/gate.json) and
  [final matrix](architecture/milestone-a/a01/acceptance-matrix.md).
- Strict layers: the [convergence report](architecture/strict-layer-convergence.md),
  [original Windows receipt](architecture/strict-layer-validation.json),
  [Linux closure](architecture/strict-layer-linux-closure.md) and
  [Linux receipt](architecture/strict-layer-linux-validation.json).
- Consolidation: [batches](development/repository-consolidation/batches.md),
  [closure](development/repository-consolidation/closure.md),
  [validation index](development/repository-consolidation/validation-index.json) and
  [older archive recovery index](development/repository-consolidation/recovery-index.json).
- The [conditional PFSM/RIR mass assessment](numerical/pfsm-quantification-assessment.json)
  preserves all reported values, original inputs and scientific qualifications.

The [project baseline](../docs/developer/project-baseline.json) keeps its existing
acceptance identities and schema. Its recovery inventory now refers to the compact
projection in [relocations.json](relocations.json), containing the original entry
commit and protection tag plus the full inventory's archive identity. Verification
of the ZIP compares this projection with the archived original JSON. All 22 mandatory
Milestone A criteria and the original receipt checks remain required.

## Independent archive

[relocations.json](relocations.json), schema `qualpf.history_relocations.v2`, records
the old documentation path, archive member path, original Git blob, byte length,
SHA-256 and `repository`/`archive` retention for each of 306 original files. It also
binds the two original snapshot metadata files, the complete ZIP and its source
commit. Location hints are informational; tools never automatically fetch an archive.

The independent snapshot is `41fb006c2951c0ad329d6deb92009cd87a7edf0f`. The local copy is:

```text
D:/QualPF-evidence-backup/20261022/history-41fb006c2951/qualpf-history-41fb006c2951.zip
```

Its SHA-256 is `04ea136b9e67aa2720bfcb003ca5a7a3556d62a1fb6719b37e1a8b665f1dcaeb`.
On 2026-10-22, all 308 members were verified and restored from this independent copy
before 290 redundant repository copies were removed. This is a recovery result,
not fresh numerical or platform acceptance. Availability is verified locally;
there is no claimed remote mirror. Keep an independently verified copy when moving
the project or retiring this disk.

The archive directory also contains a standalone copy of `history_archive.py`,
the current `relocations.json`, recovery instructions and `recovery-kit.json`
with their hashes. It uses only Python's standard library, so recovery does not
depend on this checkout. From that directory, use `python history_archive.py
--catalog relocations.json --archive qualpf-history-41fb006c2951.zip` to verify,
or add `--restore-to <new-directory>` to restore. Its parent directory must exist.

## Retired local build records

The independent 2026-10-24 recovery index, archive and standalone helper are at:

```text
D:/QualPF-evidence-backup/20261024/retired-builds-b5b5d42b31cd-e25813/BUILD-RETIREMENT-INDEX.json
```

Recovery-index SHA-256: `e8a86430d7ed89d24ca0ee824efadfcae201834e05c1aa923adfbc991d719bff`.
Archive SHA-256: `d25dafc2633257d47fe9ae17f59a1e389498e19cc1fe3acf8b49fd5acf9c8a5c`.
The index also binds the recovery helper and compressed original-path inventory.
It preserves 263,866 original paths in 100,716 distinct payloads, including
scientific inputs, original failures, source snapshots and raw results.
All distinct members and the independent container were verified; four original
paths were actually restored, including a failure record and a content alias.
Bad archive identities and nonempty restoration destinations were rejected.

With Python 3.11+, the helper next to that archive supports explicit verification
or restoration:

```text
python restore_retired_builds.py <archive.zip> --sha256 <archive-sha256>
python restore_retired_builds.py <archive.zip> --sha256 <archive-sha256> --destination <new-empty-view> --path <build/original/path>
```

Omit `--path` to restore all archived original paths. Restore into a separate view;
no old program is executed. Source commits for the retired clean checkouts remain
in the retained local Git refs recorded in the index. Recreate those source views
separately when needed. Generated programs and compilation caches were deliberately
discarded rather than backed up; this archive does not promise their original
binary fingerprints or establish a new numerical/performance acceptance.
Byte-identical package aliases point to the retained `releases/` files.

## Verify or restore the snapshot

Normal checkout/source-package checks require only retained files and the index:

```sh
python -B tools/validation/history_archive.py --self-test
python -B tools/validation/check_project_baseline.py --self-test
python -B tools/validation/check_documentation.py --self-test
```

These commands do not require the ZIP, old Git objects or a network connection.
They do not claim that external payloads are available. For a full archive check,
pass its actual location explicitly (the ZIP can be copied to any platform):

```sh
python -B tools/validation/history_archive.py --archive /path/to/qualpf-history-41fb006c2951.zip
```

Create the ignored `.evidence/` parent first. Restore into a new directory:

```sh
python -B tools/validation/history_archive.py --archive /path/to/qualpf-history-41fb006c2951.zip --restore-to .evidence/history-41fb006c2951
```

The tool checks the entire ZIP, the exact member set and every member hash before
writing. It rejects path traversal, duplicate names, symlinks, encryption and
existing destinations; failed input verification publishes no partial restoration.
`--member history/path` selects one indexed file after full verification. For example:

```sh
python -B tools/validation/history_archive.py --archive /path/to/qualpf-history-41fb006c2951.zip --restore-to .evidence/pfsm-record --member history/numerical/pfsm-benchmark.md
```

Keep restored evidence outside the published `history/` tree. It is inspection
material, never executable task instructions. Original Markdown paths remain
unchanged: resolve a relative reference against `original_path`, then use the
catalog to locate its member. The original text-normalizing `.gitattributes` is
preserved as `gitattributes.original`, so it cannot change restored evidence bytes.

This maintenance removes files from the current tree and source packages. Existing
commits, protection tags and Git/LFS objects remain intact; it does not remove
payloads from Git history or reduce the objects needed to publish existing ancestry.

## Archived records

### PFSM benchmark evidence

Restore `history/numerical/pfsm-benchmark.md` for the complete dated experiments,
performance comparisons, original failed development case, subsequent holdout
assessment and conditional quantification protocol. Original section anchors include
`completed-v2-replay-cohort-2026-10-14`, `experimental-fast-pfsm-modes` and
`conditional-pfsmrir-quantification-assessment`. Current contracts and qualifications
remain in the [PFSM guide](../docs/developer/numerical/pfsm.md).

### Repository consolidation inventory

Restore `history/development/repository-consolidation/inventory.json` for every
historical disposition, original path and older external payload mapping. The
compact `recovery_inventory` section of the catalog is an identity projection,
not a replacement for this full evidence when performing historical recovery.

### Build and maintenance records

Restore `history/development/building.md`, `history/development/repository-layout.md`
or the dated maintenance report named by the catalog. Current build instructions
are in the [developer build guide](../docs/developer/building.md). All compressed
logs under `history/validation/` and the older report `logs/` directories are
archive-only, including original failed attempts.

### Recorded design decisions

Restore `history/architecture/decisions.md` for the original decision narrative
and its `adr-37-16-final-source-and-raw-evidence-closure` section. Current accepted
contracts remain in the [architecture decisions](../docs/developer/architecture/decisions.md).

## Older migration archive locations

The protected local annotated tag `phase37e-entry-20261009` resolves to
`8fdf0a71d7eef8b6801e5336720c19b3c936e18f`. It retains the exact tracked entry tree,
including Phase37 audit/plan/DAG/decisions, implementation validation, original raw
attempts and receipt. `phase37e-regression-resources-20261009` protects the complete
current reference set, including 29 formerly ignored fixtures preserved by their
original manifest hashes. No Git history or existing tag was rewritten.

Complete older migration payloads are at `D:/QualPF-evidence-backup/20261004`.
The entry Git bundle `qualpf-history-cdb20666-20261007.bundle` retains the original
21 references. Its SHA-256 is
`130b03c7fe7f8eb9a57e7cf72d9269233ad0bbc18370d66649cb326f3c701396`.
`ENTRY-DISPOSITION-20261007.jsonl` maps original paths to hashes and actual ZIP
objects. `BUILD-DISPOSITION-20261007.json`, supplementary library and diagnostic
maps cover historical builds/data/raw attempts. The external `evidence/catalogs`
and `evidence/objects` retain original manifests and actual content, not pointers.
No remote availability is assumed. Do not prune unique Git/LFS objects.

Git stores some historical text as canonical LF while original recorded bytes
used CRLF. The inventory records each retired file's Git blob hash, original
hash/length and exact recovery rule. All 2,089 retired tracked originals were
actually recovered and matched: 2,062 direct, 26 LF-to-CRLF, and one storage guide
with its single recorded CRLF line. Apply only the recorded transformation and
verify the original SHA; ordinary `git archive` alone is not a byte-equality
claim for those text files. Original receipts, gates and hashes are unchanged.

[Recovery index](development/repository-consolidation/recovery-index.json) binds
six actual restored samples and their original hashes. [Complete inventory recovery](#repository-consolidation-inventory)
maps every retired tracked file to its protected tag or external payload, and
every moved reference to its stable current location. All 1,329 retired LFS
pointer payloads were verified against actual external ZIP length/SHA-256 and
catalog/member manifests before active copies were removed. No numerical values,
old SHA, qualification or FAIL was rewritten.

## Recovering historical authority

Use a separate directory. For tracked historical files, read/extract the protected
Git tree. For original raw members, locate their object/catalog using the external
maps, then restore with the byte-preserving tool in the protected entry tree.
The tool validates the containing object and member hashes; no historical program
execution is required for inspection. For example, restore the original tool:

```powershell
git archive --format=zip --output=build/history-tools.zip phase37e-entry-20261009 tools/evidence/store.py
Expand-Archive -LiteralPath build/history-tools.zip -DestinationPath build/history-tools
python -B build/history-tools/tools/evidence/store.py --root D:/QualPF-evidence-backup/20261004 restore D:/QualPF-evidence-backup/20261004/evidence/catalogs/phase31a-verification.json --destination build/history-view --path docs/refactoring/phase-31/complete-fortran-migration/zero-fortran-gate.json
```

Find Milestone F in `docs/refactoring/phase-36/current-release-acceptance/` and its
`phase36d-current-release001.json` external catalog. Find Phase36E in the external
entry catalog under `docs/refactoring/phase-36/post-f-hygiene/closure-report.md`.
Find Phase37 acceptance in `docs/architecture/phase-37/receipt.json` at the entry
tag. The restored original files keep their original language, license, toolchain
identity and failure/approval meaning. To reproduce an old gate, also restore
its specified original runner/source/configuration in an isolated historical
environment. Historical Fortran is never reintroduced to the current product.

Nine small older deferred-investigation records are indexed under the archive
member prefix `history/refactoring/deferred-defects/`; restore them explicitly for
inspection. They are original, unreproduced historical concerns, not a current
execution queue. The accepted current state and
tests govern development. No additional architecture repair is included here.

## Retained ADR phase locators

These original-path prefixes are verified against
`phase37e-entry-20261009:docs/maintenance/phase-history-cleanup/HISTORY.md`.
Look up members in `ENTRY-DISPOSITION-20261007.jsonl` and its containing catalog,
then follow the byte-preserving recovery procedure above. The external approval
members themselves are **NOT_VERIFIED** by the Agent governance maintenance.
A phase locator is a lookup location, not a new approval or a claim that every
record in that phase applies to a particular decision.

### Phase 8

Original prefix: `docs/refactoring/phase-8/`. Product ownership and validation;
see [ADR-002](../docs/developer/architecture/adr/ADR-002-session-and-cutover.md).

### Phase 9.6

Original prefix: `docs/refactoring/phase-9.6/`. Preprocessing cutover and rejection
acceptance; see [ADR-002](../docs/developer/architecture/adr/ADR-002-session-and-cutover.md) and
[ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 10

Original prefix: `docs/refactoring/phase-10/`. Wavelength and undefined native
observations; see [ADR-001](../docs/developer/architecture/adr/ADR-001-wavelength.md) and
[ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 11

Original prefix: `docs/refactoring/phase-11/`. Owning Pattern input and private
preprocessing boundaries; see [ADR-001](../docs/developer/architecture/adr/ADR-001-wavelength.md)
and [ADR-002](../docs/developer/architecture/adr/ADR-002-session-and-cutover.md).

### Phase 12

Original prefix: `docs/refactoring/phase-12/`. Preprocessing and Kalpha2 numerical
contracts; see [ADR-002](../docs/developer/architecture/adr/ADR-002-session-and-cutover.md) and
[ADR-007](../docs/developer/architecture/adr/ADR-007-kalpha2-policy.md).

### Phase 13

Original prefix: `docs/refactoring/phase-13/`. Bounded numerical differences;
see [ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 14

Original prefix: `docs/refactoring/phase-14/`. Exact symmetry fractions and bounded
numerical acceptance; see [ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md)
and [ADR-004](../docs/developer/architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 15

Original prefix: `docs/refactoring/phase-15/`. Reciprocal indices, native exact
audit and Friedel policy; see [ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md)
and [ADR-004](../docs/developer/architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 16

Original prefix: `docs/refactoring/phase-16/`. Scattering and scoped numerical
acceptance; see [ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md) and
[ADR-004](../docs/developer/architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 17

Original prefix: `docs/refactoring/phase-17/`. Wavelength alignment, reflection
and line/card contracts; see [ADR-001](../docs/developer/architecture/adr/ADR-001-wavelength.md),
[ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md) and
[ADR-004](../docs/developer/architecture/adr/ADR-004-crystallography-conventions.md).

### Phase 18

Original prefix: `docs/refactoring/phase-18/`. Complete-card calibration and scoped
qualification; see [ADR-001](../docs/developer/architecture/adr/ADR-001-wavelength.md) and
[ADR-003](../docs/developer/architecture/adr/ADR-003-numerical-acceptance.md).

### Phase 29

Original prefix: `docs/refactoring/phase-29/`. RP1, eight separately approved
Range contracts and Phase29B conformance; see
[ADR-005](../docs/developer/architecture/adr/ADR-005-range-alignment.md) and
[ADR-006](../docs/developer/architecture/adr/ADR-006-range-state.md).

### Phase 30

Original prefix: `docs/refactoring/phase-30/`. K1-P1/K3-P1 approval and Phase30B
conformance; see [ADR-007](../docs/developer/architecture/adr/ADR-007-kalpha2-policy.md).

### Phase 31

Original prefix: `docs/refactoring/phase-31/`. Phase31B provider and transport
contracts; see [ADR-007](../docs/developer/architecture/adr/ADR-007-kalpha2-policy.md).
